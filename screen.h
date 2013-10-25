#ifndef _SCREEN_H
#define _SCREEN_H

#include "config.h"

#ifdef HAVE_SDL_H
# include <SDL.h>
#elif defined(HAVE_SDL_SDL_H)
# include <SDL/SDL.h>
#endif

#ifndef MAIN
#define EXTERN extern
#else
#define EXTERN
#endif 

extern void  screen_init(void);
extern void  screen_reset(void);
extern void  screen_write(int, int);
extern void  screen_flush(void);
extern void  setmode40(int);

extern void  get_event(void);
extern void  key_check(void);

extern volatile int event_pending;
Uint32 post_periodic(Uint32 interval, void *param);

#endif /* _SCREEN_H */
