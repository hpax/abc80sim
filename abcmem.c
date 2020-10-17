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
static uint8_t *xmem;

#define write_ram	NULL    /* Optimized fast path */
#define write_screen	write_ram

#define PAGE_SHIFT	9
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_COUNT	(Z80_ADDRESS_LIMIT/PAGE_SIZE)

/* Up to 32 memory maps */
#define MEM_MAPS 33

typedef void (*write_func)(uint8_t *p, uint8_t v);
struct mem_page {
    uint8_t *data;
    write_func write;
};
static struct mem_page memmaps[MEM_MAPS][PAGE_COUNT];

static void write_rom(uint8_t * p, uint8_t v)
{
    /* Do nothing */
    (void)p;
    (void)v;
}
static uint8_t empty_page_data[PAGE_SIZE];
static const struct mem_page empty_page = { empty_page_data, write_rom };

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
 * The ABC80 memory map is controlled by OUT 7 (64K) or 7+31 (SRAM)
 */
static unsigned int abc80_map;


/* 48/80 char ROM patches, *excluding* the line table at address 884 */
struct patch_location {
    uint16_t address;
    uint8_t  rom[2];		/* 40, then 80 */
};

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

/* Adjust BASIC ROM to match the TKN80 mode */
void abc80_mem_mode80(bool mode80)
{
    size_t i, row;
    const struct patch_location *pl;

    if (opts.tkn80 == TKN80_NONE || !opts.basic)
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

/*
 * Open or close the MEM: area on ABC802, FGRAM on ABC800C/M
 */
void abc800_set_mem(bool opened)
{
    size_t offset = (size_t)opened << 1;
    current_map[0] = memmaps[offset + 0];
    current_map[1] = memmaps[offset + 1];
}

#define ALL_MAPS ((UINT64_C(1) << MEM_MAPS)-1)

static void map_memory(uint64_t maps, size_t where, size_t size,
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
 * ABC80 memory augmentations: SRAM and 64K
 */
static void abc80_mem_setmap(unsigned int map)
{
    abc80_map = map;
    current_map[0] = current_map[1] = memmaps[abc80_map];
}

void abc80_64k_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;
    abc80_mem_setmap(val & 3);
}

void abc80_sram_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;

    /* XXX: Add NMI/INT and reset control */
    abc80_mem_setmap((val & 31) + 1);
}

/*
 * SRAM control interface
 */
static inline bool sram_addr_is_memmap(size_t xaddr)
{
    return !(xaddr & ~(size_t)0x101fe7);
}

static uint8_t sram_mask[4];
enum sram_ic3 {
    IC3_RAM,			/* IC3 contains SRAM */
    IC3_FLASH,			/* IC3 contains programmable flash */
    IC3_ROM			/* IC3 contains write-protected flash */
};
static enum sram_ic3 sram_ic3;

#if PAGE_SHIFT != 9
# error "Need to change write_sram() to deal with PAGE_SHIFT != 9"
#endif
/*
 * This can be called internally to sync the memory maps, or
 * externally if and only if this is actually a low 8K actual SRAM page.
 * It can be called internally even if this memory address is not
 * actually writable!
 */
