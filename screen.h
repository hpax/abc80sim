#ifndef _SCREEN_H
#define _SCREEN_H

#include "SDL.h"

#ifndef MAIN
#define EXTERN extern
#else
#define EXTERN
#endif 

typedef struct {
    int           r, c;        /* Location of the position on screen. */
    int           x, y;        /* Location of the position in the image. */
    unsigned char value;       /* Last ascii value stored in the point. */
    int           graph_mode;  /* True if position is in graphic mode */
} Screen_pos;

extern void  screen_init(void);
extern void  screen_reset(void);
extern void  screen_write(int, int);
extern void  screen_flush(void);

extern void  get_event(void);
extern void  key_check(void);

extern volatile int event_pending;
Uint32 post_periodic(Uint32 interval, void *param);

#endif /* _SCREEN_H */
