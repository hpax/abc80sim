#ifndef Z80_H
#define Z80_H

#include "compiler.h"

#define Z80_ADDRESS_LIMIT	(1 << 16)

#include "compiler.h"

#include <SDL.h>

#include "trace.h"
#include "z80irq.h"
#include "z80mem.h"
#include "z80cond.h"

struct twobyte {
#if WORDS_LITTLEENDIAN
    uint8_t l, h;
#else
    uint8_t h, l;
#endif
};

/* for implementing registers which can be seen as bytes or words: */
typedef union {
    uint16_t w;
    struct twobyte b;
} regpair;

typedef void (*eoifunc) (uint8_t, void *);
struct eoi {
    eoifunc func;
    void *arg;
    int trigger;                /* Vector to call EOI for, otherwise -1 */
};

struct z80_irq;

/* This is the register numbering expected by gdb */
enum z80_regnums {
    Z80_AF,  Z80_BC,  Z80_DE,  Z80_HL,
    Z80_SP,  Z80_PC,
    Z80_IX,  Z80_IY,
    Z80_AFx, Z80_BCx, Z80_DEx, Z80_HLx, /* AF' BC' DE' HL' */
    Z80_IR,
    Z80_LAST_PC,		/* PC of last started instruction */
    Z80_REG_NUM
};
struct z80_state_struct {
    union {
	struct {
	    regpair af;
	    regpair bc;
	    regpair de;
	    regpair hl;
	    regpair sp;
	    regpair pc;
	    regpair ix;
	    regpair iy;

	    regpair afx;
	    regpair bcx;
	    regpair dex;
	    regpair hlx;

	    regpair ir;

	    regpair last_pc;
	} r;
	uint16_t dbg[Z80_REG_NUM];
    } reg;



    uint8_t rctr;		/* counter part of REG_R */

    uint8_t interrupt_mode;
    bool iff1, iff2, ei_shadow, signal_eoi;
    bool nmi_in_progress;       /* to prevent multiple simultaneous NMIs */
    bool running;		/* CPU is running */
    enum z80_cond brkpt;	/* breakpoints triggered */

    atomic_uint uncond;		/* Unconditional events: NMI, reset */
    uint64_t tc;                /* T-state (clock cycle) counter */
};
extern struct z80_state_struct z80_state;

/* Unconditional events, can be triggered asynchronously */
enum uncond {
    UCEV_NMI         =  1,
    UCEV_RESET       =  2,
    UCEV_DUMP_MEM    =  4,
    UCEV_DUMP_RAM    =  8,
    UCEV_DUMP_XMEM   = 16,
    UCEV_ALL_DUMPS   = UCEV_DUMP_MEM|UCEV_DUMP_RAM|UCEV_DUMP_XMEM
};

/*
 * Register accessors:
 */

#define REG_A		z80_state.reg.r.af.b.h
#define REG_F		z80_state.reg.r.af.b.l
#define REG_B		z80_state.reg.r.bc.b.h
#define REG_C		z80_state.reg.r.bc.b.l
#define REG_D		z80_state.reg.r.de.b.h
#define REG_E		z80_state.reg.r.de.b.l
#define REG_H		z80_state.reg.r.hl.b.h
#define REG_L		z80_state.reg.r.hl.b.l

#define REG_IXH		z80_state.reg.r.ix.b.h
#define REG_IXL		z80_state.reg.r.ix.b.l
#define REG_IYH		z80_state.reg.r.iy.b.h
#define REG_IYL		z80_state.reg.r.iy.b.l

#define REG_SP		z80_state.reg.r.sp.w
#define REG_PC		z80_state.reg.r.pc.w
#define REG_LAST_PC	z80_state.reg.r.last_pc.w

#define REG_AF		z80_state.reg.r.af.w
#define REG_BC		z80_state.reg.r.bc.w
#define REG_DE		z80_state.reg.r.de.w
#define REG_HL		z80_state.reg.r.hl.w

#define REG_AFx		z80_state.reg.r.afx.w
#define REG_BCx		z80_state.reg.r.bcx.w
#define REG_DEx		z80_state.reg.r.dex.w
#define REG_HLx		z80_state.reg.r.hlx.w

#define REG_IX		z80_state.reg.r.ix.w
#define REG_IY		z80_state.reg.r.iy.w

#define REG_IR		z80_state.reg.r.ir.w

#define REG_I		z80_state.reg.r.ir.b.h
#define REG_R		z80_state.reg.r.ir.b.l

