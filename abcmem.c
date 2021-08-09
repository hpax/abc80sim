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
 * The ABC80 memory map is controlled by OUT 7 (64K) or 7+31 (MEG80)
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

	case TKN80_CAT:
	    /* CAT80 might be linear, and may not support BASIC? */
	    addr[1] = 0x4800 + ((row & 7) << 8) + (80 * (row >> 3));
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
 * ABC80 memory augmentations: MEG80 and 64K
 */
static void abc80_mem_setmap(unsigned int map)
{
    abc80_map = map;
    current_map[0] = current_map[1] = memmaps[abc80_map];
    if (tracing(TRACE_MAP)) {
	if (opts.meg80) {
	    int map = abc80_map - 1;
	    if (map < 0)
		fprintf(tracef, "MAP: selecting map system\n");
	    else
		fprintf(tracef, "MAP: selecting map %d (in %s)\n",
			map, (map & 16) ? "flash" : "sram");
	} else if (opts.kb == 64) {
	    fprintf(tracef, "MAP: selecting map %d\n", abc80_map);
	}
    }
}

void abc80_64k_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;
    abc80_mem_setmap(val & 3);
}

void abc80_meg80_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;

    /* XXX: Add NMI/INT and reset control */
    abc80_mem_setmap((val & 31) + 1);
}

/*
 * MEG80 control interface
 */
static inline bool meg80_addr_is_memmap(size_t xaddr)
{
    return !(xaddr & ~(size_t)0x101fe7);
}

static uint8_t meg80_mask[4];
enum meg80_ic3 {
    IC3_RAM,			/* IC3 contains MEG80 */
    IC3_FLASH,			/* IC3 contains programmable flash */
    IC3_ROM			/* IC3 contains write-protected flash */
};
static enum meg80_ic3 meg80_ic3;

#if PAGE_SHIFT != 9
# error "Need to change write_meg80() to deal with PAGE_SHIFT != 9"
#endif
/*
 * This can be called internally to sync the memory maps, or
 * externally if and only if this is actually a low 8K actual MEG80 page.
 * It can be called internally even if this memory address is not
 * actually writable!
 */
