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

# error "No thread-safe version of localtime() known"

#endif