static void write_flash(uint8_t *p, uint8_t v);
static void write_sram(uint8_t *p, uint8_t ppage)
{
    size_t xaddr = p - xmem;

    if (!sram_mask[xaddr >> 19])
	ppage = 255;		/* Unpopulated slot, can only write FF */

    *p = ppage;

    if (sram_addr_is_memmap(xaddr)) {
	unsigned int map, vpage, pslot;
	size_t pageaddr;
	struct mem_page *page;

	pslot = ppage >> 6;
	ppage &= sram_mask[pslot];

	map = ((xaddr & 0x01e0) >> 5) + 1; /* Map 0 is system */
	vpage = ((xaddr & 0x1e00) + ((xaddr & 7) << 13)) >> PAGE_SHIFT;
	pageaddr = ((size_t)ppage << 13) + (xaddr & 0x1e00);
	page = &memmaps[map][vpage];

	if (pslot == 3) {
	    /* System map; map 0 is the preserved initial system map */
	    *page = memmaps[0][pageaddr >> PAGE_SHIFT];
	} else if (!sram_mask[pslot]) {
	    /* Unpopulated slot, treat as ROM containing FF */
	    *page = empty_page;
	} else {
	    page->data = xmem + pageaddr;
	    page->write = write_ram;
	    if ((ppage & 0x7f) == 0) {
		/*
		 * Bottom 8K in IC1 or IC3; this is a memory map,
		 * so we need to redirect a write back here
		 */
		page->write = write_sram;
	    }

	    if (pslot == 2) {	/* IC3 populated differently? */
		switch (sram_ic3) {
		case IC3_RAM:
		    break;	/* Same as all other slots */
		case IC3_ROM:
		    page->write = write_rom;
		    break;
		case IC3_FLASH:
		    page->write = write_flash;
		    break;
		}
	    }
	}
    }
}

/*
 * Sync SRAM mappings with system map
 */
static void sram_sync_mappings(void)
{
    uint8_t *p;
    unsigned int i, j;

    if (!opts.sram)
	return;

    for (i = 0; i < 2; i++) {
	p = xmem + (i << 20);	/* IC1, IC3 */
	for (j = 0; j < 8192; j++) {
	    write_sram(p, *p);
	    p++;
	}
    }
}

/*
 * This only implements some flash commands, and those that it does
 * are implemented as "infinitely fast." However, it should be enough
 * to test most software. Notably missing is the ID command.
 * The current simulator memory model doesn't support read side effects
 * for memory, so doing everything correctly would need to add that.
 */
static void write_flash(uint8_t *p, uint8_t v)
{
    /*
     * The command sequences supported are:
     * AA 55 A0 xx       = byte write
     * AA 55 80 AA 55 30 = sector erase
     * AA 55 80 AA 55 10 = chip erase
     */
    enum flash_state {
	FL_NORM,	/* Normal operation */
	FL_CP1,		/* AA */
	FL_CP2,		/* AA 55 */
	FL_CP3,		/* AA 55 80 */
	FL_CP4,		/* AA 55 80 AA */
	FL_CP5,		/* AA 55 80 AA 55 */
	FL_PROG		/* AA 55 A0 */
    };
    static enum flash_state state = FL_NORM;
    size_t xaddr = p - xmem;
    const size_t cmdlo = 2 << 19;
    size_t cmdhi = cmdlo + ((sram_mask[2] & 0x3c) << 13);

    switch (state) {
    case FL_PROG:
	/* Progamming can only change 1 bits to 0 */
	v &= *p;
	*p = v;
	state = FL_NORM;
	if (xaddr < K(1024)+8192)
	    write_sram(p, v);	/* Update memory mappings */
	return;
    case FL_CP5:
	if (v == 0x30) {
	    /* Sector erase */
	    uint8_t *s = p - (xaddr & 0xfff);
	    memset(s, 0xff, 4096);
	    state = FL_NORM;
	    if (xaddr < K(1024)+8192)
		sram_sync_mappings();
	    return;
	}
	break;
    default:
	break;
    }

    if (xaddr == cmdlo + 0x5555 || xaddr == cmdhi + 0x5555) {
	/* CMD1 address write */
	switch (state) {
	case FL_NORM:
	case FL_CP3:
	    if (v == 0xaa)
		state++;
	    else
		state = FL_NORM;
	    break;
	case FL_CP2:
	    if (v == 0xa0)
		state = FL_PROG;
	    else if (v == 0x80)
		state = FL_CP3;
	    else
		state = FL_NORM;
	    break;
	case FL_CP5:
	    state = FL_NORM;
	    if (v == 0x10) {
		/* Chip erase */
		memset(xmem+K(1024), 0xff, K(512));
		sram_sync_mappings();
	    }
	    break;
	default:
	    state = FL_NORM;
	}
    } else if (xaddr == cmdlo + 0x2aaa || xaddr == cmdhi + 0x2aaa) {
	if (v == 0x55 && (state == FL_CP1 || state == FL_CP4))
	    state++;
	else
	    state = FL_NORM;
    } else {
	state = FL_NORM;
    }
}

