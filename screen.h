#ifndef _SCREEN_H
#define _SCREEN_H

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#ifdef HAVE_SDL_H
# include <SDL.h>
#elif defined(HAVE_SDL_SDL_H)
# include <SDL/SDL.h>
#endif

extern void  screen_init(bool);
extern void  screen_reset(void);
extern void  screen_write(int, int);
extern void  screen_flush(void);
extern void  setmode40(bool);

extern void  get_event(void);
extern void  key_check(void);

extern volatile int event_pending;
Uint32 post_periodic(Uint32 interval, void *param);

extern void crtc_out(uint8_t, uint8_t);
extern uint8_t crtc_in(uint8_t);

extern uint8_t video_ram[];

#endif /* _SCREEN_H */
