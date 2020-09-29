#ifndef CLOCK_H
#define CLOCK_H

#include "compiler.h"

extern void timer_init(void);
extern void timer_poll(void);
extern void vsync_screen(void);

extern double ns_per_tstate;
extern double tstate_per_ns;
extern bool limit_speed;

extern atomic_bool z80_quit;

#endif /* CLOCK_H */