static inline uint8_t *io_to_sram(uint16_t addr)
{
    size_t xaddr = addr;

    /* 3 is an alias for 0, except it writes the control register */
    if ((xaddr & 3) == 3)
	xaddr &= ~3;

    if (!sram_mask[xaddr & 3])
	return NULL;		/* Not present */

    return &xmem[(xaddr & 0x1fe8) + (xaddr >> 13) + ((xaddr & 3) << 19)];
}

uint8_t abc80_sram_in(uint16_t addr)
{
    const uint8_t *p = io_to_sram(addr);
    return p ? *p : 0xff;
}

void abc80_sram_out(uint16_t addr, uint8_t val)
{
    uint8_t *p = io_to_sram(addr);

    if (p)			/* If p == NULL then socket empty */
	write_sram(p, val);

    if ((addr & 3) == 3)
	abc80_sram_control_out(addr, val);
}

/*
 * Initialize SRAM/flash card if present
 */
static uint8_t *init_sram(void)
{
    static const char default_config[] = "512,512,512,flash";
    unsigned int kb[3];
    char ic3[6];
    uint8_t *sram;
    int i;

    sscanf(default_config, "%u,%u,%u,%5s", &kb[0], &kb[1], &kb[2], ic3);
    if (opts.sram_config)
	sscanf(opts.sram_config, "%u,%u,%u,%5s", &kb[0], &kb[1], &kb[2], ic3);

    for (i = 0; i < 3; i++) {
	sram_mask[i] = 0;
	if (!kb[i])
	    continue;
	/* Configurations < 128K are of theoretical interest only */
	if ((kb[i] & (kb[i]-1)) || kb[i] < 8 || kb[i] > 512) {
	    fprintf(stderr, "%s: invalid SRAM configuration: %s\n",
		    program_name, opts.sram_config);
	    return NULL;
	}
	sram_mask[i] = ((kb[i]-1) >> 3) | 0xc0;
    }

    sram_mask[3] = 0x07;	/* System memory */

    sram_ic3 = IC3_FLASH;	/* Default */

    if (!strcmp(ic3, "flash") || !strcmp(ic3, "we")) {
	sram_ic3 = IC3_FLASH;
    } else if (!strcmp(ic3, "rom") || !strcmp(ic3, "wp")) {
	sram_ic3 = IC3_ROM;
    } else if (!strcmp(ic3, "ram") || !strcmp(ic3, "sram")) {
	sram_ic3 = IC3_RAM;
    }

    /*
     * Flash and unpopulated slots want to be filled with FF.
     * SRAM can be initialized to anything... FF is as good as
     * anything, no?
     */
    sram = malloc(3*K(512));
    if (!sram)
	return NULL;

    xmem = sram;
    memset(sram, 0xff, 3*K(512));

    sysload_add_memspace("xmem", NULL, NULL, sram, -1, K(1536));

    /* XXX: would be nice to cap sram better */
    if (sram_ic3 == IC3_RAM) {
	sysload_add_memspace("sram", NULL, NULL, sram, -1, K(1536));
    } else {
	sysload_add_memspace("sram", NULL, NULL, sram, -1, K(1024));
	sysload_add_memspace("flash", NULL, NULL, sram+K(1024), -1,
			     kb[2] << 10);
    }

    return sram;
}

