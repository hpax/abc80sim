#include "compiler.h"
#include "screen.h"
#include "z80.h"
#include "debug.h"
#include "abcio.h"
#include "rom.h"
#include "hostfile.h"
#include "abcfile.h"
#include "sysload.h"

#define K(x) ((x)*1024)

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

uint8_t ram[MEMORY_SIZE];
static uint8_t rom[MEMORY_SIZE];

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

static inline bool check_bit(const uint8_t *map, uint16_t bit)
{
    return (map[bit >> 3] >> (bit & 7)) & 1;
}
static inline bool check_2bit(const uint8_t *map, uint16_t bit)
{
    return check_bit(map, bit) | check_bit(map, bit+1);
}

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
    check_watchpoint_byte(address, Z80_RDWPT);
    return value;
}

/* Don't trace instruction fetches; code breakpoints handled elsewhere */

uint8_t mem_fetch(uint16_t address)
{
    return do_mem_read(address);
}

/* This is called when fetching the first opcode byte, corresponding to M1# */
uint8_t mem_fetch_m1(uint16_t address)
{
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
    check_watchpoint_word(address, Z80_RDWPT);
    return value;
}

uint16_t mem_fetch_word(uint16_t address)
{
    /* Don't trace or breakpoint instruction fetches */
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
    check_watchpoint_byte(address, Z80_WRWPT);
    do_mem_write(address, value);
}

void mem_write_word(uint16_t address, uint16_t value)
{
    mem_trace_record(address, value, 2, true);
    check_watchpoint_word(address, Z80_WRWPT);
    do_mem_write(address, value);
    do_mem_write(address + 1, value >> 8);
}

/*
 * The ABC80 memory map can be altered either by flipping the
 * video mode or by doing out 7 (if enabled.)
 */
static unsigned int abc80_map;

struct patch_location {
    uint16_t address;
    uint8_t  rom[2];		/* 40, then 80 */
};

/* 48/80 char ROM patches, *excluding* the line table at address 884 */
static const struct patch_location rompatch80[] = {
    {  472, {  40,  80 } },
    {  529, {  40,  80 } },
    {  590, {  40,  80 } },
    {  623, {  39,  79 } },
    {  734, {  40,  80 } },
    {  828, {  40,  80 } },
    { 8946, {  80, 160 } },	/* New BASIC */
    { 8948, {  80, 160 } }	/* Old BASIC */
};

void abc80_mem_mode80(bool mode80)
{
    size_t i, row;
    const struct patch_location *pl;

    if (opts.tkn80 == TKN80_NONE)
	return;

    /* If this doesn't look like ABC80-BASIC, don't patch it... */
    if (memcmp(&rom[1836], "\xbc\r\nABC80", 8))
	return;

    pl = rompatch80;
    for (i = 0; i < ARRAY_SIZE(rompatch80); i++) {
	if (rom[pl->address] == pl->rom[!mode80])
	    rom[pl->address] = pl->rom[mode80];
	pl++;
    }

    /*
     * The 80-character BASIC ROMs have screen row addresses
     * relative to the start of VRAM, since those addresses
     * vary. Fix them up here.
     */
    for (row = 0; row < 24; row++) {
	uint8_t *ptr = &rom[884 + (row << 1)];
	uint16_t addr[2];

	/* 40 characters */
	addr[0] = 0x7c00 + ((row & 7) << 7) + (40 * (row >> 3));

	switch (opts.tkn80) {
	default:
	    addr[1] = addr[0];		/* No TKN80 */
	    break;

	case TKN80_MYAB:
	    addr[1] = 0x5800 + ((row & 7) << 8) + (80 * (row >> 3));
	    break;

	case TKN80_GEJO:
	    addr[1] = 0x7800 + ((row & 7) << 8) + (80 * (row >> 3));
	    break;

	case TKN80_29K:
	    addr[1] = 0x7400 + ((row & 4) << 16) + ((row & 3) << 8)
		+ (80 * (row >> 3));
	    break;
	}

	if (ptr[0] == (uint8_t)addr[!mode80] &&
	    ptr[1] == (uint8_t)(addr[!mode80] >> 8)) {
	    ptr[0] = addr[mode80];
	    ptr[1] = addr[mode80] >> 8;
	}

	ptr += 2;
    }
}

