#include "compiler.h"
#include "screen.h"
#include "z80.h"
#include "abcio.h"
#include "rom.h"
#include "hostfile.h"
#include "abcfile.h"
#include "sysload.h"

#define K(x) ((x)*1024)

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

uint8_t ram[MEMORY_SIZE];
static uint8_t rom[MEMORY_SIZE];
static uint8_t rom80[K(16)];	/* BASIC rom for 80 characters on ABC80 */

#define write_ram	NULL    /* Optimized fast path */
#define write_screen	write_ram

#define PAGE_SHIFT	10
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_COUNT	(Z80_ADDRESS_LIMIT/PAGE_SIZE)

/* Up to 8 memory maps */
#define MEM_MAPS 8

typedef void (*write_func)(uint8_t *p, uint8_t v);
struct mem_page {
    uint8_t *data;
    write_func write;
};
static struct mem_page memmaps[MEM_MAPS][PAGE_COUNT];

/* Latch the last M1 address fetched, like ABC800 does */
static uint16_t last_m1_address;

/*
 * Currently active memory map(s)
 *
 * ABC800 can have two memory maps: the second kicks in when executing code
 * in the range 0x7800-0x7fff
 */
static const struct mem_page *current_map[2];

static inline const struct mem_page *get_page(uint16_t addr)
{
    size_t map = (last_m1_address & 0xf800) == 0x7800;
    return &current_map[map][addr >> PAGE_SHIFT];
}

/*
 * Memory tracing support
 */
struct mem_trace {
    uint16_t addr, data;
    uint8_t size;
    bool written;
};

#define MAX_TRACES 16
static struct mem_trace mem_traces[MAX_TRACES + 1];
static struct mem_trace *mem_trace_tail = mem_traces;

static inline void
mem_trace_record(uint16_t addr, uint16_t data, uint8_t size, bool written)
{
    if (!tracing(TRACE_CPU))
        return;

    if (mem_trace_tail <= &mem_traces[MAX_TRACES]) {
        mem_trace_tail->addr = addr;
        mem_trace_tail->data = data;
        mem_trace_tail->size = size;
        mem_trace_tail->written = written;
        mem_trace_tail++;
    }
}

/* Write memory trace data to screen */
void tracemem(void)
{
    const struct mem_trace *mtp;
    bool overflow = false;
    uint16_t last_addr = 0;
    int last_written = -1;

    if (!tracing(TRACE_CPU))
        return;

    if (mem_trace_tail >= &mem_traces[MAX_TRACES]) {
        mem_trace_tail--;
        overflow = true;
    }

    for (mtp = mem_traces; mtp < mem_trace_tail; mtp++) {
        fputc(' ', tracef);
        if (mtp->addr != last_addr || mtp->written != last_written)
            fprintf(tracef, "(%04X)%c", mtp->addr, mtp->written ? '=' : ':');
        fprintf(tracef, "%0*X", mtp->size * 2, mtp->data);
        last_addr = mtp->addr + mtp->size;
        last_written = mtp->written;
    }

    if (overflow)
        fputs(" ...", tracef);

    mem_trace_tail = mem_traces;
}

static inline uint8_t do_mem_read(uint16_t address)
{
    return get_page(address)->data[address & PAGE_MASK];
}

uint8_t mem_read(uint16_t address)
{
    uint8_t value = do_mem_read(address);

    mem_trace_record(address, value, 1, false);
    return value;
}

uint8_t mem_fetch(uint16_t address)
{
    /* Don't trace instruction fetches */
    return do_mem_read(address);
}

/* This is called when fetching the first opcode byte, corresponding to M1# */
uint8_t mem_fetch_m1(uint16_t address)
{
    /* Don't trace instruction fetches */
    last_m1_address = address;
    return do_mem_read(address);
}

/*
 * Words are stored with the low-order byte in the lower address.
 */
