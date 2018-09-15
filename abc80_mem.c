#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "screen.h"
#include "z80.h"

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

#define ROM_START	(0x0000)
#define ROM_END		(0x4000)
#define RAM_START       (0x8000)

static uint8_t memory[MEMORY_SIZE], vram[MEMORY_SIZE];

extern uint16_t video_base, video_mask;

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

/*
 * Macros to determine quickly if an address is writeable.
 */
#define WRITEABLE(address)  ((address) >= ROM_END)

static inline bool is_video(uint16_t address)
{
    return ((address & video_mask) == video_base) &&
	!(model == MODEL_ABC802 && ((REG_PC & video_mask) == video_base));
}

void mem_init(void)
{
}

/*
 * hack to let us initialize the ROM memory
 */
uint8_t *mem_rom_address(void)
{
    return memory;
}


/*
 * hack to get a pointer into the Z80 "memory"
 */
uint8_t *mem_get_addr(uint16_t address)
{
    return &memory[address];
}

static inline uint8_t do_mem_read(uint16_t address)
{
    return is_video(address) ? vram[address] : memory[address];
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

static void do_mem_write(uint16_t address, uint8_t value)
{
    if (is_video(address)) {
	if (vram[address] != value) {
	    vram[address] = value;
	    screen_write(address, value);
	}
    } else if (WRITEABLE(address)) {
	/* write to RAM */
	memory[address] = value;
    }
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