void abc80_mem_setmap(unsigned int map)
{
    if (opts.kb < 64)
	map = 0;		/* No extended memory? It's all map 0... */

    abc80_map = map & 3;	/* Until SRAM card is supported... */

    /* ABC80 doesn't contain execution-address sensitive memory map hardware */
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
 * This writes to the CPU view of memory, but does not trigger any
 * MMIO actions, nor does it enforce write protect of ROM areas.
 */
static void load_cpu(void *buf, uint32_t addr, uint8_t val)
{
    const struct mem_page *page;
    (void)buf;

    page = get_page(addr);
    page->data[addr & PAGE_MASK] = val;
}

/*
 * This reads a chunk from the CPU view of memory. It returns
 * a pointer to a chunk of data starting at the requested address.
 */
static struct dump_data dump_cpu(void *buf, uint32_t addr)
{
    const struct mem_page *page;
    struct dump_data dd;
    (void)buf;

    page = get_page(addr);
    addr &= PAGE_MASK;		/* Address within page */
    dd.data = &page->data[addr];
    dd.len = PAGE_SIZE - addr;
    return dd;
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
    sysload_add_memspace("ram", NULL, NULL, ram, -1, sizeof ram);
    sysload_add_memspace("cpu", load_cpu, dump_cpu, NULL, -1, Z80_ADDRESS_LIMIT);
    sysload_add_memspace("rom", NULL, NULL, rom, -1, sizeof rom);

    /* Unused ROM contains 0xff */
    memset(rom, 0xff, sizeof rom);

    /* Start by initializing all memory maps to all RAM */
    map_memory(ALL_MAPS, 0, K(64), ram, write_ram);

    switch (opts.model) {
    case MODEL_ABC80:
    {
	const uint8_t *dos = ufddos80;
	const uint8_t *pr  = print80_30;
	size_t prlen  = K(1);
	size_t praddr = K(30);

        /* 4 maps (for now... ) */

        if ((opts.kb < 1 || opts.kb > 32) && opts.kb != 64) {
            fprintf(stderr, "%s: invalid ABC80 memory size %uK, using 64K\n",
                    program_name, opts.kb);
            opts.kb = 64;
        }

	if (opts.basic == BASIC_II) {
	    if (opts.tkn80 != TKN80_NONE)
		opts.tkn80 = TKN80_GEJO; /* Always 30-32K */
	}

        /* Map 0: default (for < 64K, the only available map) */

	/*
	 * For GeJo TKN80 we need to map the printer ROM at a different
	 * address, which means using a printer ROM with the appropriate
	 * ORG.
	 */
	if (opts.tkn80 == TKN80_GEJO) {
	    pr = print80_29;
	    praddr = K(29);
	}

        if (!(flags & MEMFL_NOBASIC)) {
	    switch (opts.basic) {
	    case BASIC_NEW:
	    default:		/* ??? */
		memcpy(rom, abc80new,  K(16));
		break;
	    case BASIC_OLD:
		memcpy(rom, abc80old,  K(16));
		break;
	    case BASIC_II:
		memcpy(rom, basicii80, K(24));
		dos = basicii80 + K(24);
		pr  = basicii80 + K(28);
		prlen  = K(4);
		praddr = K(28);
		break;

	    }
	}

	map_memory(0x01, 0, K(32), rom, write_rom);

	if (!(flags & MEMFL_NODOS))
	    memcpy(rom+K(24), dos, K(4));
	if (!(flags & MEMFL_NOPR))
	    memcpy(rom+praddr, pr, prlen);

	/* Hack: allow device ROMs to be written to */
	map_memory(0x01, K(28), K(4), &rom[K(28)], write_ram);

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
	    map_memory(0x01, K(30), K(1), &video_ram[K(0)], write_screen);
	    break;
	case TKN80_MYAB:
	    map_memory(0x01, K(22), K(2), &video_ram[K(0)], write_screen);
	    break;
	case TKN80_29K:
	    map_memory(0x01, K(29), K(1), &video_ram[K(0)], write_screen);
	    break;
	}
	/* Standard 40-char video RAM */
	map_memory(0x01, K(31), K(1), &video_ram[K(1)], write_screen);

	if (opts.tkn80 == TKN80_NONE)
	    sysload_add_memspace("vram", NULL, NULL, &video_ram[K(1)], K(1)-1, K(2));
	else
	    sysload_add_memspace("vram", NULL, NULL, video_ram, K(2)-1, K(2));

	/*
	 * ABC80 memory grows from the top down. Memory between 32K and
	 * the start of RAM is unmapped. Map it to ROM, which normally
	 * will be uninitialized here.
	 */
        if (opts.kb < 32)
	    map_memory(0x01, K(32), K(32 - opts.kb), &rom[K(32)], write_rom);

	/*
	 * Adjust ROM for TKN80 if applicable
	 */
	abc80_mem_mode80(!opts.startup_width40);

        /* Map 1: RAM over ROM areas. Video RAM always at 30K for TKN80. */
        /* Map 2: video RAM at the end */
	if (opts.tkn80 == TKN80_NONE) {
	    map_memory(0x02, K(31), K(1), &video_ram[K(1)], write_screen);
	    map_memory(0x04, K(63), K(1), &video_ram[K(1)], write_screen);
	} else {
	    map_memory(0x02, K(30), K(2), &video_ram[K(0)], write_screen);
	    map_memory(0x04, K(62), K(2), &video_ram[K(0)], write_screen);
	}

        /* Map 3: all RAM */
	/* (nothing to do) */

	/* Default to map 0 */
        abc80_mem_setmap(0);
        break;
    }

    case MODEL_ABC802:
	sysload_add_memspace("vram", NULL, NULL, video_ram, -1, K(2));

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