#define TSTATE	z80_state.tc

/* Get/set the R register and update REG_R; this speeds up the counter */
static inline uint16_t z80_get_ir(void)
{
    return (REG_IR & ~0x7f) | (z80_state.rctr & 0x7f);
}

static inline uint8_t z80_get_r(void)
{
    return z80_get_ir();	/* R is just the low byte of IR */
}

static inline uint8_t z80_set_r(uint8_t val)
{
    return z80_state.rctr = REG_R = val;
}

/* Debugger register accessors */
static inline int z80_get_reg(unsigned int reg)
{
    REG_IR = z80_get_ir();	/* Update REG_R */
    return (reg >= Z80_REG_NUM) ? -1 : z80_state.reg.dbg[reg];
}

static inline int z80_set_reg(unsigned int reg, uint16_t val)
{
    if (reg >= Z80_REG_NUM)
	return -1;

    z80_state.reg.dbg[reg] = val;
    z80_set_r(REG_R);		/* We might have changed REG_R */
    return 0;
}

/*
 * Flag accessors:
 *
 * Flags are:
 *
 *	7   6   5   4   3   2   1   0
 *	S   Z   -   H   -  P/V  N   C
 *
 *	C	Carry
 *	N	Subtract
 *	P/V	Parity/Overflow
 *	H	Half-carry
 *	Z	Zero
 *	S	Sign
 */

#define CARRY_MASK		(0x1)
#define SUBTRACT_MASK		(0x2)
#define PARITY_MASK		(0x4)
#define OVERFLOW_MASK		(0x4)
#define HALF_CARRY_MASK		(0x10)
#define ZERO_MASK		(0x40)
#define	SIGN_MASK		(0x80)
#define ALL_FLAGS_MASK		(CARRY_MASK | SUBTRACT_MASK | OVERFLOW_MASK | \
				 HALF_CARRY_MASK | ZERO_MASK | SIGN_MASK)

#define SET_SIGN()		(REG_F |= SIGN_MASK)
#define CLEAR_SIGN()		(REG_F &= (~SIGN_MASK))
#define SET_ZERO()		(REG_F |= ZERO_MASK)
#define CLEAR_ZERO()		(REG_F &= (~ZERO_MASK))
#define SET_HALF_CARRY()	(REG_F |= HALF_CARRY_MASK)
#define CLEAR_HALF_CARRY()	(REG_F &= (~HALF_CARRY_MASK))
#define SET_OVERFLOW()		(REG_F |= OVERFLOW_MASK)
#define CLEAR_OVERFLOW()	(REG_F &= (~OVERFLOW_MASK))
#define SET_PARITY()		(REG_F |= PARITY_MASK)
#define CLEAR_PARITY()		(REG_F &= (~PARITY_MASK))
#define SET_SUBTRACT()		(REG_F |= SUBTRACT_MASK)
#define CLEAR_SUBTRACT()	(REG_F &= (~SUBTRACT_MASK))
#define SET_CARRY()		(REG_F |= CARRY_MASK)
#define CLEAR_CARRY()		(REG_F &= (~CARRY_MASK))

#define SIGN_FLAG		(REG_F & SIGN_MASK)
#define ZERO_FLAG		(REG_F & ZERO_MASK)
#define HALF_CARRY_FLAG		(REG_F & HALF_CARRY_MASK)
#define OVERFLOW_FLAG		(REG_F & OVERFLOW_MASK)
#define PARITY_FLAG		(REG_F & PARITY_MASK)
#define SUBTRACT_FLAG		(REG_F & SUBTRACT_MASK)
#define CARRY_FLAG		(REG_F & CARRY_MASK)
#define SIGN_FLAG		(REG_F & SIGN_MASK)


/* Trigger a unconditional event, asynchonously */
static inline void z80_trigger_uncond(enum uncond ev)
{
    atomic_fetch_or(&z80_state.uncond, ev);
}

/* Signal a RESET */
static inline void z80_reset(void)
{
    z80_trigger_uncond(UCEV_RESET);
}

/* Signal an NMI */
static inline void z80_nmi(void)
{
    z80_trigger_uncond(UCEV_NMI);
}

/* Disassembler */
extern int disassemble(int);
extern int DAsm(uint16_t pc, char *T, int *target);

/* Main loop */
extern enum z80_cond z80_run(enum z80_cond);

#endif /* Z80_H */
