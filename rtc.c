#include "compiler.h"
#include "z80.h"
#include "abcio.h"

#include <time.h>

struct rtc_time {
    uint16_t year;
    uint8_t month;		/* 1 = January */
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t wday;		/* 0 = Sunday */
    uint32_t usec;
};

static struct rtc_time rt;

#ifdef _WIN32

#include <windows.h>

/* Windows: use GetLocalTime */

static void sys_latch_time(void)
{
    SYSTEMTIME lt;

    GetLocalTime(&lt);

    rt.year  = lt.wYear;
    rt.month = lt.wMonth;
    rt.day   = lt.wDay;
    rt.hour  = lt.wHour;
    rt.min   = lt.wMinute;
    rt.sec   = lt.wSecond;
    rt.usec  = lt.wMilliseconds * 1000;
    rt.wday  = lt.wDayOfWeek;
}

#else

/* Unix-like API assumed */

#include <sys/time.h>

static void sys_latch_time(void)
{
    struct timeval tv;
    struct tm tm;
    time_t t;

    gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    if (!localtime_r(&t, &tm))
      return;

    rt.year  = tm.tm_year + 1900;
    rt.month = tm.tm_mon + 1;
    rt.day   = tm.tm_mday;
    rt.hour  = tm.tm_hour;
    rt.min   = tm.tm_min;
    rt.sec   = tm.tm_sec;
    rt.usec  = tv.tv_usec;
    rt.wday  = tm.tm_wday;
}

#endif

static void latch_time(void)
{
    sys_latch_time();

    if (tracing(TRACE_IO)) {
	fprintf(tracef, "RTC: latching time: %04u-%02u-%02u %02u:%02u:%02u.%06u (dow %u)\n",
		rt.year, rt.month, rt.day, rt.hour, rt.min, rt.sec, rt.usec,
		rt.wday);
    }
}

/* Fake ABC-bus connected RTC */

/* This is the "fake" ABCbus-connected RTC */
static uint8_t bustime[8];

static void busrtc_latch_time(void)
{
    latch_time();

    bustime[0] = rt.year / 100;
    bustime[1] = rt.year % 100;
    bustime[2] = rt.month;
    bustime[3] = rt.day;
    bustime[4] = rt.hour;
    bustime[5] = rt.min;
    bustime[6] = rt.sec;
    bustime[7] = rt.usec / 20000;
}

static uint8_t busrtc_in(uint8_t sel, uint16_t port)
{
    static unsigned int busptr;
    uint8_t v;

    (void)sel;

    switch (port) {
    case 0:			/* INP# */
        v = bustime[busptr];
	busptr = (busptr + 1) & 7;
        break;
    case 1:			/* STATUS# */
        busrtc_latch_time();
	busptr = 0;
        v = 0xd2;               /* Presence check */
        break;
    default:
        v = 0xff;
        break;
    }

    return v;
}

static const struct abcbus_dev busrtc_dev =
{
    .in  = busrtc_in,
    .portmask = 7
};

void rtc_init(void)
{
    register_abcbus_dev(55, &busrtc_dev);
}


/* ABC806 RTC (E05-16) */

static uint8_t e05time[7];
static unsigned int e05bit, databits;
static uint8_t e05cmd;
static uint8_t e05state;

static uint8_t bin2bcd(uint8_t v)
{
    return ((v/10) << 4) + (v % 10);
}

static void e05_latch_time(void)
{
    latch_time();

    /* These are in the order used by command 7 */
    e05time[0] = bin2bcd(rt.hour);
    e05time[1] = bin2bcd(rt.min);
    e05time[2] = bin2bcd(rt.day);
    e05time[3] = bin2bcd(rt.month);
    e05time[4] = bin2bcd(rt.year % 100);
    e05time[5] = bin2bcd(rt.wday ? rt.wday : 7); /* or rt.wday + 1? */
    e05time[6] = bin2bcd(rt.sec);
}

/*
 * This is a mapping of command number to register number
 * where the register number is in the sequence used by command 7.
 */
static const uint8_t e05cmdtoreg[8] = { 6, 1, 0, 2, 3, 5, 4, 0 };

void abc806_rtc_out(uint16_t port, uint8_t val)
{
    uint8_t oldstate, reg, set, clr;

    if (port & 1)
        return;                 /* No idea what this does */

    reg = val & 7;
    oldstate = e05state;
    e05state = (val >> 7 << reg) | (e05state & ~(1 << reg));
    set = e05state & ~oldstate;
    clr = ~e05state & oldstate;

    switch (reg) {
    case 0:
        /* "EME* = ? */
        break;
    case 1:
        /* Mode40 - set here? */
        break;
    case 2:
        /* Bit A8 on "7621/HRU11" = ? */
        break;
    case 3:
        /* INI# on "PROT" = ? */
        break;
    case 4:
        /* "TXOFF" = ? */
        break;
    case 5:
        /* RTC chip select */
        if (set) {
            e05_latch_time();
	    e05cmd = 0;		/* Reset pointer for E05-16 */
	    e05bit = -1U;
	    databits = 0;
	}
        break;
    case 6:
	/* RTC clock */
	if (e05state & 0x20) {
	    if (clr) {
		/* Negative flank: clock data */
		e05bit++;
		if (databits) {
		    databits--;
		    if (!databits)
			e05bit = 0;
		} else {
		    if (e05bit == 4) {
			unsigned int reg = e05cmd & 7;
			databits = reg == 7 ? 7*8 : 8;
			e05bit = e05cmdtoreg[reg] << 3;
			if (tracing(TRACE_IO)) {
			    unsigned int bytes = databits >> 3;
			    unsigned int r = e05bit >> 3;

			    fprintf(tracef, "RTC: %s reg %d :",
				    (e05cmd & 8) ? "rd" : "wr",
				    reg);
			    while (bytes--)
				fprintf(tracef, " %02x", e05time[r++]);
			    fputc('\n', tracef);
			}
		    }
		}
	    } else {
		/* Positive flank: latch data */

		if (databits) {
		    /* Writing not supported */
		} else {
		    e05cmd = ((e05cmd << 1) | (e05state >> 7)) & 15;
		}
	    }
        }
        break;
    case 7:
	/* RTC data (handled via RTC clock) */
        break;
    }
}

pure_func uint8_t abc806_rtc_in(uint16_t port)
{
    uint8_t v;

    if (!(port & 1))
        return 0xff;            /* No idea */

    /* D3:0 has something to do with "7621/HRU11" = ?; assume 1 for now */
    v = 0x7f | (e05state & 0x80);

    /* D7 is RTC output */
    if ((e05state & 0x20) && (e05cmd & 8) && databits) {
	unsigned int by = e05bit >> 3;
	unsigned int bi = ~e05bit & 7;
        v = (v & 0x7f) | ((e05time[by] << bi) & 0x80 & e05state);
    }
    return v;
}
