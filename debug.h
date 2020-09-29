#ifndef DEBUG_H
#define DEBUG_H

#include "compiler.h"
#include "z80cond.h"
#include "z80mem.h"

struct breakpoint {
    struct breakpoint *next;   /* Linked list */
    struct breakpoint **back;  /* Pointer to the link to this entry */
    unsigned int num;	       /* Numeric identifier */
    enum z80_cond type, hit;   /* Type, and if it has been hit */
    uint16_t addr, len;	       /* Starting address and length */
};

extern uint8_t breakpoint_map[Z80_ADDRESS_LIMIT];
extern void breakpoint_hit(uint16_t address, uint16_t len, enum z80_cond hit);

/* Check for code breakpoint */
static inline enum z80_cond check_breakpoint(uint16_t address)
{
    enum z80_cond hit = breakpoint_map[address] & Z80_BRKPT;
    if (unlikely(hit))
	breakpoint_hit(address, 1, hit);

    return hit;
}

/* Check for watchpoint. The loop should be eliminated. */
static inline void
check_watchpoint(uint16_t address, uint16_t len, enum z80_cond type)
{
    enum z80_cond hits = 0;
    uint16_t a = address;
    size_t i;

    for (i = 0; i < len; i++)
	hits |= breakpoint_map[a++] & type;

    if (unlikely(hits))
	breakpoint_hit(address, len, type);
}

static inline void
check_watchpoint_byte(uint16_t address, enum z80_cond type)
{
    check_watchpoint(address, 1, type);
}

static inline void
check_watchpoint_word(uint16_t address, enum z80_cond type)
{
    check_watchpoint(address, 2, type);
}

/* Return the first breakpoint with a pending hit */
extern struct breakpoint *breakpoint_get_hit(void);

/* Acknowledge a breakpoint hit */
static inline void breakpoint_clear_hit(struct breakpoint *brk)
{
    brk->hit = 0;
}

/* Add, override, or remove (if type == 0) a breakpoint */
extern struct breakpoint *
breakpoint_add(struct breakpoint *brk, unsigned int num,
	       enum z80_cond type, uint16_t addr, unsigned int len);

/* Remove all breakpoints */
void breakpoint_clear_all(void);

#endif /* DEBUG_H */
