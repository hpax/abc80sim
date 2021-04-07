/*
 * localtime_r implementation if missing
 */

#include "compiler.h"

#ifdef HAVE_LOCALTIME_R

/* Nothing to do */

#elif defined(HAVE_LOCALTIME_S)

/* Windows */

struct tm *localtime_r(const time_t *time, struct tm *dest)
{
    int err = localtime_s(dest, time);
    if (err) {
	errno = err;
	return NULL;
    }
    return dest;
}

#else

/* Need to run localtime() under mutex */

struct tm *localtime_r(const time_t *time, struct tm *dest)
{
    static SDL_mutex *time_mutex;
    struct tm *tm;

    SDL_mutexP(time_mutex);
    tm = localtime(&time);
    if (tm)
	*dest = *tm;
    else
	dest = NULL;
    SDL_mutexV(time_mutex);

    return dest;
}

#endif
