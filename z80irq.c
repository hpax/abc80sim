#include "compiler.h"
#include "z80.h"
#include "z80irq.h"

volatile unsigned int irq_pending;	/* Quick way to poll */
unsigned int irq_mask = ~0U;
static struct z80_irq *irqs[MAX_IRQ];

void z80_register_irq(struct z80_irq *irq)
{
    if (irq->prio < MAX_IRQ) {
	irqs[irq->prio] = irq;
    }
}

/*
 * Z80 interrupt acknowledge cycle. Return the vector from the highest
 * priority pending interrupt, or -1 if spurious.
 */
int z80_intack(void)
{
    int prio, vector;
    unsigned int irqpend, irqmask, priomask;
    struct z80_irq *irq;

    do {
	/* Find the highest priority (lowest numeric) interrupt pending */
	irqpend = irq_pending;
	do {
	    irqmask = irqpend & irq_mask;
	    if (!irqmask)
		return -1;		/* All interrupts went away... */

	    prio = __builtin_ctz(irqmask);
	    priomask = ~(1U << prio);
	} while (!cmpxchg(&irq_pending, &irqpend, irqpend & priomask));

	irq = irqs[prio];

	if (unlikely(irq->intack))
	    vector = irq->intack(irq);
	else
	    vector = irq->vector;
    } while (vector < 0);

    /* Inside the handler for this interrupt */
    irq_mask &= priomask;
    irq->handled = true;

    return vector;
}

/*
 * A RETI instruction was invoked, which is interpreted as an EOI.
 * In a real Z80 this is done by snooping the bus.
 * If somehow multiple interrupts are pending, as the RETI
 * is broadcast, all devices will EOI if they want to, or not.
 */
void z80_eoi(void)
{
    unsigned int nirqmask;
    int prio;
    struct z80_irq *irq;

    while ((nirqmask = ~irq_mask)) {
	prio = __builtin_ctz(nirqmask);
	irq = irqs[prio];
	irq->handled = false;
	irq_mask |= 1U << prio;
	if (irq->eoi)
	    irq->eoi(irq);
    }
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
		 : "rN" ((unsigned int)irq->prio));

    return raised;
}

bool z80_clear_interrupt(struct z80_irq *irq)
{
    bool cleared;

    asm volatile("lock btrl %2,%0"
		 : "+m" (irq_pending), "=@ccc" (cleared)
		 : "rN" ((unsigned int)irq->prio));

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