static inline uint16_t do_mem_read_word(uint16_t address)
{
    uint8_t b0, b1;

    b0 = do_mem_read(address);
    b1 = do_mem_read(address + 1);

    return (b1 << 8) + b0;
}

uint16_t mem_read_word(uint16_t address)
{
    uint16_t value = do_mem_read_word(address);
    mem_trace_record(address, value, 2, false);
    return value;
}

uint16_t mem_fetch_word(uint16_t address)
{
    /* Don't trace instruction fetches */
    return do_mem_read_word(address);
}

/*
 * Simple write operations
 */
static void write_rom(uint8_t * p, uint8_t v)
{
    /* Do nothing */
    (void)p;
    (void)v;
}

static void do_mem_write(uint16_t address, uint8_t value)
{
    const struct mem_page *page;
    uint8_t *p;

    page = get_page(address);
    p = &page->data[address & PAGE_MASK];
    if (likely(!page->write))
        *p = value;
    else
        page->write(p, value);
}

void mem_write(uint16_t address, uint8_t value)
{
    mem_trace_record(address, value, 1, true);
    do_mem_write(address, value);
}

void mem_write_word(uint16_t address, uint16_t value)
{
    mem_trace_record(address, value, 2, true);
    do_mem_write(address, value);
    do_mem_write(address + 1, value >> 8);
}

/*
 * The ABC80 memory map can be altered either by flipping the
 * video mode or by doing out 7 (if enabled.)
 */
static unsigned int abc80_map;

void abc80_mem_mode40(bool mode40)
{
    abc80_map = (abc80_map & ~1) | mode40;
    current_map[0] = current_map[1] = memmaps[abc80_map];
}

void abc80_mem_setmap(unsigned int map)
{
    if (opts.kb < 64)
        return;                 /* Only 64K models can remap memory */

    abc80_map = ((map & 3) << 1) | (abc80_map & ~6);
    current_map[0] = current_map[1] = memmaps[abc80_map];
}

/*
 * Open or close the MEM: area on ABC802
 */
void abc802_set_mem(bool opened)
{
    current_map[0] = memmaps[opened ? 2 : 0];
    current_map[1] = memmaps[opened ? 2 : 1];
}

#define ALL_MAPS ((1U << MEM_MAPS)-1)

static void
map_memory(unsigned int maps, size_t where, size_t size,
           void *what, write_func wfunc)
{
    size_t m;

    assert(((where | size) & PAGE_MASK) == 0);
    assert((maps & ~ALL_MAPS) == 0);

    for (m = 0; maps; m++, maps >>= 1) {
        struct mem_page *mp;
        uint8_t *datap;
        size_t npg;

        if (!(maps & 1))
            continue;

        mp = &memmaps[m][where >> PAGE_SHIFT];
        datap = what;

        npg = size >> PAGE_SHIFT;

        while (npg--) {
            mp->data = datap;
            mp->write = wfunc;
            datap += PAGE_SIZE;
            mp++;
        }
    }
}

/*
 * Load a binary file into low (< 30K) RAM, in the format used by
 * the ABC802 MEM: device.
 */
static void load_memfile(const char *memfile)
{
    struct host_file *hf;
    uint8_t *rp;
    unsigned int blk;
    struct abcdata abc;
    const unsigned int max_blocks = (K(30) >> 8) - 1;

    if (!memfile)
        return;

    rp = ram;

    hf = open_host_file(HF_BINARY, NULL, memfile, O_RDONLY);
    if (!hf)
        goto exit;
    if (!map_file(hf, 0))
        goto exit;

    init_abcdata(&abc, hf->map, hf->flen);
    blk = 0;
    while (blk < max_blocks) {
        bool done;
        rp[0] = 0x53;
        rp[1] = 0;
        rp[2] = blk++;
        done = get_abc_block(rp + 3, &abc);
        rp += 256;
        if (done)
            break;
    }

exit:
    memset(rp, 0, 3);           /* Avoid possible stray magic */
    close_file(&hf);
}

