/*
 * Nanosecond-resolution clock, if available.
 * Return a 64-bit value; wraparound is possible but
 * acceptable.
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

uint64_t nstime(void)
{
  struct timespec ts;
  clock_gettime(WHICHCLOCK, &ts);
  return ((uint64_t)ts.tv_sec * 1000000000) + ts.tv_nsec;
}

#elif defined(__WIN32__)

uint64_t nstime(void)
{
  FILETIME ft;

  GetSystemTimeAsFileTime(&ft);
  return (((uint64_t)ft.dwHighDateTime << 32) + ft.dwLowDateTime) * 100;
}

#elif defined(HAVE_GETTIMEOFDAY)

uint64_t nstime(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return ((uint64_t)tv.tv_sec * 1000000000) + ((uint64_t)tv.tv_usec * 1000);
}

#else
# error "Need to implement a different fine-grained timer function here"
#endif

#if defined(WHICHCLOCK) && defined(HAVE_CLOCK_NANOSLEEP)

void mynssleep(uint64_t until, uint64_t since)
{
  (void)since;
  struct timespec req;

  req.tv_sec  = until / UINT64_C(1000000000);
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

  SDL_Delay(until/UINT64_C(1000000));
}

#endif
