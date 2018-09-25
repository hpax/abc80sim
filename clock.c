#include "z80.h"
#include "screen.h"
#include "abcio.h"
#include "clock.h"
#include "nstime.h"

static void abc80_clock_tick(void);
static void abc800_clock_tick(void);
static struct abctimer *ctc_timer[4];

/*
 * Initialize the time for next event
 */
struct abctimer {
  uint64_t last;
  uint64_t period;
  void (*func)(void);
};

#define MAX_TIMERS 2
static struct abctimer timers[MAX_TIMERS];
static int ntimers;

static struct abctimer *create_timer(uint64_t period, void (*func)(void))
{
  struct abctimer *t;

  if (ntimers >= MAX_TIMERS)
    abort();

  t = &timers[ntimers++];

  t->period = period;
  t->func = func;
  t->last = nstime();

  return t;
}

void timer_init(void)
{
  switch (model) {
  case MODEL_ABC80:
    /* 20 ms = 50 Hz */
    create_timer(20000000, abc80_clock_tick);
    break;
  case MODEL_ABC802:
    /* 10.67 ms = 93.75 Hz */
    ctc_timer[3] = create_timer(10666667, abc800_clock_tick);

    /* 20 ms = 50 Hz */
    create_timer(20000000, abc802_vsync);
    break;
  }
}

static inline bool trigger(uint64_t now, struct abctimer *tmr)
{
  /* This expression: a) will overflow safely, b) will never trigger for 0 */
  if (likely((now - tmr->last) <= (tmr->period - 1)))
    return false;

  tmr->last += tmr->period;
  if (unlikely((now - tmr->last) >= tmr->period)) {
    /* Missed tick(s), advance clock to skip missed */
    tmr->last = now - ((now - tmr->last) % tmr->period);
  }

  return true;
}

/* Poll for timers - these the only external event we look for */
void z80_poll_external(void)
{
  uint64_t now = nstime();
  struct abctimer *t = timers;
  int i;

  for (i = 0; i < ntimers; i++) {
    if (trigger(now, t))
      t->func();

    t++;
  }
}

/*
 * ABC80: Trig a non maskable interrupt in the Z80 on the clock signal.
 */
static void abc80_clock_tick(void)
{
  vsync_screen();		/* Also vertical retrace */
  z80_nmi();
}

static uint8_t ctc_ctl[4], ctc_div[4], ctc_vector;

/*
 * ABC800: Clock interrupt through the CTC
 */

static uint8_t ctc_ctl[4], ctc_div[4], ctc_vector;

static void abc800_clock_tick(void)
{
  if ((ctc_ctl[3] & 0xc0) == 0x80)
    z80_interrupt(ctc_vector | (3 << 1)); /* 3 = channel */
}

/*
 * CTC I/O
 */
void abc800_ctc_out(uint8_t port, uint8_t v)
{
  if ((v & 1) == 0) {
    ctc_vector = v;
    return;
  }

  port &= 3;			/* Get channel */

  if (ctc_ctl[port] & 4) {
    ctc_div[port] = v;
    ctc_ctl[port] &= ~4;
    return;
  }

  if (v & 2)
    v = 1;			/* Reset channel */

  ctc_ctl[port] = v;
}

uint8_t abc800_ctc_in(uint8_t port)
{
  uint8_t v;

  switch (port & 3) {
  case 0:
  case 1:
  case 2:
    v = 0xff;
    break;

  case 3:
    {
      uint64_t now = nstime();
      v = ((ctc_timer[3]->last + ctc_timer[3]->period - now) * ctc_div[3])
	/ ctc_timer[3]->period;
      break;
    }
  }

  return v;
}
