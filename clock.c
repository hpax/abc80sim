#include "z80.h"
#include "screen.h"
#include "abcio.h"
#include "clock.h"

/*
 * ABC80: Trig a non maskable interrupt in the Z80 on the clock signal.
 */
static Uint32 clock_nmi_handler(Uint32 interval, void *param)
{
    (void)param;
    z80_state.nminterrupt = 1;
    return interval;
}

static uint8_t ctc_vector, ctc_cmd[4]; /*ctc_div[4] */

/*
 * ABC800: Clock interrupt through the CTC
 */
static Uint32 clock_ctc_handler(Uint32 interval, void *param)
{
  static unsigned int skipper;
  int i;

  (void)param;
  (void)interval;

  if (skipper-- == 0)
    skipper = 2;

  for (i = 0; i < 4; i++) {
    if ((ctc_cmd[i] & 0xe0) == 0xe0) {
      z80_state.i_vector = ctc_vector | (i << 1);
      z80_state.interrupt = true;
      break;
    }
  }

  return skipper ? 11 : 10;	/* 11, 11, 10, 11, 11, 10... ms */
}

/*
 * Set up signal handler and schedule a signal every 20 ms.
 */
void
clock_init(void)
{
  switch (model) {
  case MODEL_ABC80:
    SDL_AddTimer(20, clock_nmi_handler, NULL); /* 20 ms NMI timer */
    break;
  case MODEL_ABC802:
    SDL_AddTimer(10, clock_ctc_handler, NULL); /* 10 ms CTC timer */
    break;
  }
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
