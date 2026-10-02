/*
 * Copyright (C) 1992 Clarendon Hill Software.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software, provided this copyright notice is retained.
 *
 * This software is provided "as is" without any expressed or implied
 * warranty.  If this software brings on any sort of damage -- physical,
 * monetary, emotional, or brain -- too bad.  You've got no one to blame
 * but yourself.
 *
 * The software may be modified for your own purposes, but modified versions
 * must retain this notice.
 */

/*
 * z80.c:  The guts of the Z-80 emulator.
 *
 * The Z-80 emulator should be general and complete enough to be easily
 * adapted to emulate any Z-80 machine, although it's only really been tested
 * with TRS-80 code.  The only thing we cheat a little on is interrupt
 * handling and the refresh register.  All of the flags are supported.
 * All of the documented Z-80 instructions are implemented.
 *
 * There are undobutedly bugs in the emulator.  If you discover any,
 * please do send a report.
 */
/* Copyright (C) 2026 H. Peter Anvin <hpa@zytor.com> */
#include "z80.h"
#include "z80irq.h"
#include "sysload.h"
#include "clock.h"
#include "debug.h"

/*
 * The state of our Z-80 registers is kept in this structure:
 */
struct z80_state_struct z80_state;

static char traceline[1024];
static size_t tracelinelen;

static printf_func(1,2) void add_cputrace(const char *fmt, ...)
{
    va_list ap;
    size_t left = sizeof traceline - 1 - tracelinelen;
    size_t len;

    va_start(ap, fmt);
    len = vsnprintf(traceline+tracelinelen, left, fmt, ap);
    va_end(ap);

    if (len >= left)
	tracelinelen = sizeof traceline - 2;
    else
	tracelinelen += len;
}

static void diffstate(void);

static inline void set_flags(uint8_t flags)
{
    REG_F = flags;
    z80_state.q = true;
}

/*
 * T-states (clock cycles) for various instructions.
 * This reflects the base clock count; in particular:
 * - JR, DJNZ, RET, and CALL need to be adjusted to assume not taken,
 *   -> even the unconditional ones: subtract 5, 5, 6, 7 respectively.
 * - Block instructions not repeated
 * - HALT instruction does not loop
 *
 * Prefix opcodes count as 4 cycles for the prefix itself
 *
 * Future work: refactor this to keep track of T-states for specific
 * machine cycles.
 */

/* Main opcode group */
static const uint8_t clk_main[256] = {
    /*         0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
    /* 00 */   4, 10,  7,  6,  4,  4,  7,  4,  4, 11,  7,  6,  4,  4,  7,  4,
    /* 10 */   8, 10,  7,  6,  4,  4,  7,  4,  7, 11,  7,  6,  4,  4,  7,  4,
    /* 20 */   7, 10, 16,  6,  4,  4,  7,  4,  7, 11, 16,  6,  4,  4,  7,  4,
    /* 30 */   7, 10, 13,  6, 11, 11, 10,  4,  7, 11, 13,  6,  4,  4,  7,  4,
    /* 40 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* 50 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* 60 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* 70 */   7,  7,  7,  7,  7,  7,  4,  7,  4,  4,  4,  4,  4,  4,  7,  4,
    /* 80 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* 90 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* a0 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* b0 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    /* c0 */   5, 10, 10, 10, 10, 11,  7, 11,  5,  4, 10,  4, 10, 10,  7, 11,
    /* d0 */   5, 10, 10, 11, 10, 11,  7, 11,  5,  4, 10, 11, 10,  4,  7, 11,
    /* e0 */   5, 10, 10, 19, 10, 11,  7, 11,  5,  4, 10,  4, 10,  4,  7, 11,
    /* f0 */   5, 10, 10,  4, 10, 11,  7, 11,  5,  6, 10,  4, 10,  4,  7, 11,
};

/* EB opcode group - not including 4 cycles for the EB prefix itself */
#define X 4			/* Believed to be NOPs with this timing */
static const uint8_t clk_ED[256] = {
    /*         0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
    /* 00 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* 10 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* 20 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* 30 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* 40 */   8,  8, 11, 16,  4, 10,  4,  5,  8,  8, 11, 16,  4, 10,  4,  5,
    /* 50 */   8,  8, 11, 16,  4, 10,  4,  5,  8,  8, 11, 16,  4, 10,  4,  5,
    /* 60 */   8,  8, 11, 16,  4, 10,  4, 14,  8,  8, 11, 16,  4, 10,  4, 14,
    /* 70 */   8,  8, 11, 16,  4, 10,  4,  X,  8,  8, 11, 16,  4, 10,  4,  X,
    /* 80 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* 90 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* a0 */  12, 12, 12, 12,  X,  X,  X,  X, 12, 12, 12, 12,  X,  X,  X,  X,
    /* b0 */  12, 12, 12, 12,  X,  X,  X,  X, 12, 12, 12, 12,  X,  X,  X,  X,
    /* c0 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* d0 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* e0 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
    /* f0 */   X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,
};

#undef X

/*
 * Utility macros: swap 8- and 16-bit values
 */
static inline void do_swapb(uint8_t *a, uint8_t *b)
{
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}
#define SWAPB(a,b) do_swapb(&(a),&(b))

static inline void do_swapw(uint16_t *a, uint16_t *b)
{
    uint16_t tmp = *a;
    *a = *b;
    *b = tmp;
}
#define SWAPW(a,b) do_swapw(&(a),&(b))

/*
 * Issue a refresh cycle
 */
static void rfsh(void)
{
    REG_IR = (REG_IR & ~0x7f) | ((REG_IR + 1) & 0x7f);
    mem_rfsh(REG_IR);
}

/*
 * Read a byte from PC while asserting M1#, advance PC, and issue a
 * refresh cycle XXX: this always advances TSTATE by 4, this could be
 * baked into the cycle count tables.
 */

/*
 * Perform the opcode read, but without advancing the actual machine
 * state; this is used to handle software breakpoints without
 * advancing the CPU state (a hack for debugger convenience only.)
 */
static inline uint8_t fetch_m1_peek(void)
{
    return mem_fetch_m1(REG_PC);
}
static void fetch_m1_commit(void)
{
    REG_PC++;
    rfsh();
}

static uint8_t fetch_m1(void)
{
    uint8_t b = fetch_m1_peek();
    fetch_m1_commit();
    return b;
}

/*
 * Read a byte from PC without asserting M1#, and advance PC Note that
 * from the CPU standpoint, this is regular memory read, not a fetch;
 * the two are indistinguishable on the bus.  The reason for having
 * different memory layer operations is to help out the
 * tracers/debuggers.
 */
static uint8_t fetch_byte(void)
{
    return mem_fetch(REG_PC++);
}

/* Read a word from PC without asserting M1#, and advance PC */
static uint16_t fetch_word(void)
{
    uint16_t w = mem_fetch_word(REG_PC);
    REG_PC += 2;
    return w;
}

/* Read an 8-bit value from an immediate memory address */
static uint8_t direct_byte(void)
{
    uint16_t address = fetch_word();

    REG_WZ = address + 1;
    return mem_read(address);
}

/* Read a 16-bit value from an immediate memory address */
static uint16_t direct_word(void)
{
    uint16_t address = fetch_word();

    REG_WZ = address + 1;
    return mem_read_word(address);
}

#define HLIX REG(IXREG)	       /* HL, IX, or IY */

/*
 * The address for (HL) or (Ixy+nn) operations.
 */
static uint16_t hlix_addr(void)
{
    REG_WZ = HLIX.w + z80_state.ixdisp;
    return REG_WZ;
}

/* Read a byte from (HL)/(Ixy+n) */
static uint8_t read_byte_hlix(void)
{
    return mem_read(hlix_addr());
}

/* Write a byte to (HL)/(Ixy+n) */
static uint8_t write_byte_hlix(uint8_t b)
{
    mem_write(hlix_addr(), b);
    return b;
}

/* Get an 8-bit value from a register, (HL) or (Ixy+nn) */
static uint8_t get8(uint8_t op)
{
    switch (op & 7) {
    case 0: return REG_B;
    case 1: return REG_C;
    case 2: return REG_D;
    case 3: return REG_E;
    case 4: return HLIX.b.h;	/* H, IXH, IYH */
    case 5: return HLIX.b.l;	/* L, IXL, IYL */
    case 6: return read_byte_hlix();
    case 7: return REG_A;
    }
    abort();
}

