#include "config.h"

#include "z80.h"

#include <time.h>

static uint8_t bytes[8];	/* YY YY MM DD HH MM SS FF */
static int ptr;

#ifdef __WIN32__

#include <windows.h>

/*
 * Windows: use GetLocalTime
 */

static void latch_time(void)
{
    SYSTEMTIME lt;

    GetLocalTime(&lt);

    bytes[0] = lt.wYear / 100;
    bytes[1] = lt.wYear % 100;
    bytes[2] = lt.wMonth;
    bytes[3] = lt.wDay;
    bytes[4] = lt.wHour;
    bytes[5] = lt.wMinute;
    bytes[6] = lt.wSecond;
    bytes[7] = lt.wMilliseconds / 20;
    ptr = 0;
}

#else

/* Unix-like API assumed */

#include <sys/time.h>

static void latch_time(void)
{
    struct timeval tv;
    const struct tm *tm;
    time_t t;

    gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    tm = localtime(&t);

    bytes[0] = tm->tm_year / 100 + 19;
    bytes[1] = tm->tm_year % 100;
    bytes[2] = tm->tm_mon + 1;
    bytes[3] = tm->tm_mday;
    bytes[4] = tm->tm_hour;
    bytes[5] = tm->tm_min;
    bytes[6] = tm->tm_sec;
    bytes[7] = tv.tv_usec / 20000;
    ptr = 0;
}

#endif

int rtc_in(int sel, int port)
{
    uint8_t b;

    (void)sel;

    switch (port) {
    case 0:
	b = bytes[ptr];
	ptr = (ptr+1) & 7;
	break;
    case 1:
	latch_time();
	b = 0xd2;		/* Presence check */
	break;
    default:
	b = 0xff;
	break;
    }

    return b;
}
