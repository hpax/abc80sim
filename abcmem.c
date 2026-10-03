#include "compiler.h"
#include "screen.h"
#include "z80.h"
#include "debug.h"
#include "abcio.h"
#include "hostfile.h"
#include "abcfile.h"
#include "as.h"
#include "sysload.h"
#include "roms/roms.h"

/*
 * Common address spaces
 */
static struct as *sys_as;     /* System map (minus MEG80/fgram/MEM) */
static struct as *rom_as;     /* Primary ROM */
static struct as *ram_as;     /* Primary RAM */
static struct as *nvram_as;   /* Small external SRAM */
static struct as *vram_as;    /* Video (text) RAM */
static struct as *fgram_as;   /* High resolution RAM (ABC800) */
static struct as *xmem_as;    /* Memory space for dumping extended RAM */
static struct as *mem_as;     /* MEM: (ABC802) - alias to part of RAM */

#define K(x) ((x)*1024)
#define R(x) { x, sizeof x }
struct rom {
    const unsigned char *data;
    size_t len;
};

#define copyrom(d,r) memcpy((d), r, sizeof r)

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
static bool abc80_mem_mode80_p;

static void abc80_mem_setup_mode80(bool mode80);

void abc80_mem_mode80(bool mode80)
{
    if (mode80 == abc80_mem_mode80_p)
	return;

    abc80_mem_mode80_p = mode80;
    abc80_mem_setup_mode80(mode80);
}