/* Write an 8-bit value to a register, (HL) or (Ixy+nn) */
static uint8_t set8(uint8_t op, uint8_t val)
{
    switch (op & 7) {
    case 0: return REG_B = val;
    case 1: return REG_C = val;
    case 2: return REG_D = val;
    case 3: return REG_E = val;
    case 4: return HLIX.b.h = val;	/* H, IXH, IYH */
    case 5: return HLIX.b.l = val;	/* L, IXL, IYL */
    case 6: return write_byte_hlix(val);
    case 7: return REG_A = val;
    }
    abort();
}

/*
 * RMW add/sub to an 8-bit operand; no flags modified, returns the
 * updated value. Used for INC/DEC.
 */
static uint8_t add8(uint8_t op, int8_t delta)
{
    return set8(op, get8(op) + delta);
}

/* Similar, but ignore IX/IY prefixes */

static uint8_t get8noix(uint8_t op)
{
    switch (op & 7) {
    case 0: return REG_B;
    case 1: return REG_C;
    case 2: return REG_D;
    case 3: return REG_E;
    case 4: return REG_H;
    case 5: return REG_L;
    case 6: return mem_read(REG_HL);
    case 7: return REG_A;
    }
    abort();
}

static uint8_t set8noix(uint8_t op, uint8_t val)
{
    switch (op & 7) {
    case 0: REG_B = val; break;
    case 1: REG_C = val; break;
    case 2: REG_D = val; break;
    case 3: REG_E = val; break;
    case 4: REG_H = val; break;
    case 5: REG_L = val; break;
    case 6: mem_write(REG_HL, val); break;
    case 7: REG_A = val; break;
    default: abort(); break;
    }
    return val;
}

/* Similar, but reads 0 instead of (HL), and drops a write; no index regs */

static uint8_t get8reg(uint8_t op)
{
    switch (op & 7) {
    case 0: return REG_B;
    case 1: return REG_C;
    case 2: return REG_D;
    case 3: return REG_E;
    case 4: return REG_H;
    case 5: return REG_L;
    case 6: return 0;
    case 7: return REG_A;
    default: abort();
    }
}

static uint8_t set8reg(uint8_t op, uint8_t val)
{
    switch (op & 7) {
    case 0: REG_B = val; break;
    case 1: REG_C = val; break;
    case 2: REG_D = val; break;
    case 3: REG_E = val; break;
    case 4: REG_H = val; break;
    case 5: REG_L = val; break;
    case 6: break;
    case 7: REG_A = val; break;
    default: abort();
    }
    return val;
}

/* Get the canonical register pair. */
static regpair *get_rp(uint8_t op)
{
    uint16_t *rp;
    switch ((op >> 4) & 3) {
    case 0: rp = &REG_BC; break;
    case 1: rp = &REG_DE; break;
    case 2: rp = &HLIX.w; break;
    case 3: rp = &REG_SP; break;
    }
    return (regpair *)rp;
}

/* Same, but for push/pop (AF rather than SP) */
static regpair *get_rp_af(uint8_t op)
{
    uint16_t *rp;
    switch ((op >> 4) & 3) {
    case 0: rp = &REG_BC; break;
    case 1: rp = &REG_DE; break;
    case 2: rp = &HLIX.w; break;
    case 3: rp = &REG_AF; break;
    }
    return (regpair *)rp;
}

/*
 * Push and pop registers to/from the stack
 */
static void push(uint16_t w)
{
    REG_SP -= 2;
    mem_write_word(REG_SP, w);
}
static uint16_t pop(void)
{
    uint16_t w = mem_read_word(REG_SP);
    REG_SP += 2;
    return w;
}

/* case8 is for the lowest octal digit, case8x for the middle */
#define CASE8(x) \
    case ((x)+0): case ((x)+1): case ((x)+2): case ((x)+3):	\
    case ((x)+4): case ((x)+5): case ((x)+6): case ((x)+7)
#define CASE8x(x) \
    case ((x)+ 0): case ((x)+ 8): case ((x)+16): case ((x)+24):	\
    case ((x)+32): case ((x)+40): case ((x)+48): case ((x)+56)
#define CASE4rp(x) \
    case ((x)+ 0): case ((x)+16): case ((x)+32): case ((x)+48)

/*
 * Tables and routines for computing various flag values:
 */
static const uint8_t sign_carry_overflow_table[] = {
    0,
    OVERFLOW_MASK | SIGN_MASK,
    CARRY_MASK,
    SIGN_MASK,
    CARRY_MASK,
    SIGN_MASK,
    CARRY_MASK | OVERFLOW_MASK,
    CARRY_MASK | SIGN_MASK,
};

static const uint8_t half_carry_table[] = {
    0,
    0,
    HALF_CARRY_MASK,
    0,
    HALF_CARRY_MASK,
    0,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
};

static const uint8_t subtract_sign_carry_overflow_table[] = {
    0,
    CARRY_MASK | SIGN_MASK,
    CARRY_MASK,
    OVERFLOW_MASK | CARRY_MASK | SIGN_MASK,
    OVERFLOW_MASK,
    SIGN_MASK,
    0,
    CARRY_MASK | SIGN_MASK,
};

static const uint8_t subtract_half_carry_table[] = {
    0,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
    HALF_CARRY_MASK,
    0,
    0,
    0,
    HALF_CARRY_MASK,
};

static inline bool const_func even_parity(uint8_t value)
{
#ifdef HAVE___BUILTIN_PARITY
    return !__builtin_parity(value);
#elif defined(HAVE_STDC_COUNT_ONES)
    return !(stdc_count_ones(value) & 1);
#elif defined(HAVE___BUILTIN_POPCNT)
    return !(__builtin_popcnt(value) & 1);
#elif defined(HAVE___POPCNT)
    return !(__popcnt(value) & 1);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    bool out;
    asm("test %1,%1" : "=@ccp" (out) : "q" (value));
    return out;
#else
    value ^= (value << 4);
    value ^= (value << 2);
    value ^= (value << 1);
    return (int8_t)value >= 0;
#endif
}

static void do_add_flags(int a, int b, int result)
{
    /*
     * Compute the flag values for a + b = result operation
     */
    int index;
    int f;

    /*
     * sign, carry, and overflow depend upon values of bit 7.
     * half-carry depends upon values of bit 3.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     */

    f = REG_F & ~(SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | XY_MASK
                  | OVERFLOW_MASK | SUBTRACT_MASK | CARRY_MASK);

    index = ((a & 0x88) >> 1) | ((b & 0x88) >> 2) | ((result & 0x88) >> 3);
    f |= half_carry_table[index & 7] | sign_carry_overflow_table[index >> 4];

    if ((result & 0xFF) == 0)
        f |= ZERO_MASK;

    set_flags(f | (result & XY_MASK));
}

static void do_sub_flags(int a, int b, int result)
{
    int index;
    int f;

    /*
     * sign, carry, and overflow depend upon values of bit 7.
     * half-carry depends upon values of bit 3.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     */

    f = (REG_F | SUBTRACT_MASK) & ~(SIGN_MASK | ZERO_MASK |
                                    HALF_CARRY_MASK | XY_MASK |
                                    OVERFLOW_MASK | CARRY_MASK);

    index = ((a & 0x88) >> 1) | ((b & 0x88) >> 2) | ((result & 0x88) >> 3);
    f |= subtract_half_carry_table[index & 7] |
        subtract_sign_carry_overflow_table[index >> 4];

    if ((result & 0xFF) == 0)
        f |= ZERO_MASK;

    set_flags(f | (result & XY_MASK));
}

static uint16_t do_adc_word_flags(uint16_t a, uint16_t b)
{
    uint16_t result = a + b + !!CARRY_FLAG;
    int index;
    int f;

    /*
     * sign, carry, and overflow depend upon values of bit 15.
     * half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     */

    f = REG_F & ~(SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | XY_MASK
                  | OVERFLOW_MASK | SUBTRACT_MASK | CARRY_MASK);

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
        ((result & 0x8800) >> 11);

    f |= half_carry_table[index & 7] | sign_carry_overflow_table[index >> 4];

    if (!result)
        f |= ZERO_MASK;

    set_flags(f | ((result >> 8) & XY_MASK));
    REG_WZ = a + 1;
    return result;
}

