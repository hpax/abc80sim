#ifndef Z80IRQ_H
#define Z80IRQ_H

#include "compiler.h"
#include "z80.h"

typedef int (*irq_func)(struct z80_irq *irq);

struct z80_irq {
    irq_func intack;
    irq_func eoi;
    void *pvt;			/* Available for user */
    int vector;			/* Available for user if intack defined */
    unsigned int prio;		/* Priority level */
};

#define MAX_IRQ 32

extern volatile unsigned int irq_pending;
static inline bool poll_irq(void)
{
    return unlikely(irq_pending != 0);
}

void z80_register_irq(struct z80_irq *irq);
int z80_intack(void);
void z80_eoi(void);
bool z80_interrupt(struct z80_irq *irq);
bool z80_clear_interrupt(struct z80_irq *irq);

#endif /* Z80IRQ_H */
