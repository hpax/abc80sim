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

#ifdef _POSIX_TIMERS

uint64_t nstime(void)
{
  struct timespec ts;
#ifdef _POSIX_MONOTONIC_CLOCK
  const clockid_t whichclock = CLOCK_MONOTONIC;
#else
  const clockid_t whichclock = CLOCK_REALTIME;
#endif
  clock_gettime(whichclock, &ts);
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