static uint16_t do_add_word_flags(uint16_t a, uint16_t b)
{
    uint16_t result = a + b;
    unsigned int index;
    uint8_t f;

    /*
     * carry depends upon values of bit 15.
     * half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     */

    f = REG_F & ~(HALF_CARRY_MASK | SUBTRACT_MASK | CARRY_MASK | XY_MASK);

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
        ((result & 0x8800) >> 11);

    f |= half_carry_table[index & 7] |
        (sign_carry_overflow_table[index >> 4] & CARRY_MASK);

    set_flags(f | ((result >> 8) & XY_MASK));
    REG_WZ = a + 1;
    return result;
}

static uint16_t do_sbc_word_flags(uint16_t a, uint16_t b)
{
    uint16_t result = a - (b + !!CARRY_FLAG);
    unsigned int index;
    uint8_t f;

    /*
     * sign, carry, and overflow depend upon values of bit 15.
     * half-carry depends upon values of bit 11.
     * We mask those bits, munge them into an index, and look
     * up the flag values in the above tables.
     */

    f = (REG_F | SUBTRACT_MASK) &
	~(SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | XY_MASK
	  | OVERFLOW_MASK | CARRY_MASK);

    index = ((a & 0x8800) >> 9) | ((b & 0x8800) >> 10) |
        ((result & 0x8800) >> 11);

    f |= subtract_half_carry_table[index & 7] |
        subtract_sign_carry_overflow_table[index >> 4];

    if (!result)
        f |= ZERO_MASK;

    set_flags(f | ((result >> 8) & XY_MASK));
    REG_WZ = a + 1;
    return result;
}

static void do_inc_dec_byte(uint8_t op)
{
    int8_t  delta;
    uint8_t wrap;
    uint8_t value, set;
    const uint8_t clear = SUBTRACT_MASK | OVERFLOW_MASK |
	HALF_CARRY_MASK | ZERO_MASK | SIGN_MASK | XY_MASK;

    if (op & 1) {
	delta  = -1;
	wrap   = 0x7f;
	set    = SUBTRACT_MASK;
    } else {
	delta  = 1;
	wrap   = 0x80;
	set    = 0;
    }

    value = add8(op >> 3, delta);
    wrap ^= value;

    if (!wrap)
        set |= OVERFLOW_MASK;
    if (!(wrap & 0xf))
        set |= HALF_CARRY_MASK;
    if (!value)
        set |= ZERO_MASK;
    if ((int8_t)value < 0)
        set |= SIGN_MASK;

    set_flags((REG_F & ~clear) | set | (value & XY_MASK));
}

/*
 * Routines for executing or assisting various non-trivial arithmetic
 * instructions:
 */
static void do_and_byte(int value)
{
    int result;
    uint8_t clear, set;

    result = (REG_A &= value);

    clear = CARRY_MASK | SUBTRACT_MASK | PARITY_MASK | ZERO_MASK | SIGN_MASK |
        XY_MASK;
    set = HALF_CARRY_MASK;

    if (even_parity(result))
        set |= PARITY_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (result & 0x80)
        set |= SIGN_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));
}

static void do_or_byte(int value)
{
    int result;                 /* the result of the or operation */
    uint8_t clear, set;

    result = (REG_A |= value);

    clear = CARRY_MASK | SUBTRACT_MASK | PARITY_MASK
        | HALF_CARRY_MASK | ZERO_MASK | SIGN_MASK | XY_MASK;
    set = 0;

    if (even_parity(result))
        set |= PARITY_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (result & 0x80)
        set |= SIGN_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));
}

static void do_xor_byte(int value)
{
    int result;                 /* the result of the xor operation */
    uint8_t clear, set;

    result = (REG_A ^= value);

    clear = CARRY_MASK | SUBTRACT_MASK | PARITY_MASK
        | HALF_CARRY_MASK | ZERO_MASK | SIGN_MASK | XY_MASK;
    set = 0;

    if (even_parity(result))
        set |= PARITY_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (result & 0x80)
        set |= SIGN_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));
}

static void do_add_byte(int value)
{
    int a, result;

    result = (a = REG_A) + value;
    REG_A = result;
    do_add_flags(a, value, result);
}

static void do_adc_byte(int value)
{
    int a, result;

    if (CARRY_FLAG)
        result = (a = REG_A) + value + 1;
    else
        result = (a = REG_A) + value;
    REG_A = result;
    do_add_flags(a, value, result);
}

static void do_sub_byte(int value)
{
    int a, result;

    result = (a = REG_A) - value;
    REG_A = result;
    do_sub_flags(a, value, result);
}

static void do_negate(void)
{
    int a;

    a = REG_A;
    REG_A = -a;
    do_sub_flags(0, a, REG_A);
    if (a == 0)
        SET_CARRY();
}

static void do_sbc_byte(int value)
{
    int a, result;

    result = (a = REG_A) - (value + !!CARRY_FLAG);
    REG_A = result;
    do_sub_flags(a, value, result);
}

static void do_cp_byte(int value)
{                               /* compare this value with A's contents */
    int a, result;

    result = (a = REG_A) - value;
    do_sub_flags(a, value, result);
    set_flags((REG_F & ~XY_MASK) | (value & XY_MASK));
}

/* 8-bit arithmetic against the accumulator */
static void do_arith_byte(uint8_t op, uint8_t val)
{
    switch ((op >> 3) & 7) {
    case 0: do_add_byte(val); break;
    case 1: do_adc_byte(val); break;
    case 2: do_sub_byte(val); break;
    case 3: do_sbc_byte(val); break;
    case 4: do_and_byte(val); break;
    case 5: do_xor_byte(val); break;
    case 6: do_or_byte(val);  break;
    case 7: do_cp_byte(val);  break;
    }
}

/* Handle the repeat condition for string ops */
static inline void string_rep(uint8_t op, bool quit)
{
    if (quit || !(op & 16))
	return;

    TSTATE += 5;
    REG_PC -= 2;
}

static inline int string_dir(uint8_t op)
{
    return (op & 8) ? -1 : 1;
}

static void do_cpid(uint8_t op)
{
    uint8_t value = mem_read(REG_HL);
    uint8_t result = REG_A - value;

    do_cp_byte(value);
    REG_HL += string_dir(op);
    REG_BC--;
    REG_WZ += string_dir(op);

    if (REG_BC == 0)
        CLEAR_OVERFLOW();
    else
        SET_OVERFLOW();

    set_flags((REG_F & ~XY_MASK) | ((result - !!HALF_CARRY_FLAG) & X_MASK) |
              (((result - !!HALF_CARRY_FLAG) & 2) << 4));
    if ((op & 16) && REG_BC != 0 && !ZERO_FLAG) {
        string_rep(op, false);
        REG_WZ = REG_PC + 1;
    } else {
        string_rep(op, true);
        if (op & 16)
            REG_WZ++;
    }
}

static uint8_t do_test_bit(uint8_t value, unsigned int bit)
{
    uint8_t clear, set;

    clear = SIGN_MASK | ZERO_MASK | OVERFLOW_MASK | SUBTRACT_MASK | XY_MASK;
    set = HALF_CARRY_MASK;

    if ((value & (1 << bit)) == 0)
        set |= ZERO_MASK;

    set_flags((REG_F & ~clear) | set | (value & XY_MASK));
    return value;
}

