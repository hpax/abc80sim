#ifndef CHARTYPE_H
#define CHARTYPE_H

#include "compiler.h"

/*
 * Return the hex value of a single character, or negative if invalid.
 */
static inline int hexval(int c)
{
    if (c < '0')
	return -1;
    if (c <= '9')
	return c - '0';
    c |= 0x20;
    if (c < 'a')
	return -1;
    if (c <= 'f')
	return c - 'a' + 10;
    return -1;
}

static inline bool is_eoln(int c)
{
    return c == '\n' || c == '\r';
}

static inline bool is_eof(int c)
{
    return (c < 0) || c == ('Z' & 0x1f);
}

static inline bool is_white(int c)
{
    return (c >= '\a' && c <= '\r') || c == 0x7f || c == 0xff;
}

#endif /* CHARTYPE_H */
