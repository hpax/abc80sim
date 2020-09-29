#ifndef Z80COND_H
#define Z80COND_H

/*
 * Return values from z80_run(), also used by the breakpoint
 * routines.
 *
 * These flags are sticky and cleared on entry to z80_run(),
 * *except* for Z80_RUNNING which is part of the CPU state.
 * z80_run() exits if any of the bits in the passed-in mask
 * get set.
 *
 * Z80_STEP: set after completed execution of one instruction,
 * iteration of the repeating instruction, or HALT cycle.
 *
 * If break after reset/interrupt delivery is desired, immediately after
 * event delivery, need to also set Z80_BRK_{INT,NMI,RESET}
 *
 * Z80_BRK_{INT,NMI,RESET}: if set, z80_run() will exit immediately
 * after the event has been delivered, without executing further
 * instructions. Set on exit if event hit.
 *
 * Z80_BRKPT: set if the current PC triggers a code breakpoint, before
 * executing any instructions.
 *
 * Z80_{RD,WR,IN,OT}WPT: set if the immediately previous instruction
 * triggered a watchpoint; delivered *after* the event.
 *
 * Z80_RUNNING: CPU is not halted. This is part of the CPU state and
 * is not cleared on entry. If enabled as an exit event, it triggers
 * any time the CPU would exit halt.
 */
enum z80_cond {
    Z80_RUNNING      =    1,	/* Z80 is NOT halted (out) */
    Z80_STEP         =    2,	/* Single step request (in/out) */

    /* The following 5 *must* be in the low byte */
    Z80_BRKPT        =    4,	/* Hit a breakpoint) */
    Z80_RDWPT        =    8,	/* Hit a read memory watchpoint */
    Z80_WRWPT        =   16,	/* Hit a write memory watchpoint */
    Z80_INWPT        =   32,	/* Hit a read I/O watchpoint */
    Z80_OTWPT        =   64,    /* Hit a write I/O watchpoint */

    Z80_SWBRK        =  128,	/* Break on "LD H,H" = 0x64  */

    Z80_INT          =  256,       /* Exit on INT (in/out) */
    Z80_NMI          =  512,	/* Exit on NMI (in/out) */
    Z80_RESET        = 1024,	/* Exit on RESET (in/out) */
    Z80_HALT         = 2048,	/* Exit on HALT instruction (in/out) */
    Z80_QUIT         = 4096	/* External quit event */
};

#define Z80_BREAKPOINTS  (Z80_BRKPT|Z80_RDWPT|Z80_WRWPT|\
			  Z80_INWPT|Z80_OTWPT|Z80_SWBRK)
#define Z80_ALWAYS_BREAK (Z80_QUIT|Z80_BREAKPOINTS)

#endif /* Z80COND_H */
