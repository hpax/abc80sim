#ifndef _SCREEN_H
#define _SCREEN_H

#include "compiler.h"

#include <SDL.h>

extern void screen_init(bool, bool);
extern void screen_reset(void);
extern void screen_write(int, int);
extern void screen_flush(void);
extern void setmode40(bool);

extern void event_loop(void);
extern void key_check(void);

extern volatile int event_pending;
Uint32 post_periodic(Uint32 interval, void *param);

extern void crtc_out(uint16_t, uint8_t);
extern uint8_t crtc_in(uint16_t);

extern void fg_out(uint16_t, uint8_t);

extern void do_magic(int);
extern void do_quit(void);
extern void enable_real_keyboard(void);

/* Pointer to video RAM (text and graphics, respectively) */
extern uint8_t *const video_ram;
extern uint8_t *const fgram;

extern const uint8_t fgcolor[128][4];

#endif /* _SCREEN_H */
