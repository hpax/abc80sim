/*
 * Nanosecond-resolution clock, or as close as available.
 * Return a 64-bit value with an arbitrary epoch; the
 * function nstime_init() should initialize the baseline
 * so that wraparound is as unlikely as possible.
 */

#include "compiler.h"
#include "nstime.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <SDL.h>

#ifdef _POSIX_TIMERS

#ifdef _POSIX_MONOTONIC_CLOCK
# define WHICHCLOCK CLOCK_MONOTONIC
#else
# define WHICHCLOCK CLOCK_REALTIME
#endif

static time_t tv_sec_zero;

uint64_t nstime(void)
{
  struct timespec ts;
  clock_gettime(WHICHCLOCK, &ts);
  return ((ts.tv_sec - tv_sec_zero) * UINT64_C(1000000000)) + ts.tv_nsec;
}

void nstime_init(void)
{
  struct timespec ts;
  clock_gettime(WHICHCLOCK, &ts);
  tv_sec_zero = ts.tv_sec;
}

#elif defined(__WIN32__)

static uint64_t tzero;

uint64_t nstime(void)
{
  FILETIME ft;
  uint64_t t;

  GetSystemTimeAsFileTime(&ft);
  t = ((uint64_t)ft.dwHighDateTime << 32) + ft.dwLowDateTime;
  return (t - tzero) * UINT64_C(100);
}

void nstime_init(void)
{
  FILETIME ft;

  GetSystemTimeAsFileTime(&ft);
  tzero = ((uint64_t)ft.dwHighDateTime << 32) + ft.dwLowDateTime;
}

#elif defined(HAVE_GETTIMEOFDAY)

static time_t tv_sec_zero;

uint64_t nstime(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return ((tv.tv_sec - tv_sec_zero) * UINT64_C(1000000000))
    + (tv.tv_usec * UINT64_C(1000));
}

void nstime_init(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  tv_sec_zero = tv.tv_sec;
}

#else
# error "Need to implement a different fine-grained timer function here"
#endif

#if defined(WHICHCLOCK) && defined(HAVE_CLOCK_NANOSLEEP)

void mynssleep(uint64_t until, uint64_t since)
{
  (void)since;
  struct timespec req;

  req.tv_sec  = until / UINT64_C(1000000000) + tv_sec_zero;
  req.tv_nsec = until % UINT64_C(1000000000);

  clock_nanosleep(WHICHCLOCK, TIMER_ABSTIME, &req, NULL);
}

#elif defined(HAVE_NANOSLEEP)

void mynssleep(uint64_t until, uint64_t since)
{
  struct timespec req;

  until -= since;

  req.tv_sec  = until / UINT64_C(1000000000);
  req.tv_nsec = until % UINT64_C(1000000000);

  nanosleep(&req, NULL);
}

#else

void mynssleep(uint64_t until, uint64_t since)
{
  until -= since;

  SDL_Delay((until + UINT64_C(999999))/UINT64_C(1000000));
}

#endif
