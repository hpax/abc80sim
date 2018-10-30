#include "compiler.h"
#include "z80.h"
#include "z80irq.h"

volatile unsigned int irq_pending;	/* Quick way to poll */
static struct z80_irq *irqs[MAX_IRQ];
static struct z80_irq *current_irq;

void z80_register_irq(struct z80_irq *irq)
{
    if (irq->prio < MAX_IRQ)
	irqs[irq->prio] = irq;
}

/*
 * Z80 interrupt acknowledge cycle. Return the vector from the highest
 * priority pending interrupt, or -1 if spurious.
 */
int z80_intack(void)
{
    int prio, vector;
    unsigned int irqmask;
    struct z80_irq *irq;

    do {
	/* Find the highest priority (lowest numeric) interrupt pending */
	irqmask = irq_pending;
	do {
	    if (!irqmask)
		return -1;		/* All interrupts went away... */

	    prio = __builtin_ctz(irqmask);
	} while (!cmpxchg(&irq_pending, &irqmask, irqmask & ~(1U << prio)));

	irq = irqs[prio];

	if (unlikely(irq->intack))
	    vector = irq->intack(irq);
	else
	    vector = irq->vector;
    } while (vector < 0);

    current_irq = irq;

    return vector;
}

/*
 * A RETI instruction was invoked, which is interpreted as an EOI.
 * In a real Z80 this is done by snooping the bus. Ick.
 */
void z80_eoi(void)
{
    struct z80_irq *irq = current_irq;

    if (!irq)
	return;			/* No known interrupt to EOI */

    current_irq = NULL;

    if (irq->eoi)
	irq->eoi(irq);
}

/*
 * Raise an interrupt with specific priority level; return true if
 * interrupt raised, false if the interrupt was already pending or
 * the argument is invalid.
 */
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

bool z80_interrupt(struct z80_irq *irq)
{
    bool raised;

    asm volatile("lock btsl %2,%0"
		 : "+m" (irq_pending), "=@ccnc" (raised)
		 : "rN" (irq->prio));

    return raised;
}

bool z80_clear_interrupt(struct z80_irq *irq)
{
    bool cleared;

    asm volatile("lock btrl %2,%0"
		 : "+m" (irq_pending), "=@ccc" (cleared)
		 : "rN" (irq->prio));

    return cleared;
}

#else

bool z80_interrupt(struct z80_irq *irq)
{
    unsigned int irqmask, irqpend;

    irqmask = 1U << irq->prio;
    irqpend = irq_pending;
    do {
	if (irqpend & irqmask)
	    return false;
    } while (!cmpxchg(&irq_pending, &irqpend, irqpend | irqmask));

    return true;
}

bool z80_clear_interrupt(struct z80_irq *irq)
{
    unsigned int irqmask, irqpend;

    irqmask = 1U << irq->prio;
    irqpend = irq_pending;
    do {
	if (!(irqpend & irqmask))
	    return false;
    } while (!cmpxchg(&irq_pending, &irqpend, irqpend & ~irqmask));

    return true;
}

#endif
