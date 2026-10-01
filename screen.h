#ifndef _SCREEN_H
#define _SCREEN_H

#include "compiler.h"

#include <SDL3/SDL.h>

extern void screen_init(bool, bool);
extern void screen_reset(void);
extern void screen_write(int, int);
extern void screen_flush(void);
extern void trigger_screen_refresh(void);
extern void setmode40(bool);

extern void event_loop(void);
extern void key_check(void);

extern volatile int event_pending;
Uint32 post_periodic(Uint32 interval, void *param);

extern void crtc_out(uint16_t, uint8_t);
extern pure_func uint8_t crtc_in(uint16_t);

extern void fg_out(uint16_t, uint8_t);

extern void do_magic(int);
extern void do_quit(void);
extern void enable_real_keyboard(void);

extern void abc_screenshot(const char *path);
extern void dump_txt_screen(const char *path, const char *file);

/* Pointer to video RAM (text and graphics, respectively) */
extern uint8_t *const video_ram;
extern uint8_t *const fgram;

extern const uint8_t fgcolor[128][4];

#endif /* _SCREEN_H */