/* Common memory initialization for all ABC800 models */
static void mem_init_abc800(unsigned int flags, const uint8_t *master_rom,
			    unsigned int vram)
{
    if (!(flags & MEMFL_NOBASIC))
	memcpy(rom, master_rom, K(24));
    if (!(flags & MEMFL_NODOS))
	memcpy(rom+K(24), master_rom+K(24), K(4));
    if (!(flags & MEMFL_NOPR))
	memcpy(rom+K(28), master_rom+K(28), K(4));

    sysload_add_memspace("vram", NULL, NULL, video_ram + K(2) - vram,
			 vram - 1, vram);

    /*
     * Map 0: normal execution
     * Map 1: execution in option ROM - all ROM except for model specific
     * Map 2: extended RAM (FGRAM, MEM...) mapped in
     * Map 3: extended RAM (FGRAM, MEM...) mapped in, running in option ROM
     */
    map_memory(0x0f, 0, K(32), rom, write_rom);
    map_memory(0x05, K(32) - vram, vram, video_ram + K(2) - vram, write_screen);

    abc800_set_mem(false);	/* Normal memory mode */
}

/* Common memory initialization for ABC800C/M */
static void mem_init_abc800cm(unsigned int flags, const uint8_t *master_rom,
			      unsigned int vram)
{
    mem_init_abc800(flags, master_rom, vram);

    /*
     * Map 1: execution in option ROM - HR RAM open, but
     * 16-32K is ROM
     */
    if (opts.hr) {
	sysload_add_memspace("fgram", NULL, NULL, fgram, -1, K(16));
	map_memory(0x0e, K(0), K(16), fgram, write_screen);
    }
}

/*
 * Set up memory maps.  Note: dump_memory() currently relies on
 * map 7 being all RAM, regardless of if there is an actual
 * map 7 or not.  If this isn't reliable, change this to have a
 * map set up specifically for Alt-u dumps.
 */
void mem_init(unsigned int flags, const char *memfile)
{
    /* Initialize empty page */
    memset(empty_page_data, 0xff, sizeof empty_page_data);

    /* Register common sysload memory spaces */
    sysload_add_memspace("ram", NULL, NULL, ram, -1, sizeof ram);
    sysload_add_memspace("cpu", load_cpu, dump_cpu, NULL, -1,
			 Z80_ADDRESS_LIMIT);
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

	if (flags & MEMFL_NOBASIC)
	    opts.basic = BASIC_NONE;

	if (opts.sram) {
	    if (!init_sram()) {
		opts.sram = false;
	    } else {
		if (opts.kb > 32)
		    opts.kb = 32;
	    }
	}
	if (opts.kb != 64 && (opts.kb < 1 || opts.kb > 32)) {
	    unsigned int k = opts.sram ? 16 : 64;
            fprintf(stderr, "%s: invalid ABC80 memory size %uK, using %uK\n",
                    program_name, opts.kb, k);
            opts.kb = k;
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
	 *
	 * This also applies to SRAM users and 64K users with *any*
	 * TKN80, since the standard is that the VRAM is moved to
	 * 30-32K in that case... assume a user with such a modded
	 * machine will have modded this too.
	 */
	if (opts.tkn80 == TKN80_GEJO ||
	    (opts.tkn80 != TKN80_NONE && (opts.sram || opts.kb == 64))) {
	    pr = print80_29;
	    praddr = K(29);
	}

	switch (opts.basic) {
	case BASIC_NONE:
	    break;
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

        abc80_mem_setmap(0);
	sram_sync_mappings();
        break;
    }

    case MODEL_ABC800C:
	mem_init_abc800cm(flags, abc800crom, K(1));
        break;

    case MODEL_ABC800M:
	mem_init_abc800cm(flags, abc800mrom, K(2));
	break;

    case MODEL_ABC802:
	mem_init_abc800(flags, abc802rom, K(2));

	sysload_add_memspace("mem", NULL, NULL, ram, -1, K(32));

        /* Map 1: execution in option ROM - RAM other than the ROM itself */
        map_memory(0x02, K(0), K(30), ram, write_ram);

        /* Map 2-3: MEM area open in its entirety, so all RAM */
	map_memory(0x0c, K(0), K(64), ram, write_ram);

	/* If we have a MEM: file from the command line, load it */
	load_memfile(memfile);
        break;

    case MODEL_ABC806:
	break;			/* Not implemented yet */
    }
}
