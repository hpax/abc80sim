#include "compiler.h"
#include "debug.h"
#include "z80.h"

uint8_t breakpoint_map[Z80_ADDRESS_LIMIT];

static struct breakpoint *brkpts;

#define for_each_breakpoint(brk) \
    for ((brk) = brkpts; (brk); (brk) = (brk)->next)

/* Mark applicable breakpoints as hit */
void breakpoint_hit(uint16_t address, uint16_t len, enum z80_cond type)
{
    struct breakpoint *brk;

    /*
     * Wraparound-safe calculation!
     * Note that more than one breakpoint can be hit at the same time.
     */
    for_each_breakpoint(brk) {
	uint16_t adelta = address - brk->addr;
	enum z80_cond hit = type & brk->type;
	if (type && adelta < len && adelta < brk->len) {
	    brk->hit |= hit;
	    z80_state.brkpt |= hit;
	}
    }
}

/* Return the first pending hit. */
struct breakpoint *breakpoint_get_hit(void)
{
    struct breakpoint *brk;

    for_each_breakpoint(brk) {
	if (brk->hit)
	    return brk;
    }

    return NULL;		/* No hits */
}

/* Recalculate the breakpoint_map */
static void brkpt_recalc_map(void)
{
    struct breakpoint *brk;

    memset(breakpoint_map, 0, sizeof breakpoint_map);

    for_each_breakpoint(brk) {
	size_t a = brk->addr;
	enum z80_cond type = brk->type;
	size_t i;

	for (i = 0; i < brk->len; i++) {
	    breakpoint_map[i+a] |= type;
	}
    }
}

/* Find a breakpoint by number */
struct breakpoint *breakpoint_find(unsigned int num)
{
    struct breakpoint *brk;

    for_each_breakpoint(brk) {
	if (brk->num == num)
	    return brk;
    }
    return NULL;
}

/*
 * Add, override, or remove (if type == 0) a breakpoint. If num == 0,
 * allocate a new index.
 */
struct breakpoint *
breakpoint_add(struct breakpoint *brk, unsigned int num,
	       enum z80_cond type, uint16_t addr, unsigned int len)
{
    static unsigned int breakpoint_ctr;

    if (len < 1)
	return NULL;

    if (len+addr >= Z80_ADDRESS_LIMIT)
	len = Z80_ADDRESS_LIMIT - addr;

    if (!type) {
	/* Deleting */
	if (!brk)
	    return NULL;	/* Nothing to do */

	*brk->back = brk->next;
	free(brk);
	brk = NULL;
    } else {
	/* Adding/overriding */
	if (!brk) {
	    if (!num)
		num = ++breakpoint_ctr;
	    brk = calloc(1, sizeof(*brk));
	    if (!brk)
		return NULL;
	    brk->next = brkpts;
	    brk->back = &brkpts;
	    if (brkpts)
		brkpts->back = &brk->next;
	    brkpts = brk;
	} else {
	    if (!num)
		num = brk->num;
	}

	brk->type = type & Z80_BREAKPOINTS;
	brk->num  = num;
	brk->hit  = 0;
	brk->addr = addr;
	brk->len  = len;
    }

    brkpt_recalc_map();
    return brk;
}

/*
 * Clear all breakpoints
 */
void breakpoint_clear_all(void)
{
    struct breakpoint *brk, *brkfree;

    brk = brkpts;
    while (brk) {
	brkfree = brk;
	brk = brk->next;
	free(brkfree);
    }

    brkpts = 0;
    memset(breakpoint_map, 0, sizeof breakpoint_map);

    z80_state.brkpt = 0;
}
