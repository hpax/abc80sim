#include "compiler.h"
#include "z80.h"
#include "as.h"
#include "debug.h"

/*
 * Actual Z80 memory operations
 */

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

/* Latch the last M1 address fetched, like ABC800 does */
uint16_t last_m1_address;

/*
 * Memory tracing bookkeeping
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

static inline uint8_t do_mem_read(uint16_t address)
{
    return do_as_read(cpu_as, address);
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
    do_as_write(cpu_as, address, value);
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
