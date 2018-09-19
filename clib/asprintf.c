#include "compiler.h"

#ifndef HAVE_ASPRINTF

int asprintf(char **strp, const char *fmt, ...)
{
    char *buf;
    int len, out;
    va_list ap;

    *strp = NULL;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0)
	return -1;

    buf = malloc(len+1);
    if (!buf)
	return -1;

    va_start(ap, fmt);
    out = vsnprintf(buf, len+1, fmt, ap);
    if (out < 0 || out >= len) {
	/* Size of output changed behind our back?! */
	free(buf);
	errno = EAGAIN;		/* You can try again if you want... */
	return -1;
    }
    va_end(ap);

    *strp = buf;
    return out;
}

#endif
