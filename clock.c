#include "z80.h"
#include "screen.h"
#include "abcio.h"
#include "clock.h"
#include "nstime.h"

static void abc80_clock_tick(void);
static void abc800_clock_tick(void);

/*
 * Initialize the time for next event
 */
struct abctimer {
  uint64_t last;
  uint64_t period;
};
static struct abctimer clock_timer, vsync_timer, blink_timer;
static void (*clock_tick)(void);

void timer_init(void)
{
  switch (model) {
  case MODEL_ABC80:
    clock_timer.period = 20000000;	/* 20 ms */
    clock_tick = abc80_clock_tick;
    break;
  case MODEL_ABC802:
    clock_timer.period = 10666667;	/* 10.67 ms = 93.75 Hz */
    clock_tick = abc800_clock_tick;
    vsync_timer.period = 20000000;
    break;
  }

  blink_timer.period = 400000000; /* 400 ms = 2.5 Hz */
  clock_timer.last = blink_timer.last = vsync_timer.last = nstime();
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

/* This returns the desired blink status */
bool timer_poll(void)
{
  static bool blink = true;
  uint64_t now = nstime();

  if (trigger(now, &clock_timer))
    clock_tick();

  if (trigger(now, &vsync_timer))
    abc802_vsync();

  if (trigger(now, &blink_timer))
    blink = !blink;

  return blink;
}

/*
 * ABC80: Trig a non maskable interrupt in the Z80 on the clock signal.
 */
static void abc80_clock_tick(void)
{
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
      v = ((clock_timer.last + clock_timer.period - now) * ctc_div[3])
	/ clock_timer.period;
      break;
    }
  }

  return v;
}
