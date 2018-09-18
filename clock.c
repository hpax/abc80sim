#include "z80.h"
#include "screen.h"
#include "abcio.h"
#include "clock.h"
#include "nstime.h"

/*
 * ABC80: Trig a non maskable interrupt in the Z80 on the clock signal.
 */
static void abc80_clock_tick(void)
{
  z80_nmi();
}

static uint8_t ctc_irq = -1;
static uint8_t ctc_cmd[4]; /*ctc_div[4] */

/*
 * ABC800: Clock interrupt through the CTC
 */
static void abc800_clock_tick(void)
{
  z80_interrupt(ctc_irq);
}

/*
 * Initialize the time for next event
 */
struct abctimer {
  uint64_t last;
  uint64_t period;
};
static struct abctimer clock_timer, blink_timer;
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
    break;
  }

  blink_timer.period = 400000000; /* 400 ms = 2.5 Hz */
  clock_timer.last = blink_timer.last = nstime();
}

static inline bool trigger(uint64_t now, struct abctimer *tmr)
{
  if (likely((now - tmr->last) < tmr->period))
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

  if (trigger(now, &blink_timer))
    blink = !blink;

  return blink;
}
