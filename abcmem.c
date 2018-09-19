#include "compiler.h"
#include "screen.h"
#include "z80.h"
#include "abcio.h"
#include "rom.h"

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

uint8_t ram[MEMORY_SIZE];

typedef void (*write_func)(uint8_t *p, uint8_t v);
struct mem_page {
    uint8_t *data;
    write_func write;
};

static void write_rom(uint8_t *p, uint8_t v);
static void write_ram(uint8_t *p, uint8_t v);
extern void write_screen(uint8_t *p, uint8_t v);

#define PAGE_SHIFT	10
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_COUNT	(65536U/PAGE_SIZE)

/* Up to 8 memory maps */
#define MEM_MAPS 8
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
    uint8_t  size;
    bool     written;
};

#define MAX_TRACES 16
static struct mem_trace mem_traces[MAX_TRACES+1];
static struct mem_trace *mem_trace_tail = mem_traces;

static inline void
mem_trace_record(uint16_t addr, uint16_t data, uint8_t size, bool written)
{
    if (!(tracing & TRACE_CPU))
	return;

    if (mem_trace_tail <= &mem_traces[MAX_TRACES]) {
	mem_trace_tail->addr    = addr;
	mem_trace_tail->data    = data;
	mem_trace_tail->size    = size;
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

    if (!(tracing & TRACE_CPU))
	return;

    if (mem_trace_tail >= &mem_traces[MAX_TRACES]) {
	mem_trace_tail--;
	overflow = true;
    }

    for (mtp = mem_traces; mtp < mem_trace_tail; mtp++) {
	putchar(' ');
	if (mtp->addr != last_addr || mtp->written != last_written)
	    printf("(%04X)%c", mtp->addr, mtp->written ? '=' : ':');
	printf("%0*X", mtp->size*2, mtp->data);
	last_addr = mtp->addr + mtp->size;
	last_written = mtp->written;
    }

    if (overflow)
	printf(" ...");

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
    b1 = do_mem_read(address+1);

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
static void write_rom(uint8_t *p, uint8_t v)
{
    /* Do nothing */
    (void)p; (void) v;
}
static void write_ram(uint8_t *p, uint8_t v)
{
    *p = v;
}

static void do_mem_write(uint16_t address, uint8_t value)
{
    const struct mem_page *page;

    page = get_page(address);
    page->write(&page->data[address & PAGE_MASK], value);
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
    do_mem_write(address+1, value >> 8);
}

/*
 * Block move instructions, for LDIR and LDDR instructions.
 *
 * Direction is either +1 or -1.
 *
 * Note that a count of zero => move 64K bytes.
 */
void
mem_block_transfer(uint16_t dest, uint16_t source, int direction, uint16_t count)
{
    if(direction > 0)
    {
        do
        {
            mem_write(dest++, mem_read(source++));
            count--;
        }
        while(count);
    }
    else
    {
        do
        {
            mem_write(dest--, mem_read(source--));
            count--;
        }
        while(count);
    }
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


#define K(x) ((x)*1024)
#define ALL_MAPS ((1U << MEM_MAPS)-1)

static void
map_memory(unsigned int maps, size_t where, size_t size,
	   void *what, write_func wfunc)
{
    size_t m;

    assert(((where|size) & PAGE_MASK) == 0);
    assert((maps & ~ALL_MAPS) == 0);

    for (m = 0; maps; m++, maps >>=1) {
	struct mem_page *mp;
	uint8_t *datap;
	size_t npg;

	if (!(maps & 1))
	    continue;

	mp = &memmaps[m][where >> PAGE_SHIFT];
	datap = what;

	npg = size >> PAGE_SHIFT;

	while (npg--) {
	    mp->data  = datap;
	    mp->write = wfunc;
	    datap += PAGE_SIZE;
	    mp++;
	}
    }
}

void mem_init(unsigned int flags)
{
    /* Start by initializing all memory maps to all RAM */
    map_memory(ALL_MAPS, 0, K(64), ram, write_ram);

    switch (model) {
    case MODEL_ABC80:
	/* 4 maps * 2 (40/80) */

	/* Map 0: default */
	if (!(flags & MEMFL_NOBASIC)) {
	    map_memory(0x01, 0, K(16), abc80_bas80, write_rom);
	    map_memory(0x02, 0, K(16), abc80_bas40, write_rom);
	}
	if (!(flags & MEMFL_NODEV)) {
	    /* Hack: allow device ROMs to be written to */
	    map_memory(0x03, K(16), K(16), abc80_devs, write_ram);
	}
	map_memory(0x01, K(29), K(1), &video_ram[K(0)], write_screen);
	map_memory(0x03, K(31), K(1), &video_ram[K(1)], write_screen);

	/* Map 1: RAM over ROM areas */
	map_memory(0x04, K(30), K(2), &video_ram[K(0)], write_screen);
	map_memory(0x08, K(31), K(1), &video_ram[K(1)], write_screen);

	/* Map 2: video RAM at the end */
	map_memory(0x10, K(62), K(2), &video_ram[K(0)], write_screen);
	map_memory(0x20, K(63), K(1), &video_ram[K(1)], write_screen);

	/* Map 3: all RAM */

	abc80_mem_setmap(0);	/* Default to map 0 */
	break;

    case MODEL_ABC802:
	/* Map 0: normal execution */

	if (!(flags & MEMFL_NOBASIC))
	    map_memory(0x01, 0, K(24), abc802rom, write_rom);

	if (!(flags & MEMFL_NODEV))
	    map_memory(0x01, K(24), K(8), &abc802rom[K(24)], write_rom);

	map_memory(0x01, K(30), K(2), video_ram, write_screen);

	/* Map 1: execution in option ROM - RAM other than the ROM itself */
	map_memory(0x02, K(30), K(2), &abc802rom[K(30)], write_rom);

	/* Map 2: MEM area open in its entirety */

	abc802_set_mem(false);	/* On start, MEM area closed */
	break;
    }
}