static int rl_byte(int value)
{
    /*
     * Compute rotate-left-through-carry
     * operation, setting flags as appropriate.
     */

    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (CARRY_FLAG) {
        result = ((value << 1) & 0xFF) | 1;
    } else {
        result = (value << 1) & 0xFF;
    }

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x80)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static int rr_byte(int value)
{
    /*
     * Compute rotate-right-through-carry
     * operation, setting flags as appropriate.
     */

    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (CARRY_FLAG) {
        result = (value >> 1) | 0x80;
    } else {
        result = (value >> 1);
    }

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x1)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static int rlc_byte(int value)
{
    /*
     * Compute the result of an RLC operation and set the flags appropriately.
     * This does not do the right thing for the RLCA instruction.
     */

    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (value & 0x80) {
        result = ((value << 1) & 0xFF) | 1;
        set |= CARRY_MASK;
    } else {
        result = (value << 1) & 0xFF;
    }

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static int rrc_byte(int value)
{
    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (value & 0x1) {
        result = (value >> 1) | 0x80;
        set |= CARRY_MASK;
    } else {
        result = (value >> 1);  /* fixed /jonas-y */
    }

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

/*
 * Perform the RLA, RLCA, RRA, RRCA instructions.  These set the flags
 * differently than the other rotate instrucitons.
 */
static void do_rla(void)
{
    uint8_t clear, set;

    clear = HALF_CARRY_MASK | SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (REG_A & 0x80)
        set |= CARRY_MASK;

    if (CARRY_FLAG) {
        REG_A = ((REG_A << 1) & 0xFF) | 1;
    } else {
        REG_A = (REG_A << 1) & 0xFF;
    }

    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
}

static void do_rra(void)
{
    uint8_t clear, set;

    clear = HALF_CARRY_MASK | SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (REG_A & 0x1)
        set |= CARRY_MASK;

    if (CARRY_FLAG) {
        REG_A = (REG_A >> 1) | 0x80;
    } else {
        REG_A = REG_A >> 1;
    }
    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
}

static void do_rlca(void)
{
    uint8_t clear, set;

    clear = HALF_CARRY_MASK | SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (REG_A & 0x80) {
        REG_A = ((REG_A << 1) & 0xFF) | 1;
        set |= CARRY_MASK;
    } else {
        REG_A = (REG_A << 1) & 0xFF;
    }
    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
}

static void do_rrca(void)
{
    uint8_t clear, set;

    clear = HALF_CARRY_MASK | SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (REG_A & 0x1) {
        REG_A = (REG_A >> 1) | 0x80;
        set |= CARRY_MASK;
    } else {
        REG_A = REG_A >> 1;
    }
    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
}

static int sla_byte(int value)
{
    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    result = value << 1;

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x80)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

/* SLL is an undocumented instruction which shifts left and sets the LSB */
static int sll_byte(int value)
{
    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    result = (value << 1) | 1;

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x80)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static int sra_byte(int value)
{
    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    if (value & 0x80) {
        result = (value >> 1) & 0x80;
        set |= SIGN_MASK;
    } else {
        result = value >> 1;
    }

    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x1)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static int srl_byte(int value)
{
    uint8_t clear, set;
    int result;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | CARRY_MASK | XY_MASK;
    set = 0;

    result = value >> 1;

    if (result & 0x80)
        set |= SIGN_MASK;
    if (result == 0)
        set |= ZERO_MASK;
    if (even_parity(result))
        set |= PARITY_MASK;
    if (value & 0x1)
        set |= CARRY_MASK;

    set_flags((REG_F & ~clear) | set | (result & XY_MASK));

    return result;
}

static void do_ldid(uint8_t op)
{
    uint8_t value = mem_read(REG_HL);
    uint8_t result = REG_A + value;

    mem_write(REG_DE, value);

    REG_DE += string_dir(op);
    REG_HL += string_dir(op);
    REG_BC--;

    CLEAR_HALF_CARRY();
    CLEAR_SUBTRACT();
    if (REG_BC == 0)
        CLEAR_OVERFLOW();
    else
        SET_OVERFLOW();

    set_flags((REG_F & ~XY_MASK) | (result & X_MASK) |
              ((result & 2) << 4));
    if ((op & 16) && REG_BC != 0) {
        string_rep(op, false);
        REG_WZ = REG_PC + 1;
    } else {
        string_rep(op, true);
    }
}

static void do_ld_a_ir(uint8_t val)
{
    uint8_t clear, set;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | OVERFLOW_MASK |
        SUBTRACT_MASK | XY_MASK;
    set = 0;

    REG_A = val;

    if (REG_A & 0x80)
        set |= SIGN_MASK;
    if (REG_A == 0)
        set |= ZERO_MASK;

    if (z80_state.iff2)
        set |= OVERFLOW_MASK;

    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
}

static void do_daa(void)
{
    /*
     * The bizzare decimal-adjust-accumulator instruction....
     */

    int high_nibble, low_nibble, add, carry, subtract_flag;

    high_nibble = REG_A >> 4;
    low_nibble = REG_A & 0xf;
    subtract_flag = SUBTRACT_FLAG;

    if (subtract_flag == 0) {   /* add, adc, inc */
        if (CARRY_FLAG == 0) {  /* no carry */
            if (HALF_CARRY_FLAG == 0) { /* no half-carry */
                if (low_nibble < 10) {
                    if (high_nibble < 10) {
                        add = 0x00;
                        carry = 0;
                    } else {
                        add = 0x60;
                        carry = 1;
                    }
                } else {
                    if (high_nibble < 9) {
                        add = 0x06;
                        carry = 0;
                    } else {
                        add = 0x66;
                        carry = 1;
                    }
                }
            } else {            /* half-carry */

                if (high_nibble < 10) {
                    add = 0x06;
                    carry = 0;
                } else {
                    add = 0x66;
                    carry = 1;
                }
            }
        } else {                /* carry */

            if (HALF_CARRY_FLAG == 0) { /* no half-carry */
                if (low_nibble < 10) {
                    add = 0x60;
                    carry = 1;
                } else {
                    add = 0x66;
                    carry = 1;
                }
            } else {            /* half-carry */

                add = 0x66;
                carry = 1;
            }
        }
    } else {                    /* sub, sbc, dec, neg */

        if (CARRY_FLAG == 0) {  /* no carry */
            if (HALF_CARRY_FLAG == 0) { /* no half-carry */
                add = 0x00;
                carry = 0;
            } else {            /* half-carry */

                add = 0xFA;
                carry = 0;
            }
        } else {                /* carry */

            if (HALF_CARRY_FLAG == 0) { /* no half-carry */
                add = 0xA0;
                carry = 1;
            } else {            /* half-carry */

                add = 0x9A;
                carry = 1;
            }
        }
    }

    do_add_byte(add);           /* adjust the value */

    if (even_parity(REG_A))          /* This seems odd -- is it a mistake? */
        SET_PARITY();
    else
        CLEAR_PARITY();

    if (subtract_flag)          /* leave the subtract flag intact (right?) */
        SET_SUBTRACT();
    else
        CLEAR_SUBTRACT();

    if (carry)
        SET_CARRY();
    else
        CLEAR_CARRY();
}

static void do_rld(void)
{
    /*
     * Rotate-left-decimal.
     */
    int old_value, new_value;
    uint8_t clear, set;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | XY_MASK;
    set = 0;

    old_value = mem_read(REG_HL);

    /* left-shift old value, add lower bits of a */
    new_value = ((old_value << 4) | (REG_A & 0x0f)) & 0xff;

    /* rotate high bits of old value into low bits of a */
    REG_A = (REG_A & 0xf0) | (old_value >> 4);

    if (REG_A & 0x80)
        set |= SIGN_MASK;
    if (REG_A == 0)
        set |= ZERO_MASK;
    if (even_parity(REG_A))
        set |= PARITY_MASK;

    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
    mem_write(REG_HL, new_value);
    REG_WZ = REG_HL + 1;
}

static void do_rrd(void)
{
    /*
     * Rotate-right-decimal.
     */
    int old_value, new_value;
    uint8_t clear, set;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK | PARITY_MASK |
        SUBTRACT_MASK | XY_MASK;
    set = 0;

    old_value = mem_read(REG_HL);

    /* right-shift old value, add lower bits of a */
    new_value = (old_value >> 4) | ((REG_A & 0x0f) << 4);

    /* rotate low bits of old value into low bits of a */
    REG_A = (REG_A & 0xf0) | (old_value & 0x0f);

    if (REG_A & 0x80)
        set |= SIGN_MASK;
    if (REG_A == 0)
        set |= ZERO_MASK;
    if (even_parity(REG_A))
        set |= PARITY_MASK;

    set_flags((REG_F & ~clear) | set | (REG_A & XY_MASK));
    mem_write(REG_HL, new_value);
    REG_WZ = REG_HL + 1;
}

/*
 * Input/output instruction support:
 */

static void do_inid(uint8_t op)
{
    mem_write(REG_HL, z80_in(REG_BC));
    REG_HL += (op & 8) ? -1 : 1;
    REG_B--;
    REG_WZ = REG_BC + ((op & 8) ? -2 : 1);

    if (REG_B == 0)
        SET_ZERO();
    else
        CLEAR_ZERO();

    SET_SUBTRACT();

    string_rep(op, REG_B == 0);
}

static uint8_t in_with_flags(uint16_t port)
{
    /*
     * Do the appropriate flag calculations for the in instructions
     * which compute the flags.  Return the input value.
     */

    uint8_t value;
    uint8_t clear, set;

    clear = SIGN_MASK | ZERO_MASK | HALF_CARRY_MASK |
        PARITY_MASK | SUBTRACT_MASK | XY_MASK;
    set = 0;

    value = z80_in(port);

    if (value & 0x80)
        set |= SIGN_MASK;
    if (value == 0)
        set |= ZERO_MASK;
    if (even_parity(value))
        set |= PARITY_MASK;

    /* What should the half-carry do?  Is this a mistake? */

    set_flags((REG_F & ~clear) | set | (value & XY_MASK));

    return value;
}

static void do_outid(uint8_t op)
{
    z80_out(REG_BC, mem_read(REG_HL));
    REG_HL += string_dir(op);
    REG_B--;
    REG_WZ = REG_BC + ((op & 8) ? -2 : 1);

    if (REG_B == 0)
        SET_ZERO();
    else
        CLEAR_ZERO();

    SET_SUBTRACT();

    string_rep(op, REG_B == 0);
}

/*
 * Interrupt handling routines:
 */

static void do_di(void)
{
    z80_state.iff1 = z80_state.iff2 = false;
}

static void do_ei(void)
{
    z80_state.iff1 = z80_state.iff2 = true;
    z80_state.ei_shadow = true;
}

static void do_im0(void)
{
    z80_state.interrupt_mode = 0;
}

static void do_im1(void)
{
    z80_state.interrupt_mode = 1;
}

static void do_im2(void)
{
    z80_state.interrupt_mode = 2;
}

static void do_nmi(void)
{
    return;

    /* handle a non-maskable interrupt */
    if (tracing(TRACE_IO | TRACE_CPU)) {
        fprintf(tracef, "[%12" PRIu64 "] NMI: PC=%04x\n", TSTATE, REG_PC);
    }

    REG_SP -= 2;
    mem_write_word(REG_SP, REG_PC);
    z80_state.iff2 = z80_state.iff1;
    z80_state.iff1 = false;
    z80_state.nmi_in_progress = true;
    z80_state.running = true;
    REG_PC = 0x66;
    rfsh();
    TSTATE += 11;

    atomic_fetch_and(&z80_state.uncond, ~UCEV_NMI);
}

static void do_int(void)
{
    uint16_t old_pc = REG_PC;
    uint64_t when = TSTATE;
    int i_vector;

    i_vector = z80_intack();
    if (i_vector < 0)
        return;

    switch (z80_state.interrupt_mode) {
    case 0:
        /* We blithly assume we are fed an RST instruction, otherwise
	   this is totally wrong... */
        do_di();
        REG_SP -= 2;
        mem_write_word(REG_SP, REG_PC);
        REG_PC = i_vector & 0x38;
        REG_WZ = REG_PC;
        TSTATE += 11;
        break;

    case 1:
        do_di();
        REG_SP -= 2;
        mem_write_word(REG_SP, REG_PC);
        REG_PC = 0x38;
        REG_WZ = REG_PC;
        TSTATE += 11;
        break;

    case 2:
        do_di();
        REG_SP -= 2;
        mem_write_word(REG_SP, REG_PC);
        REG_PC = mem_read_word((REG_IR & 0xff00) + i_vector);
        REG_WZ = REG_PC;
        TSTATE += 19;
        break;

    default:                   /* oops, unkown interrupt mode... */
        break;
    }

    z80_state.running = true;
    z80_state.iff1 = false;

    if (tracing(TRACE_CPU | TRACE_IO)) {
        fprintf(tracef, "[%12" PRIu64 "] INT: "
                "im%u vector 0x%02x (%3d) I=%02x PC=%04x -> %04x\n",
		when, z80_state.interrupt_mode,
                i_vector, i_vector, REG_I, old_pc, REG_PC);
    }

    rfsh();
}

static void do_reset(void)
{
    REG_PC = 0;
    REG_I  = 0;			/* REG_IR? */
    REG_WZ = 0;
    z80_state.iff1 = false;
    z80_state.iff2 = false;
    z80_state.ei_shadow = false;
    z80_state.q = false;
    z80_state.interrupt_mode = 0;
    z80_state.nmi_in_progress = false;
    z80_state.signal_eoi = false;
    z80_state.running = true;
    atomic_fetch_and(&z80_state.uncond, ~(UCEV_NMI|UCEV_RESET));
}

struct dump_type {
    unsigned int event;
    const char *memspace;
};
static const struct dump_type memdumps[] = {
    { UCEV_DUMP_MEM,  "cpu" },
    { UCEV_DUMP_RAM,  "ram" },
    { UCEV_DUMP_XMEM, "xmem" },
};

/* Check for an unconditional event (NMI, reset) */
static enum z80_cond check_cpu_events(void)
{
    unsigned int ucevent;
    enum z80_cond cond = 0;

    ucevent = atomic_load(&z80_state.uncond);

    if (unlikely(ucevent)) {
	if (unlikely(ucevent & UCEV_DUMP_MASK)) {
	    size_t i;
	    atomic_fetch_and(&z80_state.uncond, ~(ucevent & UCEV_DUMP_MASK));
	    for (i = 0; i < ARRAY_SIZE(memdumps); i++) {
		if (ucevent & memdumps[i].event)
		    dump_memory(memdumps[i].memspace);
	    }

	    if (ucevent & UCEV_DUMP_ALL)
		dump_all();

	    ucevent = atomic_load(&z80_state.uncond);
	}

	if (unlikely(ucevent & UCEV_RESET)) {
	    do_reset();
	    cond = Z80_RESET|Z80_RUNNING;
	} else if ((ucevent & UCEV_NMI) && !z80_state.nmi_in_progress) {
	    do_nmi();
	    cond = Z80_NMI|Z80_RUNNING;
	}
    }

    if (z80_state.iff1 && !z80_state.ei_shadow && poll_irq()) {
	do_int();
	cond = Z80_INT|Z80_RUNNING;
    }

    cond |= check_breakpoint(REG_PC);
    return cond;
}

/*
 * Common code for the CB bit operations
 */
static uint8_t do_shiftop(uint8_t op, uint8_t data)
{
    switch (op & 0x38) {
    case 0x00: return rlc_byte(data);
    case 0x08: return rrc_byte(data);
    case 0x10: return rl_byte(data);
    case 0x18: return rr_byte(data);
    case 0x20: return sla_byte(data);
    case 0x28: return sra_byte(data);
    case 0x30: return sll_byte(data);
    case 0x38: return srl_byte(data);
    default: abort();
    }
}

static uint8_t do_bitop(uint8_t op, uint8_t data)
{
    const unsigned int bit = (op >> 3) & 7;

    switch (op >> 6) {
    case 0: return do_shiftop(op, data);
    case 1: return do_test_bit(data, bit); /* bit */
    case 2: return data &= ~(1 << bit);	   /* res */
    case 3: return data |= 1 << bit;	   /* set */
    default: abort();
    }
}

/*
 * CB prefixed instruction, without DD/FD
 */
static void do_CB_noix(void)
{
    uint8_t op, data;

    /*
     * The sub-opcode is loaded using a normal M1# cycle.
     */
    op = fetch_m1();
    TSTATE += 4;

    if ((op & 7) == 6)
	TSTATE += 4;	       /* For the load */

    data = get8noix(op);
    data = do_bitop(op, data);

    if ((op >> 6) == 1)	       /* BIT */
	return;		       /* Skip writeback */

    if ((op & 7) == 6)
	TSTATE += 3;	       /* For the store */

    set8noix(op, data);
}

/*
 * Handle DD and FD prefixes
 */
static inline void clear_indexing(void)
{
    IXREG = Z80_HL;
    IXDISP = 0;
}

static uint8_t start_indexed_insn(enum z80_regnums reg)
{
    /*
     * This is a bitmask for which primary opcode bytes take a displacement
     * byte after an IX/IY prefix
     */
    static const uint32_t need_disp[8] = {
	/* 00-3f */ 0x00000000, 0x00700000, /* only 34-36 */
	/* 40-7f */ 0x40404040, 0x40bf4040,
	/* 80-bf */ 0x40404040, 0x40404040,
	/* c0-ff */ 0x00000800, 0x00000000  /* only CB */
    };
    uint8_t op;

    IXREG = reg;
    op = fetch_m1();

    if (need_disp[op >> 5] & ((uint32_t)1 << (op & 31))) {
	IXDISP = fetch_byte();
	TSTATE += 8;	       /* 3+5 T-states for two machine cycles */
    }

    return op;
}

/*
 * Extended instructions which have DD/FD as the first byte
 * and CB as the second byte:
 */
static void do_CB_ixiy(void)
{
    /*
     * Indexed CB instructions are weird.  They ALWAYS take the source from
     * (Ix+d) and ALWAYS write the result back, but ALSO write the result
     * to a GPR unless the register specifier is 6.  BIT never writes
     * anything back to either memory or GPR.
     *
     * Futhermore, the displacement comes *before* the instruction,
     * (the displacement has already been fetched when we get here),
     * and the instruction is loaded using a normal memory read, as if
     * it were an immediate, rather than an M1# cycle.
     */
    uint8_t op, data;

    op = fetch_byte();
    TSTATE += 4;

    data = read_byte_hlix();
    data = do_bitop(op, data);
    if ((op >> 6) == 1) {	       /* BIT */
	set_flags((REG_F & ~XY_MASK) | ((hlix_addr() >> 8) & XY_MASK));
	return;
    }

    TSTATE += 3;
    write_byte_hlix(data);
    set8reg(op, data);
}

static void do_CB_instruction(void)
{
    if (IXREG == Z80_HL)
	do_CB_noix();
    else
	do_CB_ixiy();
}

static void do_ED_instruction(void)
{
    uint8_t op;
    regpair *rp;

    /* DD/FD has no effect on ED-prefixed instructions */
    clear_indexing();

    /*
     * Undocumented instruction notes:
     * ED 00-3F = NOP
     * ED 80-BF = NOP unless documented
     * ED C0-FF = NOP
     * ED 40-7F duplicate:
     *   NEG       at ED4C, ED54, ED5C, ED64, ED6C, ED74, ED7C
     *   NOP       at ED77, ED7F
     *   RETN      at ED45, ED55, ED65, ED75, ED5D, ED6D, ED7D
     *   RETI      at ED4D (same as RETN for the CPU itself)
     *   IM ?      at ED4E, ED6E
     *   IM 0      at ED66
     *   IM 1      at ED76
     *   IM 2      at ED7E
     *   IN F,(C)  at ED70
     *   OUT (C),0 at ED71  -- OUT (C),0FFh for CMOS Z80
     */

    op = fetch_m1();
    TSTATE += clk_ED[op];

    rp = get_rp(op);

    switch (op) {
    CASE4rp(0x4A):	       /* adc hl, rp */
        REG_HL = do_adc_word_flags(REG_HL, rp->w);
        break;

    case 0x46:                 /* im 0 */
    case 0x66:
    case 0x4E:
    case 0x6E:
        do_im0();
        break;
    case 0x56:                 /* im 1 */
    case 0x76:
        do_im1();
        break;
    case 0x5E:                 /* im 2 */
    case 0x7E:
        do_im2();
        break;

    CASE8x(0x40):	       /* in xx, (c) */
	set8reg(op >> 3, in_with_flags(REG_BC));
        REG_WZ = REG_BC + 1;
        break;

    case 0x57:                 /* ld a, i */
        do_ld_a_ir(REG_I);
        break;
    case 0x47:                 /* ld i, a */
        REG_I = REG_A;
        break;

    case 0x5F:                 /* ld a, r */
        do_ld_a_ir(REG_R);
        break;
    case 0x4F:                 /* ld r, a */
	REG_R = REG_A;
        break;

    CASE4rp(0x4B):	       /* ld rp, (address) */
        rp->w = direct_word();
        break;

    CASE4rp(0x43):	       /* ld (address), rp */
    {
        uint16_t address = fetch_word();
        mem_write_word(address, rp->w);
        REG_WZ = address + 1;
        break;
    }


    CASE8x(0x44):	       /* neg */
        do_negate();
        break;

    CASE8x(0x41):	       /* out (c), xx */
	z80_out(REG_BC, get8reg(op >> 3));
        REG_WZ = REG_BC + 1;
	break;

    case 0xA8:                 /* ldd */
    case 0xB8:                 /* lddr */
    case 0xA0:                 /* ldi */
    case 0xB0:                 /* ldir */
        do_ldid(op);
        break;
    case 0xAA:                 /* ind */
    case 0xBA:                 /* indr */
    case 0xA2:                 /* ini */
    case 0xB2:                 /* inir */
        do_inid(op);
        break;
    case 0xAB:                 /* outd */
    case 0xBB:                 /* outdr */
    case 0xA3:                 /* outi */
    case 0xB3:                 /* outir */
        do_outid(op);
        break;
    case 0xA9:                 /* cpd */
    case 0xB9:                 /* cpdr */
    case 0xA1:                 /* cpi */
    case 0xB1:                 /* cpir */
        do_cpid(op);
	break;

    CASE8x(0x45):	       /* RETI/RETN */
	if (op == 0x4D) {
	    /*
	     * RETI detection logic for EOI is external to the CPU
	     * Some sources say any opcode of the format [4567]D is
	     * detected as RETI, but it is hard to know for sure what
	     * any specific peripheral will do.
	     */
	    z80_state.signal_eoi = true;
	}
        REG_PC = pop();
	REG_WZ = REG_PC;
	z80_state.iff1 = z80_state.iff2;
        z80_state.nmi_in_progress = false;
        break;

    case 0x6F:                 /* rld */
        do_rld();
        break;

    case 0x67:                 /* rrd */
        do_rrd();
        break;

    CASE4rp(0x42):	       /* sbc hl, rp */
        HLIX.w = do_sbc_word_flags(HLIX.w, rp->w);
	break;

    default:
        /* Assume all others are NOP */
        break;
    }
}

static inline void check_eoi(void)
{
    if (!likely(z80_state.signal_eoi))
        return;

    if (tracing(TRACE_IO)) {
        fprintf(tracef, "[%12" PRIu64 "] EOI: RETI executed\n", TSTATE);
    }

    z80_state.signal_eoi = false;
    z80_eoi();
}

/*
 * This resolves flag condition codes based on the opcode. The flag
 * selected is determined by bits [5:4] of the opcode, but in a rather
 * non-obvious manner, and bit 3 is 1 if the flag is expected to be set.
 */
static bool flagcond(uint8_t op)
{
    switch ((op >> 3) & 7) {
    case 0: return !ZERO_FLAG;
    case 1: return !!ZERO_FLAG;
    case 2: return !CARRY_FLAG;
    case 3: return !!CARRY_FLAG;
    case 4: return !PARITY_FLAG;
    case 5: return !!PARITY_FLAG;
    case 6: return !SIGN_FLAG;
    case 7: return !!SIGN_FLAG;
    default: abort();
    }
}

/*
 * Various branch conditions. From all I can manage to determine, the
 * conditional operations always perform the immediate read. However,
 * RET does not read the stack unless it is actually executed.
 *
 * These return the condition as a convenience to the code below.
 */
static bool do_jp(bool cond)
{
    uint16_t address = fetch_word();
    REG_WZ = address;
    if (cond) {
	REG_PC = address;
	/* TSTATE += 0 */
    }
    return cond;
}

static bool do_jr(bool cond)
{
    int8_t disp = fetch_byte();
    if (cond) {
	REG_PC += disp;
	REG_WZ = REG_PC;
	TSTATE += 5;
    }
    return cond;
}

static bool do_call(bool cond)
{
    uint16_t address = fetch_word();
    REG_WZ = address;
    if (cond) {
	push(REG_PC);
	REG_PC = address;
	TSTATE += 7;
    }
    return cond;
}

/* Note: a conditional RET takes an extra cycle during M1; not handled here */
static bool do_ret(bool cond)
{
    if (cond) {
	REG_PC = pop();
	REG_WZ = REG_PC;
	TSTATE += 6;
    }
    return cond;
}

enum z80_cond z80_run(enum z80_cond condrq)
{
    uint8_t op;
    enum z80_cond cond;
    bool call_ret;

    cond = z80_state.running ? Z80_RUNNING : 0;
    z80_state.brkpt = 0;	/* No breakpoints hit */

    /* loop to do a z80 instruction */
    do {
	/*
	 * Save the values at the top of the instruction, useful
	 * for tracing/debugging
	 */
	REG_LAST_PC = REG_PC;
	TSTATE_INIT = TSTATE;

        check_eoi();

	for (;;) {
            /* Poll for external event */
	    cond |= z80_poll_external();
	    if (cond & condrq)
		return cond;

            /* Check for an interrupt or reset */
	    cond |= check_cpu_events();
            z80_state.ei_shadow = false;
	    if (cond & condrq)
		return cond;

            if (cond & Z80_RUNNING)
                break;

	    /* Halt cycle */
            TSTATE += 4;
	    cond |= Z80_STEP;

            if (cond & condrq)
		return cond;
        }

        if (tracing(TRACE_CPU | TRACE_CALL)) {
            add_cputrace("[%12" PRIu64 "] PC=%04X ",TSTATE, REG_PC);
	    DAsm(REG_PC, traceline+tracelinelen, NULL);
	    tracelinelen = strlen(traceline);
	    traceline[tracelinelen++] = ' ';
        }

	clear_indexing();
	call_ret = false;		/* Not a CALL, RET, or RST */

	/*
	 * This has to be done as a peek due to the software breakpoint
	 * check below, so can't use fetch_m1() here...
	 */
        op = fetch_m1_peek();

	/*
	 * Software breakpoint check; do this early to avoid
	 * needless state changes.
	 *
	 * This is completely artificial, to improve debugging.
	 */
	if (op == 0x64) { /* LD H,H */
	    cond |= Z80_SWBRK;
	} else if (!(uint8_t)(~op & ~0x38)) { /* RST xx */
	    cond |= Z80_RST00 << ((op >> 3) & 7);
	}
	if (cond & condrq)
	    return cond;

	z80_state.q = false;
	fetch_m1_commit();

    indexed:
        TSTATE += clk_main[op];

	/* Pick quadrant */
	switch (op >> 6) {
	case 0:
	{
	    regpair *rp = get_rp(op);

	    /* Quadrant 0:
	     * register pair operations, immediate, direct addressing,
	     * JR, and a few miscellaneous
	     */
	    switch (op) {
	    case 0x00:             /* nop */
		break;

	    CASE4rp(0x09):             /* add hl, rp */
		HLIX.w = do_add_word_flags(HLIX.w, rp->w);
		break;

	    case 0x2F:             /* cpl */
		REG_A = ~REG_A;
		set_flags((REG_F & ~XY_MASK) | (REG_A & XY_MASK) |
			  HALF_CARRY_MASK | SUBTRACT_MASK);
		break;

	    case 0x27:             /* daa */
		do_daa();
		break;

	    case 0x08:	       /* ex af, af' */
		SWAPW(REG_AF, REG_AFx);
		z80_state.q = true;
		break;

	    CASE8x(0x04):      /* inc xx */
	    CASE8x(0x05):      /* dec xx */
		do_inc_dec_byte(op);
		break;

	    CASE4rp(0x03):     /* inc rp */
		rp->w++;
		break;

	    CASE4rp(0x0B):     /* dec rp */
		rp->w--;
		break;

	    case 0x18:             /* jr offset */
		do_jr(true);
		break;
	    case 0x20:             /* jr nz, offset */
	    case 0x28:             /* jr z, offset */
	    case 0x30:             /* jr nc, offset */
	    case 0x38:             /* jr c, offset */
		do_jr(flagcond(op & ~0x20));
		break;
	    case 0x10:             /* djnz offset */
		do_jr(--REG_B != 0);
		break;

	    case 0x02:             /* ld (bc), a */
		mem_write(rp->w, REG_A);
		REG_WZ = (REG_A << 8) | ((rp->w + 1) & 0xff);
		break;
	    case 0x12:             /* ld (de), a */
		mem_write(rp->w, REG_A);
		REG_WZ = (REG_A << 8) | ((rp->w + 1) & 0xff);
		break;

	    CASE8x(0x06):	       /* ld xx, value */
		set8(op >> 3, fetch_byte());
		break;

	    CASE4rp(0x01):             /* ld rp, value */
		rp->w = fetch_word();
		break;

	    case 0x3A:             /* ld a, (address) */
		REG_A = direct_byte();
		break;

	    case 0x0A:             /* ld a, (bc) */
		REG_A = mem_read(rp->w);
		REG_WZ = rp->w + 1;
		break;
	    case 0x1A:             /* ld a, (de) */
		REG_A = mem_read(rp->w);
		REG_WZ = rp->w + 1;
		break;

	    case 0x32:             /* ld (address), a */
            {
                uint16_t address = fetch_word();
		mem_write(address, REG_A);
		REG_WZ = (REG_A << 8) | ((address + 1) & 0xff);
            }
		break;

	    case 0x22:             /* ld (address), hl */
            {
                uint16_t address = fetch_word();
		mem_write_word(address, HLIX.w);
		REG_WZ = address + 1;
            }
		break;

	    case 0x2A:             /* ld hl, (address) */
		HLIX.w = direct_word();
		break;

	    case 0x07:             /* rlca */
		do_rlca();
		break;

	    case 0x1F:             /* rra */
		do_rra();
		break;

	    case 0x0F:             /* rrca */
		do_rrca();
		break;

	    case 0x17:             /* rla */
		do_rla();
		break;

	    case 0x37:             /* scf */
		set_flags(((REG_F | CARRY_MASK) &
			   ~(SUBTRACT_MASK | HALF_CARRY_MASK | XY_MASK)) |
			  (REG_A & XY_MASK));
		break;

	    case 0x3F:             /* ccf */
		set_flags(((REG_F ^ CARRY_MASK) &
			   ~(SUBTRACT_MASK | XY_MASK)) | (REG_A & XY_MASK));
		break;
	    }
	    break;
	}
	case 1:		       /* ld xx, xx + HALT */
	{
	    /*
	     * Important: if there is a memory operand, the register
	     * operand ignores IX/IY.
	     */
	    uint8_t src = op & 7;
	    uint8_t dst = (op >> 3) & 7;
	    if (dst == 6) {
		if (unlikely(src == 6)) {
		    /* HALT - special case encoding */
		    z80_state.running = false;
		    cond &= ~Z80_RUNNING;
		    cond |= Z80_HALT;
		} else {
		    write_byte_hlix(get8reg(src));
		}
	    } else if (src == 6) {
		set8reg(dst, read_byte_hlix());
	    } else {
		set8(dst, get8(src));
	    }
	    break;
	}
	case 2:		       /* 8-bit arithmetic */
	{
	    do_arith_byte(op, get8(op));
	    break;
	}
	case 3:
	{
	    /*
	     * Quadrant 3: miscellaneous, jp, call, ret, push/pop
	     */
	    regpair *rp = get_rp_af(op);

	    switch (op) {
	    case 0xDD:	       /* DD.. extended instruction */
		z80_state.q = false;
		op = start_indexed_insn(Z80_IX);
		goto indexed;
	    case 0xFD:	       /* FD.. extended instruction */
		z80_state.q = false;
		op = start_indexed_insn(Z80_IY);
		goto indexed;

	    case 0xCB:	       /* CB.. extended instruction */
		do_CB_instruction();
		break;
	    case 0xED:	       /* ED.. extended instruction */
		do_ED_instruction();
		break;

	    CASE8x(0xC6):      /* arith a, imm */
		do_arith_byte(op, fetch_byte());
		break;

	    case 0xCD:	       /* call address */
		call_ret = do_call(true);
		break;
	    CASE8x(0xC4):       /* call cond, address */
		call_ret = do_call(flagcond(op));
		break;

	    case 0xF3:	       /* di */
		do_di();
		break;

	    case 0xFB:	       /* ei */
		do_ei();
		break;

	    case 0xEB:	       /* ex de, hl */
		SWAPW(REG_DE, HLIX.w);
		break;

	    case 0xE3:	       /* ex (sp), hl */
            {
                uint16_t temp;
                temp = mem_read_word(REG_SP);
                mem_write_word(REG_SP, HLIX.w);
                HLIX.w = temp;
                REG_WZ = temp;
            }
            break;

	    case 0xD9:	       /* exx */
		SWAPW(REG_BC, REG_BCx);
		SWAPW(REG_DE, REG_DEx);
		SWAPW(REG_HL, REG_HLx);
		break;

	    case 0xDB:	       /* in a, (port) */
            {
                uint8_t a = REG_A;
		REG_A = z80_in((a << 8) + fetch_byte());
		REG_WZ = (a << 8) | (REG_A + 1);
            }
		break;

	    case 0xC3:	       /* jp address */
		do_jp(true);
		break;
	    CASE8x(0xC2):      /* jp cond, address */
		do_jp(flagcond(op));
		break;
	    case 0xE9:	       /* jp (hl) */
		REG_PC = HLIX.w;
		REG_WZ = REG_PC;
		break;

	    case 0xF9:	       /* ld sp, hl */
		REG_SP = HLIX.w;
		break;

	    case 0xD3:	       /* out (port), a */
            {
                uint8_t port = fetch_byte();
		z80_out((REG_A << 8) + port, REG_A);
		REG_WZ = (REG_A << 8) | (port + 1);
            }
		break;

	    CASE4rp(0xC1):     /* pop rp */
		rp->w = pop();
		break;

	    CASE4rp(0xC5):     /* push rp */
		push(rp->w);
		break;

	    case 0xC9:	       /* ret */
		call_ret = do_ret(true);
		break;
	    CASE8x(0xC0):      /* ret cond */
		call_ret = do_ret(flagcond(op));
		break;

	    CASE8x(0xC7):      /* rst nn */
		push(REG_PC);
		REG_PC = op & 0x38;
		REG_WZ = REG_PC;
		call_ret = true;
		break;
	    }
	    break;
        }
	}

	/* Executed an instruction, also add watchpoints */
	cond |= z80_state.brkpt | Z80_STEP;

        if (tracing(TRACE_CPU | TRACE_CALL)) {
	    if (tracing(TRACE_CPU)) {
		diffstate();
		fwrite(traceline, 1, tracelinelen, tracef);
		tracemem();
		fputc('\n', tracef);
	    } else if (call_ret || z80_state.was_call_ret) {
		fwrite(traceline, 1, tracelinelen, tracef);
		fputc('\n', tracef);
	    }
	    tracelinelen = 0;
	    z80_state.was_call_ret = call_ret;
	    if (call_ret)
		z80_dumpregs(tracef, "               - ");
        }
    } while (!(cond & condrq));

    return cond;
}

static const char *flagdis(uint8_t f)
{
    static const char flags[] = "SZYHXPNC";
    static char buf[16], *bp;
    int i;
    unsigned int fx = f;

    bp = buf;

    for (i = 0; i < 8; i++) {
	*bp++ = (fx & 0x80) ? flags[i] : '-';
	fx <<= 1;
    }
    *bp = '\0';

    return buf;
}

#define WREG(U,L)						\
    if (z80_state.L.w != old_state.L.w) {			\
	add_cputrace(" %s=%04X", U, z80_state.L.w);		\
	old_state.L.w = z80_state.L.w;				\
    }
#define BREG(U,L)						\
    if (z80_state.L != old_state.L) {				\
	add_cputrace(" %s=%02X", U, z80_state.L);		\
	old_state.L = z80_state.L;				\
    }
#define FREG(U,L)						\
    if (z80_state.L != old_state.L) {				\
	add_cputrace(" %s=%s(%02X)", U,				\
		     flagdis(z80_state.L), z80_state.L);	\
	old_state.L = z80_state.L;				\
    }

static void diffstate(void)
{
    static struct z80_state_struct old_state;

    BREG("A", af.b.h);
    WREG("BC", bc);
    WREG("DE", de);
    WREG("HL", hl);
    WREG("IX", ix);
    WREG("IY", iy);
    WREG("SP", sp);
    WREG("WZ", wz);
    /* WREG(PC,pc); */
    FREG("F", af.b.l);
    WREG("AF\'", afx);
    WREG("BC\'", bcx);
    WREG("DE\'", dex);
    WREG("HL\'", hlx);
    BREG("I", ir.b.h);
}

void z80_dumpregs(FILE *f, const char *prefix)
{
    if (prefix) {
	/* Compact form */
	fprintf(f, "%sBC=%04X DE=%04X HL=%04X IX=%04X IY=%04X SP=%04X\n"
		"%sA=%02X F=%s(%02X) I=%02X R=%02X BC\'=%04X DE\'=%04X HL\'=%04X AF\'=%04X\n",
		prefix, REG_BC, REG_DE, REG_HL, REG_IX, REG_IY, REG_SP,
		prefix, REG_A, flagdis(REG_F), REG_F, REG_I, REG_R,
		REG_BCx, REG_DEx, REG_HLx, REG_AFx);
    } else {
	/* Extended form */
	fprintf(f,
		"PC  = 0x%04x   %5u   %3u:%3u\n"
		"SP  = 0x%04x   %5u   %3u:%3u\n"
		"BC  = 0x%04x   %5u   %3u:%3u\n"
		"DE  = 0x%04x   %5u   %3u:%3u\n"
		"HL  = 0x%04x   %5u   %3u:%3u\n"
		"AF  = 0x%04x           %3u:%3u  %s\n"
		"IX  = 0x%04x   %5u   %3u:%3u\n"
		"IY  = 0x%04x   %5u   %3u:%3u\n"
		"WZ  = 0x%04x   %5u   %3u:%3u\n"
		"IR  = 0x%02x%02x           %3u:%3u\n",
		REG_PC, REG_PC, z80_state.pc.b.h, z80_state.pc.b.l,
		REG_SP, REG_SP, z80_state.sp.b.h, z80_state.sp.b.l,
		REG_BC, REG_BC, REG_B, REG_C,
		REG_DE, REG_DE, REG_D, REG_E,
		REG_HL, REG_HL, REG_H, REG_L,
		REG_AF, REG_A, REG_F, flagdis(REG_F),
		REG_IX, REG_IX, REG_IXH, REG_IXL,
		REG_IY, REG_IY, REG_IYH, REG_IYL,
		REG_WZ, REG_WZ, z80_state.wz.b.h, z80_state.wz.b.l,
		REG_I, REG_R, REG_I, REG_R);
	fprintf(f,
		"BC' = 0x%04x   %5u   %3u:%3u\n"
		"DE' = 0x%04x   %5u   %3u:%3u\n"
		"HL' = 0x%04x   %5u   %3u:%3u\n"
		"AF' = 0x%04x           %3u:%3u  %s\n",
		REG_BCx, REG_BCx, z80_state.bcx.b.h, z80_state.bcx.b.l,
		REG_DEx, REG_DEx, z80_state.dex.b.h, z80_state.dex.b.l,
		REG_HLx, REG_HLx, z80_state.hlx.b.h, z80_state.hlx.b.l,
		REG_AFx, z80_state.afx.b.h, z80_state.afx.b.l, flagdis(z80_state.afx.b.l));
	fprintf(f,
		"\n"
		"Tstate = %" PRIu64 "\n"
		"Clock  = %0.6f s\n",
		TSTATE,
		TSTATE*ns_per_tstate*1.0e-9);
    }
}