/*
 * Special handling of various sysload memspaces
 */
static void load_rom_4080(const struct load_data *ws, uint32_t addr,
			 uint8_t val)
{
    if (addr < sizeof rom80)
	rom80[addr] = val;

    ((uint8_t *)(ws->buf))[addr] = val;
}

static void load_rom_80(const struct load_data *ws, uint32_t addr,
			uint8_t val)
{
    if (addr < sizeof rom80)
	rom80[addr] = val;
    else
	((uint8_t *)(ws->buf))[addr] = val;
}

/*
 * This writes to the CPU view of memory, but does not trigger any
 * MMIO actions, nor does it enforce write protect of ROM areas.
 */
static void load_sys(const struct load_data *ws, uint32_t addr,
		     uint8_t val)
{
    const struct mem_page *page;
    (void)ws;

    page = get_page(addr);
    page->data[addr & PAGE_MASK] = val;
}

/*
 * Set up memory maps.  Note: dump_memory() currently relies on
 * map 7 being all RAM, regardless of if there is an actual
 * map 7 or not.  If this isn't reliable, change this to have a
 * map set up specifically for Alt-u dumps.
 */
void mem_init(unsigned int flags, const char *memfile)
{
    /* Register common sysload memory spaces */
    sysload_add_memspace("ram", NULL, ram, -1, sizeof ram);
    sysload_add_memspace("cpu", load_sys, NULL, -1, Z80_ADDRESS_LIMIT);

    /* Unused ROM contains 0xff */
    memset(rom, 0xff, sizeof rom);
    memset(rom80, 0xff, sizeof rom80);

    /* Start by initializing all memory maps to all RAM */
    map_memory(ALL_MAPS, 0, K(64), ram, write_ram);

    switch (opts.model) {
    case MODEL_ABC80:
        /* 4 maps * 2 (40/80) */

	sysload_add_memspace("rom", load_rom_4080, rom, -1, sizeof rom);
	sysload_add_memspace("rom40", NULL, rom, -1, sizeof rom);
	sysload_add_memspace("rom80", load_rom_80, rom, -1, sizeof rom);

        if ((opts.kb < 1 || opts.kb > 32) && opts.kb != 64) {
            fprintf(stderr, "%s: invalid ABC80 memory size %uK, using 64K\n",
                    program_name, opts.kb);
            opts.kb = 64;
        }

        /* Map 0: default (for < 64K, the only available map) */

        if (!(flags & MEMFL_NOBASIC)) {
	    int i;

	    memcpy(rom,   opts.old_basic ? abc80bas40o : abc80bas40n, K(16));
	    memcpy(rom80, opts.old_basic ? abc80bas80o : abc80bas80n, K(16));

	    /*
	     * The 80-character BASIC ROMs have screen row addresses
	     * relative to the start of VRAM, since those addresses
	     * vary. Fix them up here.
	     */
	    switch (opts.tkn80) {
	    case TKN80_MYAB:
		for (i = 885; i < 885+2*24; i += 2)
		    rom80[i] += 0x58;
		break;

	    case TKN80_GEJO:
		for (i = 885; i < 885+2*24; i += 2)
		    rom80[i] += 0x78;
		break;

	    case TKN80_29K:
		for (i = 885; i < 885+2*24; i += 2)
		    rom80[i] += (rom80[i] & 4) + 0x74;
		break;

	    default:
		break;
	    }
        }

	map_memory(0x03, 0, K(32), rom, write_rom);
	if (opts.tkn80 != TKN80_NONE)
	    map_memory(0x01, 0, K(16), rom80, write_rom);

        if (!(flags & MEMFL_NODOS))
	    memcpy(rom+K(24), abc80_devs, K(4));
	if (!(flags & MEMFL_NOPR))
	    memcpy(rom+K(28), abc80_devs, K(4));

	/* Hack: allow printer ROMs to be written to */
	map_memory(0x03, K(28), K(4), &rom[K(28)], write_ram);

	/*
	 * Note: leave 80-character VRAM always mapped, there is no
	 * evidence that any of them unmapped the extra video RAM
	 * (why would they?)
	 */
	switch (opts.tkn80) {
	case TKN80_NONE:
	    /* Nothing to map */
	    break;
	case TKN80_GEJO:
	    map_memory(0x03, K(30), K(1), &video_ram[K(0)], write_screen);
	    break;
	case TKN80_MYAB:
	    map_memory(0x03, K(22), K(2), &video_ram[K(0)], write_screen);
	    break;
	case TKN80_29K:
	    map_memory(0x03, K(29), K(1), &video_ram[K(0)], write_screen);
	    break;
	}
	/* Standard 40-char video RAM */
	map_memory(0x03, K(31), K(1), &video_ram[K(1)], write_screen);

	if (opts.tkn80 == TKN80_NONE)
	    sysload_add_memspace("vram", NULL, &video_ram[K(1)], K(1)-1, K(2));
	else
	    sysload_add_memspace("vram", NULL, video_ram, -1, K(2));

	/*
	 * ABC80 memory grows from the top down. Memory between 32K and
	 * the start of RAM is unmapped. Map it to ROM, which normally
	 * will be uninitialized here.
	 */
        if (opts.kb < 32)
	    map_memory(0x03, K(32), K(32 - opts.kb), &rom[K(32)], write_rom);

        /* Map 1: RAM over ROM areas. Video RAM always at 30K for TKN80. */
        /* Map 2: video RAM at the end */
	if (opts.tkn80 == TKN80_NONE) {
	    map_memory(0x0c, K(31), K(1), &video_ram[K(1)], write_screen);
	    map_memory(0x30, K(63), K(1), &video_ram[K(1)], write_screen);
	} else {
	    map_memory(0x0c, K(30), K(2), &video_ram[K(0)], write_screen);
	    map_memory(0x30, K(62), K(2), &video_ram[K(0)], write_screen);
	}

        /* Map 3: all RAM */
	/* (nothing to do) */

	/* Default to map 0 */
        abc80_mem_setmap(0);
        break;

    case MODEL_ABC802:
	sysload_add_memspace("rom", NULL, rom, -1, sizeof rom);
	sysload_add_memspace("vram", NULL, video_ram, -1, sizeof video_ram);

        /* Map 0: normal execution */

        if (!(flags & MEMFL_NOBASIC))
	    memcpy(rom, abc802rom, K(24));
	if (!(flags & MEMFL_NODOS))
	    memcpy(rom+K(24), &abc802rom[K(24)], K(4));
	if (!(flags & MEMFL_NOPR))
	    memcpy(rom+K(28), &abc802rom[K(28)], K(4));

	map_memory(0x01, 0, K(30), abc802rom, write_rom);
        map_memory(0x01, K(30), K(2), video_ram, write_screen);

        /* Map 1: execution in option ROM - RAM other than the ROM itself */
        map_memory(0x02, K(30), K(2), &abc802rom[K(30)], write_rom);

        /* Map 2: MEM area open in its entirety */

        abc802_set_mem(false);  /* On start, MEM area closed */
        break;
    }

    load_memfile(memfile);
}

/*
 * Dump memory to a file
 */
const char *memdump_path;

void dump_memory(bool ramonly)
{
    const struct mem_page *map = ramonly ? memmaps[7] : current_map[0];
    struct host_file *hf;
    size_t i;

    hf = dump_file(HF_BINARY, memdump_path,
                   ramonly ? "ram%04u.bin" : "mem%04u.bin");
    if (!hf)
        return;

    for (i = 0; i < PAGE_COUNT; i++)
        fwrite(map[i].data, 1, PAGE_SIZE, hf->f);

    if (!ferror(hf->f))
        keep_file(hf);          /* It's good */

    close_file(&hf);
}
