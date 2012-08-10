#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#include "z80.h"
#include "screen.h"
#include "SDL.h"

/*
 * Trig a non maskable interrupt in the Z80 on the clock signal.
 */
static Uint32 clock_handler(Uint32 interval, void *param)
{
    z80_state.nminterrupt = 1;
    return interval;
}


/*
 * Set up signal handler and schedule a signal every 20 ms.
 */
void
clock_init(void)
{
    SDL_AddTimer(20, clock_handler, NULL);
}

/* Standard callback routine to post a periodic user event */
/* (int)param is the event code */
Uint32 post_periodic(Uint32 interval, void *param)
{
  SDL_Event event;
  event.type = SDL_USEREVENT;
  event.user.code  = 0;
  event.user.data1 = param;
  event.user.data2 = 0;

  if ( event_pending < 32 )
    SDL_PushEvent(&event);

  event_pending++;

  return interval;
}