static void write_flash(uint8_t *p, uint8_t v);
static void write_meg80(uint8_t *p, uint8_t ppage)
{
    size_t xaddr = p - xmem;

    if (!meg80_mask[xaddr >> 19])
	ppage = 255;		/* Unpopulated slot, can only write FF */

    *p = ppage;

    if (meg80_addr_is_memmap(xaddr)) {
	unsigned int map, vpage, pslot;
	size_t pageaddr;
	struct mem_page *page;

	pslot = ppage >> 6;
	ppage &= meg80_mask[pslot];

	/* Map 0 reflects the system map, so add 1 */
	map = ((xaddr & 0x1e0) >> 5) + ((xaddr & (2 << 19)) >> 16) + 1;
	vpage = ((xaddr & 0x1e00) + ((xaddr & 7) << 13)) >> PAGE_SHIFT;
	pageaddr = ((size_t)ppage << 13) + (xaddr & 0x1e00);
	page = &memmaps[map][vpage];

	if (tracing(TRACE_MAP)) {
	    static const char slotname[4][4] = { "IC1", "IC2", "IC3", "sys" };
	    fprintf(tracef, "MAP: IC%u 0x%04x vpage %2d:%u:%2u (0x%04x) -> %s 0x%05x (%2u:%2u)\n",
		    (((unsigned int)xaddr >> 19) + 1),
		    ((unsigned int)xaddr & 0x7ffff),
		    (int)map-1, vpage >> 4, vpage & 15, vpage << 9,
		    slotname[pslot], ((unsigned int)pageaddr & 0x7ffff),
		    ppage & 63, vpage & 15);
	}

	if (pslot == 3) {
	    /* System map; map 0 is the preserved initial system map */
	    *page = memmaps[0][pageaddr >> PAGE_SHIFT];
	} else if (!meg80_mask[pslot]) {
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
		page->write = write_meg80;
	    }

	    if (pslot == 2) {	/* IC3 populated differently? */
		switch (meg80_ic3) {
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
 * Sync MEG80 mappings with system map
 */
static void meg80_sync_mappings(void)
{
    uint8_t *p;
    unsigned int i, j, k;
    enum tracing old_traceflags = traceflags;

    if (!opts.meg80)
	return;

    if (tracing(TRACE_MAP)) {
	fprintf(tracef, "MAP: synchronizing MEG80 maps\n");
	traceflags &= ~TRACE_MAP;
    }

    /* IC1, IC3; subpage; page */
    for (i = 0; i <= (1 << 20); i += (1 << 20)) {
	for (j = 0; j < 8192; j += 32) {
	    for (k = 0; k < 8; k++) {
		p = xmem + i + j + k;
		write_meg80(p, *p);
	    }
	}
    }

    traceflags = old_traceflags;
}

/*
 * This only implements some flash commands, and those that it does
 * are implemented as "infinitely fast." However, it should be enough
 * to test most software. Notably missing is the ID command.
 * The current simulator memory model doesn't support read side effects
 * for memory, so doing everything correctly would need to add that.
 */

/*
 * In "software ID mode", the first two bytes of flash contents are
 * replaced with vendor ID and product ID, respectively.
 */
static bool meg80_ic3_flash_id_active;	  /* Software ID mode active */
static uint8_t meg80_ic3_flash_id[2];	  /* The flash ID for this chip */
static uint8_t meg80_ic3_flash_id_save[2]; /* Saved real contents */

static inline void flash_resume_flash_id(void)
{
    uint8_t * const fl = xmem + K(1024);
    if (likely(!meg80_ic3_flash_id_active))
	return;

    meg80_ic3_flash_id_save[0] = fl[0];
    meg80_ic3_flash_id_save[1] = fl[1];
    write_meg80(&fl[0], meg80_ic3_flash_id[0]);
    write_meg80(&fl[1], meg80_ic3_flash_id[1]);
}

static void flash_exit_flash_id(void)
{
    uint8_t * const fl = xmem + K(1024);
    if (!meg80_ic3_flash_id_active)
	return;

    /* At this point the flash array should contain "true" values */
    meg80_ic3_flash_id_active = false;
    write_meg80(&fl[0], fl[0]);
    write_meg80(&fl[1], fl[1]);

    if (tracing(TRACE_FLASH)) {
	fprintf(tracef, "FLASH: sw_id: exit  %02X %02X\n",
		fl[0], fl[1]);
    }
}

static inline bool is_cmd(size_t faddr, unsigned int cmdmask)
{
    if (likely((faddr & 0x7fff) != cmdmask))
	return false;

    faddr >>= 15;
    return !faddr || (faddr == (meg80_mask[2] & 0x3c) >> 2);
}

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
    unsigned int faddr = xaddr & 0x7ffff;
    uint8_t op, np;

    if (unlikely(meg80_ic3_flash_id_active)) {
	/* Restore true contents */
	uint8_t * const fl = xmem + K(1024);
	fl[0] = meg80_ic3_flash_id_save[0];
	fl[1] = meg80_ic3_flash_id_save[1];

	if (state == FL_NORM && v == 0xf0) {
	    flash_exit_flash_id();
	    return;
	}
    }

    switch (state) {
    case FL_PROG:
	/* Progamming can only change 1 bits to 0 */
	op = *p;
	np = v & op;
	*p = np;
	if (tracing(TRACE_FLASH)) {
	    fprintf(tracef, "FLASH: write: %05X - %02X : %02X -> %02X%s\n",
		    faddr, v, op, np, (v != np) ? " (!)" : "");
	}
	if (faddr < 8192)
	    write_meg80(p, v);	/* Update memory mappings */
	state = FL_NORM;
	break;

    case FL_CP5:
	if (v == 0x30) {
	    /* Sector erase */
	    uint8_t *s = p - (xaddr & 0xfff);
	    if (tracing(TRACE_FLASH)) {
		fprintf(tracef, "FLASH: erase: %05X ... %05X (sector)\n",
			faddr & ~0xfff, faddr | 0xfff);
	    }
	    memset(s, 0xff, 4096);
	    if (faddr < 8192)
		meg80_sync_mappings();
	} else if (v == 0x10 && is_cmd(faddr, 0x5555)) {
	    /* Chip erase */
	    if (tracing(TRACE_FLASH)) {
		fprintf(tracef, "FLASH: erase: 00000 ... %05X (chip)\n",
			((meg80_mask[2] & 63) << 13) | 0x1fff);
	    }
	    memset(xmem+K(1024), 0xff, K(512));
	    meg80_sync_mappings();
	}
	state = FL_NORM;
	break;

    default:
	if (is_cmd(faddr, 0x5555)) {
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
		switch (v) {
		case 0xa0:
		    state = FL_PROG;
		    break;
		case 0x80:
		    state = FL_CP3;
		    break;
		case 0x90:
		    /*
		     * If the vendor ID is zero, we don't have a
		     * software ID mode - probably < 128K is an EEPROM
		     * anyway, which did not have this feature it seems.
		     */
		    if (!meg80_ic3_flash_id_active && meg80_ic3_flash_id[0]) {
			if (tracing(TRACE_FLASH)) {
			    fprintf(tracef, "FLASH: sw_id: enter %02X %02X\n",
				    meg80_ic3_flash_id[0],
				    meg80_ic3_flash_id[1]);
			}
			meg80_ic3_flash_id_active = true;
		    }
		    state = FL_NORM;
		    break;
		case 0xf0:
		    state = FL_NORM;
		    flash_exit_flash_id();
		    return;
		default:
		    state = FL_NORM;
		    break;
		}
		break;
	    case FL_CP5:
		state = FL_NORM;
		break;
	    default:
		state = FL_NORM;
		break;
	    }
	} else if (is_cmd(faddr, 0x2aaa)) {
	    if (v == 0x55 && (state == FL_CP1 || state == FL_CP4))
		state++;
	    else
		state = FL_NORM;
	} else {
	    state = FL_NORM;
	}
	break;
    }

    flash_resume_flash_id();
}

static inline uint8_t *io_to_meg80(uint16_t addr)
{
    size_t xaddr = addr;

    /* 3 is an alias for 0, except it writes the control register */
    if ((xaddr & 3) == 3)
	xaddr &= ~3;

    if (!meg80_mask[xaddr & 3])
	return NULL;		/* Not present */

    return &xmem[(xaddr & 0x1fe8) + (xaddr >> 13) + ((xaddr & 3) << 19)];
}

uint8_t abc80_meg80_in(uint16_t addr)
{
    const uint8_t *p = io_to_meg80(addr);
    return p ? *p : 0xff;
}

void abc80_meg80_out(uint16_t addr, uint8_t val)
{
    uint8_t *p = io_to_meg80(addr);

    if (p)			/* If p == NULL then socket empty */
	write_meg80(p, val);

    if ((addr & 3) == 3)
	abc80_meg80_control_out(addr, val);
}

/*
 * Initialize MEG80 SRAM/flash card if present
 */
static int init_meg80(void)
{
    unsigned int kb[3];
    uint8_t *meg80;
    int bootmap;
    int i;

    /* Defaults */
    kb[0] = kb[1] = kb[2] = 512; /* 3x512K */
    meg80_ic3 = IC3_FLASH;	 /* IC3 is flash */
    bootmap = 0;		 /* System boot */

    if (opts.meg80_config) {
	const char *srp = opts.meg80_config;
	const char *esrp;
	int ikb = 0;
	unsigned long nkb;

	while (*srp) {
	    bool err = false;

	    nkb = strtoul(srp, (char **)&esrp, 0);
	    if (esrp != srp && (!*esrp || *esrp == ',')) {
		if (ikb < 3) {
		    kb[ikb++] = nkb;
		    /* Configurations < 128K are theoretical only */
		    if ((nkb & (nkb-1)) || nkb < 8 || nkb > 512)
			err = true;
		} else {
		    err = true;
		}
	    } else {
		size_t olen;
		esrp = strchr(srp, ',');
		if (!esrp)
		    esrp = strchr(srp, '\0');
		olen = esrp - srp;

		if (olen == 0) {
		    if (ikb < 3)
			kb[ikb++] = 0;
		    else
			err = true;
		} else if (isstr("flash", srp, olen) ||
			   isstr("boot", srp, olen)) {
		    bootmap = 17; /* Flash boot */
		} else if (isstr("system", srp, olen) ||
			   isstr("noboot", srp, olen)) {
		    bootmap = 0; /* System boot */
		} else if (isstr("we", srp, olen)) {
		    meg80_ic3 = IC3_FLASH;
		} else if (isstr("rom", srp, olen) ||
			   isstr("wp", srp, olen)) {
		    meg80_ic3 = IC3_ROM;
		} else if (isstr("ram", srp, olen) ||
			   isstr("meg80", srp, olen)) {
		    meg80_ic3 = IC3_RAM;
		} else {
		    err = true;
		}
	    }

	    srp = esrp;
	    switch (*srp) {
	    case ',':
		srp++;
		break;
	    case '\0':
		break;
	    default:
		err = true;
		break;
	    }

	    if (err) {
		fprintf(stderr, "%s: invalid MEG80 configuration: %s\n",
			program_name, opts.meg80_config);
		return -1;
	    }
	}
    }

    for (i = 0; i < 3; i++) {
	meg80_mask[i] = 0;
	if (!kb[i])
	    continue;
	meg80_mask[i] = ((kb[i]-1) >> 3) | 0xc0;
    }

    meg80_mask[3] = 0x07;	/* System memory */

    /*
     * Flash software identification ID.
     * Vendor ID = BF (SST)
     * Device ID = B5 (128K)
     * Device ID = B6 (256K)
     * Device ID = B7 (512K)
     *
     * Smaller devices are theoretical, but in practice would probably
     * have to be EEPROM devices which don't support the software ID
     * feature, so if the device ID is zero the flash write code
     * will ignore a software ID mode command.
     */
    switch (kb[2]) {
    case 128:
	meg80_ic3_flash_id[0] = 0xbf;
	meg80_ic3_flash_id[1] = 0xb5;
	break;
    case 256:
	meg80_ic3_flash_id[0] = 0xbf;
	meg80_ic3_flash_id[1] = 0xb6;
	break;
    case 512:
	meg80_ic3_flash_id[0] = 0xbf;
	meg80_ic3_flash_id[1] = 0xb7;
	break;
    default:
	meg80_ic3_flash_id[0] = 0;
	meg80_ic3_flash_id[1] = 0;
	break;
    }

    /*
     * Flash and unpopulated slots want to be filled with FF.
     * MEG80 can be initialized to anything... FF is as good as
     * anything, no?
     */
    meg80 = malloc(3*K(512));
    if (!meg80)
	return -1;

    xmem = meg80;
    memset(meg80, 0xff, 3*K(512));

    sysload_add_memspace("xmem", NULL, NULL, meg80_sync_mappings,
			 meg80, -1, K(1536));

    /*
     * XXX: would be nice to cap sram better, but if IC1 is < 512K]
     * then the address space is noncontiguous. Either way, it is not a
     * normal configuration.
     */
    if (meg80_ic3 == IC3_RAM) {
	sysload_add_memspace("sram", NULL, NULL, meg80_sync_mappings,
			     meg80, -1, K(1536));
    } else {
	sysload_add_memspace("sram", NULL, NULL, meg80_sync_mappings,
			     meg80, -1, K(1024));
	sysload_add_memspace("flash", NULL, NULL, meg80_sync_mappings,
			     meg80+K(1024), -1, kb[2] << 10);
    }

    return bootmap;
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

    sysload_add_memspace("vram", NULL, NULL, NULL,
			 video_ram + K(2) - vram, vram - 1, vram);

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
	sysload_add_memspace("fgram", NULL, NULL, NULL, fgram, -1, K(16));
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
    sysload_add_memspace("ram", NULL, NULL, NULL, ram, -1, sizeof ram);
    sysload_add_memspace("cpu", load_cpu, dump_cpu, NULL,
			 NULL, -1, Z80_ADDRESS_LIMIT);
    sysload_add_memspace("rom", NULL, NULL, NULL, rom, -1, sizeof rom);

    /* Unused ROM contains 0xff */
    memset(rom, 0xff, sizeof rom);

    /* Start by initializing all memory maps to all RAM */
    map_memory(ALL_MAPS, 0, K(64), ram, write_ram);

    switch (opts.model) {
    case MODEL_ABC80:
    {
	int init_map = 0;
	const uint8_t *dos = ufddos80;
	const uint8_t *pr  = print80_30;
	size_t prlen  = K(1);
	size_t praddr = K(30);

	if (flags & MEMFL_NOBASIC)
	    opts.basic = BASIC_NONE;

	if (opts.meg80) {
	    int meg80_status = init_meg80();
	    if (meg80_status < 0) {
		opts.meg80 = false;
	    } else {
		if (opts.kb == 64)
		    opts.kb = 16;
		init_map = meg80_status;
	    }
	}
	if (opts.kb != 64 && (opts.kb < 1 || opts.kb > 32)) {
	    unsigned int k = opts.meg80 ? 16 : 64;
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
	 * This also applies to 64K users with *any* TKN80, since the
	 * standard is that the VRAM is moved to 30-32K in that
	 * case... assume a user with such a modded machine will have
	 * modded this too.
	 */
	if (opts.tkn80 == TKN80_GEJO ||
	    (opts.tkn80 != TKN80_NONE && opts.kb == 64)) {
	    pr = print80_29;
	    praddr = K(29);
	}

	switch (opts.basic) {
	case BASIC_NONE:
	    break;
	case BASIC_10042:
	    memcpy(rom, abc80new, K(16));
	    rom[0x3843] = 0x81;	/* Only byte that differs!! */
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

	/*
	 * Hack: map 20-22K as writable. This seems to be consistent
	 * with (some?) UFD-DOS modifications putting a 2K external
	 * RAM at this address. Map it still out of the ROM space as
	 * it would be different from the RAM space on 64K.
	 */
	map_memory(0x01, K(20), K(2), &rom[K(20)], write_ram);

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
	case TKN80_CAT:
	    map_memory(0x01, K(18), K(2), &video_ram[K(0)], write_screen);
	    break;
	}
	/* Standard 40-char video RAM */
	map_memory(0x01, K(31), K(1), &video_ram[K(1)], write_screen);

	if (opts.tkn80 == TKN80_NONE)
	    sysload_add_memspace("vram", NULL, NULL, NULL,
				 &video_ram[K(1)], K(1)-1, K(2));
	else
	    sysload_add_memspace("vram", NULL, NULL, NULL,
				 video_ram, K(2)-1, K(2));

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

	meg80_sync_mappings();
        abc80_mem_setmap(init_map);
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

	sysload_add_memspace("mem", NULL, NULL, NULL, ram, -1, K(32));

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