static void abc80_mem_setup_mode80(bool mode80)
{
    size_t i, row;
    const struct patch_location *pl;
    uint8_t *rom;

    if (opts.tkn80 == TKN80_NONE || !opts.basic)
	return;

    if (unlikely(!rom_as))
	return;			/* ROM not configured yet */

    rom = rom_as->p.data;

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
 * The ABC800 system address space has three maps: the normal one, the
 * one when executing in the option ROM, and one when the extended
 * memory area (fgram/mem:) is "open". Otherwise this is identical to
 * page_as_translate(), and we treat is otherwise as a 3-map page
 * table.
 */
static bool abc800_mem_open;

static struct asoffs abc800_sys_translate(struct asoffs vso)
{
    struct asoffs pso;

    if (abc800_mem_open)
	vso.offs += 2 << 16;
    else if ((last_m1_address & 0xf800) == 0x7800)
	vso.offs += 1 << 16;

    pso = vso.as->p.page[vso.offs >> vso.as->grain];
    pso.offs += vso.offs & grain_mask(vso.as->grain);

    return pso;
}

void abc800_set_mem(bool opened)
{
    abc800_mem_open = opened;
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

    rp = ram_as->p.data;

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
 * ABC80 paging handling via I/O ports (64K, MEG80)
 */
void abc80_64k_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;

    as_set_map(sys_as, val & 3);
}

/*
 * MEG80 memory spaces
 */
static struct as *meg80v_as;	 /* Virtual address space */
static struct as *sram_as;	 /* Address space for all of SRAM */
static struct as *flash_as;	 /* Address space alias for flash */
static struct as *meg80p_as;	 /* All MEG80 physical address space */
static uint8_t *xmem;		 /* Actual memory buffer for MEG80 */

static inline void meg80_set_map(int map)
{
    if (map < 0) {
	as_point_alias(cpu_as, sys_as, 0);
    } else {
	as_set_map(meg80v_as, map);
	as_point_alias(cpu_as, meg80v_as, 0);
    }
}

void abc80_meg80_control_out(uint16_t addr, uint8_t val)
{
    (void)addr;

    meg80_set_map(val & 31);
}

static struct asoffs meg80_translate(struct asoffs aso)
{
    size_t ppoffs;

    ppoffs = (aso.offs & ((1 << 20)|(15 << 9))) +
	((aso.offs >> 13) & 7);

    aso.offs = (aso.offs & 0x1fff) + ((size_t)xmem[ppoffs] << 13);
    aso.as = meg80p_as;
    return aso;
}

/*
 * This only implements some flash commands, and those that it does
 * are implemented as "infinitely fast." However, it should be enough
 * to test most software. Notably missing is the ID command.
 * The current simulator memory model doesn't support read side effects
 * for memory, so doing everything correctly would need to add that.
 *
 * XXX: this should be made into a generic feature not limited to MEG80.
 */

/*
 * In "software ID mode", the first two bytes of flash contents are
 * replaced with vendor ID and product ID, respectively.
 */
static bool meg80_ic3_flash_id_active;	  /* Software ID mode active */
static regpair meg80_ic3_flash_id;	  /* The flash ID for this chip */
static uint16_t meg80_ic3_flash_id_save;  /* Saved real contents */

static inline void flash_resume_flash_id(void)
{
    uint16_t * const fl = (uint16_t *)&xmem[K(1024)];
    if (likely(!meg80_ic3_flash_id_active))
	return;

    meg80_ic3_flash_id_save = *fl;
    *fl = meg80_ic3_flash_id.w;
}

static void flash_exit_flash_id(void)
{
    uint8_t * const fl = xmem + K(1024);
    if (!meg80_ic3_flash_id_active)
	return;

    /* At this point the flash array should contain "true" values */
    meg80_ic3_flash_id_active = false;

    if (tracing(TRACE_FLASH)) {
	fprintf(tracef, "FLASH: sw_id: exit  %02X %02X\n",
		fl[0], fl[1]);
    }
}

static inline bool is_cmd(size_t faddr, unsigned int grain,
			  unsigned int cmdmask)
{
    int32_t saddr;

    if (likely((faddr & 0x7fff) != cmdmask))
	return false;

    saddr = faddr;
    saddr = saddr << (31-grain) >> 15;

    return saddr == 0 || saddr == -1;
}

static void flash_as_write(struct as *as, size_t faddr, uint8_t v);

static const struct as_ops meg80_flash_ops = {
    .read = NULL,
    .write = flash_as_write,
    .dump = mem_as_dump,
    .load = NULL,
    .sync = NULL,
    .init = 0xff
};

static void flash_as_write(struct as *as, size_t faddr, uint8_t v)
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

    if (unlikely(meg80_ic3_flash_id_active)) {
	/* Restore true contents */
	uint16_t * const fl = (uint16_t *)as->p.data;
	*fl = meg80_ic3_flash_id_save;

	if (state == FL_NORM && v == 0xf0) {
	    flash_exit_flash_id();
	    return;
	}
    }

    switch (state) {
    case FL_PROG:
    {
	/* Progamming can only change 1 bits to 0 */
	uint8_t * const p = as->p.data + faddr;
	uint8_t op, np;

	op = *p;
	np = v & op;
	*p = np;
	if (tracing(TRACE_FLASH)) {
	    fprintf(tracef, "FLASH: write: %05zX - %02X : %02X -> %02X%s\n",
		    faddr, v, op, np, (v != np) ? " (!)" : "");
	}
	state = FL_NORM;
	break;
    }

    case FL_CP5:
	if (v == 0x30) {
	    /* Sector erase */
	    faddr &= ~0xfff;
	    uint8_t * const s = as->p.data + faddr;
	    if (tracing(TRACE_FLASH)) {
		fprintf(tracef, "FLASH: erase: %05zX ... %05zX (sector)\n",
			faddr, faddr + 4095);
	    }
	    memset(s, 0xff, 4096);
	} else if (v == 0x10 && is_cmd(faddr, as->grain, 0x5555)) {
	    /* Chip erase */
	    if (tracing(TRACE_FLASH)) {
		fprintf(tracef, "FLASH: erase: 00000 ... %05zX (chip)\n",
			as->len - 1);
	    }
	    memset(as->p.data, 0xff, as->len);
	}
	state = FL_NORM;
	break;

    default:
	if (is_cmd(faddr, as->grain, 0x5555)) {
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
		    if (!meg80_ic3_flash_id_active && meg80_ic3_flash_id.w) {
			if (tracing(TRACE_FLASH)) {
			    fprintf(tracef, "FLASH: sw_id: enter %02X %02X\n",
				    meg80_ic3_flash_id.b.l,
				    meg80_ic3_flash_id.b.h);
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
	} else if (is_cmd(faddr, as->grain, 0x2aaa)) {
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

/*
 * Note: the I/O layer will already have masked out the MEG80 control
 * bits A[4], A[2], and the case A[1:0] == 11. Therefore io_to_meg80()
 * doesn't have to take either into account.
 */
static inline size_t io_to_meg80(uint16_t addr)
{
    size_t xaddr = addr;	/* Make sure of correct promotion */
    return (xaddr & 0x1fe8) + (xaddr >> 13) + ((xaddr & 3) << 19);
}

uint8_t abc80_meg80_in(uint16_t addr)
{
    return do_as_read(meg80p_as, io_to_meg80(addr));
}

void abc80_meg80_rw_out(uint16_t addr, uint8_t val)
{
    do_as_write(meg80p_as, io_to_meg80(addr), val);
}

void abc80_meg80_rwctl_out(uint16_t addr, uint8_t val)
{
    abc80_meg80_rw_out(addr, val);
    abc80_meg80_control_out(addr, val);
}

/*
 * Initialize MEG80 SRAM/flash card if present
 */
static int init_meg80(void)
{
    const struct as_ops *ic_ops[3];
    unsigned int kb[3];
    struct as *as[4];
    int bootmap;
    int i;
    const char *srp, *esrp;
    int ikb = 0;
    unsigned long nkb;

    /* Defaults */
    kb[0] = kb[1] = kb[2] = 512; /* 3x512K */
    ic_ops[0] = ic_ops[1] = &ram_as_ops;
    ic_ops[2] = &meg80_flash_ops; /* IC3 is flash */
    bootmap = opts.bootmap;	  /* Defaults to -1 = system boot */

    srp = opts.meg80_config;
    if (!srp)
	srp = "";

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
		bootmap = 16; /* Flash boot */
	    } else if (isstr("system", srp, olen) ||
		       isstr("noboot", srp, olen)) {
		bootmap = -1; /* System boot */
	    } else if (isstr("we", srp, olen)) {
		ic_ops[2] = &meg80_flash_ops;
	    } else if (isstr("rom", srp, olen) ||
		       isstr("wp", srp, olen)) {
		ic_ops[2] = &rom_as_ops;
	    } else if (isstr("ram", srp, olen) ||
		       isstr("meg80", srp, olen)) {
		ic_ops[2] = &ram_as_ops;
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

    /* Allocate buffer, so there is always a linear buffer */
    xmem = malloc(3*K(512));

    /* Create address spaces */
    for (i = 0; i < 3; i++) {
	if (!kb[i]) {
	    as[i] = null_as;
	} else {
	    static const char * const as_names[3]
		= { "meg80-ic1", "meg80-ic2", "meg80-ic3" };
	    as[i] = new_mem(as_names[i], kb[i] << 10, 1,
			    xmem + K(512)*i, ic_ops[i]);
	    as[i]->flags |= AS_NODUMP_ALL;
	}
    }
    as[3] = sys_as;

    meg80p_as = as_new_pagespace("meg80p", 4*K(512), 1, 19);
    meg80p_as->flags |= AS_NODUMP_ALL;

    for (i = 0; i < 4; i++) {
	as_set_pages(meg80p_as, i*K(512), 0, as[i], 0, 1 << 19);
    }

    meg80v_as = as_new_space("meg80v", NULL, Z80_ADDRESS_LIMIT, 32);
    meg80v_as->grain = 9;
    meg80v_as->translate = meg80_translate;
    meg80v_as->flags |= AS_NODUMP_ALL; /* Meaningful only when aliased to cpu */
    meg80_set_map(bootmap);

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
	meg80_ic3_flash_id.b.l = 0xbf;
	meg80_ic3_flash_id.b.h = 0xb5;
	break;
    case 256:
	meg80_ic3_flash_id.b.l = 0xbf;
	meg80_ic3_flash_id.b.h = 0xb6;
	break;
    case 512:
	meg80_ic3_flash_id.b.l = 0xbf;
	meg80_ic3_flash_id.b.h = 0xb7;
	break;
    default:
	meg80_ic3_flash_id.b.l = 0;
	meg80_ic3_flash_id.b.h = 0;
	break;
    }

    /*
     * Address spaces for load/dump convenince
     */
    sram_as = as_alias("sram", K(1536), meg80p_as, 0, 0);
    xmem_as = as_alias("xmem", K(1536), meg80p_as, 0, AS_NODUMP_ALL);

    if (ic_ops[2] != &ram_as_ops) {
	sram_as->len = K(1024);
	flash_as = as_alias("flash", K(512), meg80p_as, K(1024), 0);
    }

    return 0;
}

/* Paged ROM used by Supersmartaid and some other models */

static void ssarom_as_write(struct as *as, size_t faddr, uint8_t v);

static const struct as_ops ssarom_as_ops = {
    .read  = NULL,
    .write = ssarom_as_write,
    .dump  = mem_as_dump,
    .load  = NULL,
    .sync  = NULL,
    .init  = 0xff
};
static struct as *ssarom_as;

static void ssarom_as_write(struct as *as, size_t faddr, uint8_t v)
{
    (void)v;
    as_set_map(as, faddr & 1);
}

static void mem_init_supersmartaid(void)
{
    ssarom_as = new_mem("ssarom", K(5), 2, NULL, &ssarom_as_ops);

    memcpy(ssarom_as->p.data+K(0), rom_abc80_supersmartaid16k+K(0), K(4));
    memcpy(ssarom_as->p.data+K(4), rom_abc80_supersmartaid30k+K(0), K(1));
    memcpy(ssarom_as->p.data+K(5), rom_abc80_supersmartaid16k+K(4), K(4));
    memcpy(ssarom_as->p.data+K(9), rom_abc80_supersmartaid30k+K(1), K(1));

    as_set_map(ssarom_as, 0);

    as_set_pages(sys_as, K(16), 0, ssarom_as, 0, K(4));
    as_set_pages(sys_as, K(30), 0, ssarom_as, K(4), K(1));
}

/* Common memory initialization for all ABC800 models */
static void mem_init_abc800(unsigned int flags, const uint8_t *master_rom)
{
    unsigned int m;
    uint8_t *rom = rom_as->p.data;

    sys_as = as_new_pagespace("sys", K(64), 3, 10);
    sys_as->translate = abc800_sys_translate;
    sys_as->flags |= AS_NODUMP_ALL | AS_ONE_MAP;
    as_point_alias(cpu_as, sys_as, 0);

    if (!(flags & MEMFL_NOBASIC))
	memcpy(rom, master_rom, K(24));
    if (!(flags & MEMFL_NODOS))
	memcpy(rom+K(24), master_rom+K(24), K(4));
    if (!(flags & MEMFL_NOPR))
	memcpy(rom+K(28), master_rom+K(28), K(4));

    /*
     * Map 0: normal execution (ROM, VRAM, RAM)
     * Map 1: execution in option ROM
     * Map 2: extended RAM (FGRAM, MEM...) mapped in
     *
     * Maps 1-2 are initialized to ROM for 0-32K; model-specific
     * code can adjust.
     */
    for (m = 0; m < 3; m++) {
	as_set_pages(sys_as, 0, m, rom_as, 0, K(32));
	as_set_pages(sys_as, K(32), m, ram_as, K(32), K(32));
    }
    as_set_pages(sys_as, K(32)-vram_as->len, 0, vram_as, 0, vram_as->len);

    abc800_set_mem(false);	/* Normal memory mode */
}

/* Common memory initialization for ABC800C/M */
static void mem_init_abc800cm(unsigned int flags, const uint8_t *master_rom)
{
    unsigned int m;

    mem_init_abc800(flags, master_rom);

    /*
     * Map 1: execution in option ROM - FGRAM open, but
     *        16-32K is ROM.
     *
     * Can FGRAM be "opened" like on 802/806? Looks like it, but
     * it *also* looks like it might be possible to do write-under-mask?
     */
    if (!opts.hr)
	return;

    fgram_as = new_ram("fgram", K(16), 1, fgram);

    for (m = 1; m < 3; m++)
	as_set_pages(sys_as, 0, m, fgram_as, 0, K(16));

    xmem_as = as_alias("xmem", K(16), fgram_as, 0, AS_NODUMP_ALL);
}

static inline void set_vram_1k(void)
{
    vram_as->len    = 1024;
    vram_as->mask   = 1023;
    vram_as->grain  = 10;
    vram_as->base   = 1024;	/* Second half of "actual" vram */
}

/*
 * On ABC800C, need to catch a vram write to detect cursor on;
 * this is needed for scripting since simply hooking the interrupt
 * IRQ will drain the input immediately, with all data lost due
 * to buffer overrun.
 */
static void abc800c_vram_as_write(struct as *as, size_t faddr, uint8_t v)
{
    as->p.data[faddr] = v;

    if (v & 0x80)
	cursor_enable_hook();
}

static const struct as_ops abc800c_vram_ops = {
    .read  = NULL,
    .write = abc800c_vram_as_write,
    .dump  = mem_as_dump,
    .load  = NULL,
    .sync  = NULL,
    .init  = -1
};

static inline void init_vram_abc800c(void)
{
    set_vram_1k();
    vram_as->ops    = &abc800c_vram_ops;
}

/*
 * ABC80 memory initialization
 */

struct romset {
    struct rom nonv, nv20, nv22;
};
static void mem_init_abc80(void)
{
    static const struct rom dos_var[] = {
	R(rom_abc80_no_nvram_ufddos80),
	R(rom_abc80_nvram_20k_ufddos80),
	R(rom_abc80_nvram_22k_ufddos80),
	{ rom_abc80_basicii80 + K(24), K(4) }
    };
    static const struct rom print_28_var[] = {
	R(rom_abc80_no_nvram_print80_28),
	R(rom_abc80_nvram_20k_print80_28),
	R(rom_abc80_nvram_22k_print80_28),
	{ rom_abc80_basicii80 + K(28), K(4) }
    };
    static const struct rom print_29_var[] = {
	R(rom_abc80_no_nvram_print80_29),
	R(rom_abc80_nvram_20k_print80_29),
	R(rom_abc80_nvram_22k_print80_29),
	{ NULL, 0 }
    };
    static const struct rom print_30_var[] = {
	R(rom_abc80_no_nvram_print80_30),
	R(rom_abc80_nvram_20k_print80_30),
	R(rom_abc80_nvram_22k_print80_30),
	{ NULL, 0 }
    };
    const struct rom *pr;
    const struct rom *dos = dos_var;
    int var_idx = 0;
    uint8_t * const rom = rom_as->p.data;
    size_t praddr;
    unsigned int m;
    enum memflags flags = opts.memflags;

    if (flags & MEMFL_NOBASIC)
	opts.basic = BASIC_NONE;

    if (opts.meg80 && opts.kb == 64)
	opts.kb = 16;	/* MEG80 and 64K are incompatible */

    sys_as = as_new_pagespace("sys", Z80_ADDRESS_LIMIT,
			      opts.kb == 64 ? 4 : 1, 10);
    sys_as->flags |= AS_NODUMP_ALL | AS_ONE_MAP;
    as_point_alias(cpu_as, sys_as, 0);

    if (opts.meg80) {
	if (init_meg80() < 0)
	    opts.meg80 = false;
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
	opts.smartaid = SA_NONE;     /* Can't smartaid */
	flags |= MEMFL_NONVRAM;
    }

    /* Start by initializing all maps to RAM */
    for (m = 0; m < sys_as->nmaps; m++)
	as_set_pages(sys_as, 0, m, ram_as, 0, K(64));

    /* Map 0: default (for < 64K, the only available map) */

    /* Lower 32K = ROM (overwritten by VRAM latter) */
    as_set_pages(sys_as, 0, 0, rom_as, 0, K(32));

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

    praddr = 0;

    if (opts.praddr) {
	if (opts.praddr < 64)
	    opts.praddr <<= 10;
	praddr = opts.praddr;
	if (praddr < K(28) || praddr > K(30) || (praddr & 1023)) {
	    fprintf(stderr, "%s: invalid printer ROM address: %uK\n",
		    program_name, opts.praddr >> 10);
	    praddr = 0;
	}
    }

    if (!praddr) {
	if (opts.tkn80 == TKN80_GEJO ||
	    opts.smartaid == SA_SUPERSMARTAID ||
	    (opts.kb == 64 && opts.tkn80 != TKN80_NONE &&
	     opts.smartaid != SA_SUPERBASIC)) {
	    praddr = K(29);
	} else {
	    praddr = K(30);
	}
    }

    if (opts.nvram_addr < 64)
	opts.nvram_addr <<= 10;
    opts.nvram_addr &= ~1023;
    if (opts.nvram_size < 1024)
	opts.nvram_size <<= 10;
    opts.nvram_size = (opts.nvram_size + 1023) & ~1023;

    switch (opts.smartaid) {
    case SA_NONE:
	break;
    case SA_SUPERBASIC:
	copyrom(rom+K(16), rom_abc80_superbasic16k);
	copyrom(rom+K(28), rom_abc80_superbasic28k);
	if (praddr < K(30))
	    praddr = 0;
	break;
    case SA_SMARTAID3:
	copyrom(rom+K(16), rom_abc80_smartaid3);
	break;
    case SA_SUPERSMARTAID:
	mem_init_supersmartaid();
	flags &= ~MEMFL_NONVRAM;
	if (opts.nvram_addr == K(22))
	    opts.nvram_size += K(2);
	opts.nvram_addr = K(20);
	if (opts.nvram_size < K(2))
	    opts.nvram_size = K(2);
	break;
    case SA_ABC80L:
	copyrom(rom+K(20), rom_abc80_abc80l);
	break;
    }

    switch (opts.basic) {
    case BASIC_NONE:
	break;
    case BASIC_10042:
	copyrom(rom, rom_abc80_abc80new);
	rom[0x3843] = 0x81;	/* Only byte that differs!! */
	break;
    case BASIC_NEW:
    default:		/* ??? */
	copyrom(rom, rom_abc80_abc80new);
	break;
    case BASIC_OLD:
	copyrom(rom, rom_abc80_abc80old);
	break;
    case BASIC_II:
	memcpy(rom, rom_abc80_basicii80, K(24));
	praddr = K(28);
	break;
    }

    /*
     * Hack: emulated "NVRAM", as was part of the MyAB 128K
     * extension, or available as external cards. Some UFD-DOS
     * modifications seem to have expected a 2K external RAM at
     * 20K address.
     */
    var_idx = 0;	/* No nvram version */

    if (!(flags & MEMFL_NONVRAM)) {
	size_t nvaddr = opts.nvram_addr;
	size_t nvsize = opts.nvram_size;

	if (nvaddr >= Z80_ADDRESS_LIMIT) {
	    nvsize = 0;
	} else if (nvaddr + nvsize > Z80_ADDRESS_LIMIT) {
	    nvsize = Z80_ADDRESS_LIMIT - nvaddr;
	}

	if (nvsize) {
	    nvram_as = new_ram("nvram", nvsize, 1, NULL);

	    if (opts.nvramfile) {
		struct host_file *hf;

		hf = open_host_file(HF_BINARY, NULL, opts.nvramfile,
				    O_RDWR|O_CREAT);
		if (hf) {
		    uint8_t *data = map_file(hf, nvsize);
		    if (data) {
			free(nvram_as->p.data);
			nvram_as->p.data = data;
		    }
		}
	    }

	    as_set_pages(sys_as, nvaddr, 0, nvram_as, 0, nvsize);

	    if (nvaddr <= K(23) && nvaddr+nvsize >= K(24))
		var_idx = 2;	/* 22K version */
	    else if (nvaddr <= K(21) && nvaddr+nvsize >= K(22))
		var_idx = 1;	/* 20K version */
	}
    }

    switch (praddr) {
    case K(28):
	pr = print_28_var;
	break;
    case K(29):
	pr = print_29_var;
	break;
    case K(30):
	pr = print_30_var;
	break;
    default:
	pr = NULL;
	break;
    }

    if (opts.basic == BASIC_II)
	var_idx = 3;

    if (!(flags & MEMFL_NODOS)) {
	dos += var_idx;
	memcpy(rom+K(24), dos->data, dos->len);
    }
    if (!(flags & MEMFL_NOPR) && pr) {
	pr += var_idx;
	memcpy(rom+praddr, pr->data, pr->len);
    }

    /*
     * Note: leave 80-character VRAM always mapped, there is no
     * evidence that any of them unmapped the extra video RAM
     * (why would they?)
     */
    switch (opts.tkn80) {
    case TKN80_NONE:
	set_vram_1k();
	break;
    case TKN80_GEJO:
	as_set_pages(sys_as, K(30), 0, vram_as, K(0), K(1));
	break;
    case TKN80_MYAB:
	as_set_pages(sys_as, K(22), 0, vram_as, K(0), K(2));
	break;
    case TKN80_CAT:
	as_set_pages(sys_as, K(18), 0, vram_as, K(0), K(2));
	break;
    }
    /* Standard 40-char video RAM */
    as_set_pages(sys_as, K(31), 0, vram_as, K(1), K(1));

    /*
     * ABC80 memory grows from the top down. Memory between 32K and
     * the start of RAM is unmapped. Map it to ROM, which normally
     * will be initialized to FF here (by not using the null
     * address space the user can write ROM contents here)
     */
    if (opts.kb < 32)
	as_set_pages(sys_as, K(32), 0, rom_as, K(32), K(32 - opts.kb));

    /*
     * Adjust ROM for TKN80 if applicable
     */
    abc80_mem_setup_mode80(abc80_mem_mode80_p);

    if (opts.kb == 64) {
	/* Map 1: RAM over ROM areas. Video RAM always at 30K for TKN80. */
	/* Map 2: video RAM at the end */
	/* Map 3: all RAM (nothing to do) */
	size_t vlen = vram_as->len;

	as_set_pages(sys_as, K(32)-vlen, 1, vram_as, 0, vlen);
	as_set_pages(sys_as, K(64)-vlen, 2, vram_as, 0, vlen);

	if (opts.bootmap >= 0)
	    as_set_map(sys_as, opts.bootmap & 3);
    }
}

/*
 * Set up memory maps.  Note: dump_memory() currently relies on
 * map 7 being all RAM, regardless of if there is an actual
 * map 7 or not.  If this isn't reliable, change this to have a
 * map set up specifically for Alt-u dumps.
 */
void mem_init(enum memflags flags, const char *memfile)
{
    /* General memory subsystem */
    as_init();

    /* Create common namespaces */
    cpu_as = as_new_aliasspace("cpu", Z80_ADDRESS_LIMIT);
    ram_as = new_ram("ram", Z80_ADDRESS_LIMIT, 1, NULL);
    rom_as = new_rom("rom", Z80_ADDRESS_LIMIT, 1, NULL);
    vram_as = new_ram("vram", K(2), 1, video_ram);

    switch (opts.model) {
    case MODEL_ABC80:
	mem_init_abc80();
	break;

    case MODEL_ABC800C:
	init_vram_abc800c();
	mem_init_abc800cm(flags, rom_abc800_abc800crom);
        break;

    case MODEL_ABC800M:
	mem_init_abc800cm(flags, rom_abc800_abc800mrom);
	break;

    case MODEL_ABC802:
	mem_init_abc800(flags, rom_abc800_abc802rom);

	/* For convenience in loading, mostly, but allow dumping */
	mem_as = as_alias("mem", K(32), ram_as, 0, 0);
	xmem_as = as_alias("xmem", K(32), ram_as, 0, AS_NODUMP_ALL);

        /* Map 1: execution in option ROM - RAM other than the ROM itself */
	as_set_pages(sys_as, 0, 1, ram_as, 0, K(30));

        /* Map 2: MEM area open in its entirety, so all RAM */
	as_set_pages(sys_as, 0, 2, ram_as, 0, K(32));

	/* If we have a MEM: file from the command line, load it */
	load_memfile(memfile);
        break;

    case MODEL_ABC806:
	break;			/* Not implemented yet */
    }
}
