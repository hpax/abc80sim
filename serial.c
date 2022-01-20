/*
 * Open a host serial port, and configure it for a specific speed
 * and, optionally, enable flow control. This is horrendously target-specific.
 */

#include "compiler.h"
#include "serial.h"

#ifdef _WIN32
# define NOPREFIX   "/.\\"
# define _PATH_DEV  "\\\\.\\"	/* This is correct per MSDN */
#else
# define NOPREFIX "/."
# ifndef _PATH_DEV
#  define _PATH_DEV "/dev/"
# endif
#endif

static char *strdup2(const char *s1, const char *s2)
{
    char *p;
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2) + 1;	/* Final NULL byte from here */

    p = malloc(l1 + l2);
    if (!p)
	return NULL;

    memcpy(p, s1, l1);
    memcpy(p+l1, s2, l2);

    return p;
}

static char *port_path(const char *port)
{
    const char *prefix = _PATH_DEV;

    if (!port[0]) {
	errno = ENOENT;
	return NULL;
    }

    if (strchr(NOPREFIX, port[0]))
	prefix = "";

    return strdup2(prefix, port);
}

#ifdef _WIN32

#include <windows.h>

static int config_port(int fd, unsigned long baud, enum flowctrl flowctrl)
{
    HANDLE fh = (HANDLE)_get_osfhandle(fd);
    DCB dcb;
    COMMTIMEOUTS cto;
    DWORD comerr;

    memset(&dcb, 0, sizeof dcb);
    dcb.DCBlength = sizeof dcb;

    if (!GetCommState(fh, &dcb)) {
	errno = ENOTTY;
	return -1;
    }

    /* There doesn't seem to be any equivalent to HUPCL? */

    dcb.BaudRate = baud;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.XonChar = 0;
    dcb.XoffChar = 0;
    dcb.ErrorChar = 0;
    dcb.EofChar = 0;
    dcb.EvtChar = 0;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    switch (flowctrl) {
    case FLOW_NONE:
	/* Already set up */
	break;

    case FLOW_DTR:
	dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
	dcb.fOutxDsrFlow = TRUE;
	break;

    case FLOW_RTS:
	dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
	dcb.fOutxCtsFlow = TRUE;
	break;
    }

    if (!SetCommState(fh, &dcb)) {
	errno = EINVAL;
	return -1;
    }

    memset(&cto, 0, sizeof cto);
    cto.ReadIntervalTimeout = 1;

    if (SetCommTimeouts(fh, &cto)) {
	errno = EINVAL;
	return -1;
    }

    PurgeComm(fh, PURGE_TXCLEAR|PURGE_RXCLEAR);
    comerr = CE_RXOVER|CE_OVERRUN|CE_RXPARITY|CE_FRAME|CE_BREAK;
    ClearCommError(fh, &comerr, NULL);

    return 0;
}

#elif defined(HAVE_TERMIOS_H)

/*
 * POSIX systems
 */

# ifdef __linux__
/*
 * Linux has been able to set arbitrary speeds for ages, but glibc never
 * caught up.  Our own mini-implementation of termios...
 */
#include <sys/ioctl.h>
#include <asm/termbits.h>	/* struct termios2 */
#include <linux/serial.h>	/* struct serial_struct */

#ifndef TCGETS2			/* On PowerPC kernel termios == termios2 */
typedef struct termios my_termios;
# define TCGETS2  TCGETS
# define TCSETS2  TCSETS
#else
typedef struct termios2 my_termios;
#endif

/* Do nonstandard initialization: set port to minimal latency */
static int mytcsetup(int fd)
{
    struct serial_struct ss;
    int rv;

    memset(&ss, 0, sizeof ss);

    rv = ioctl(fd, TIOCGSERIAL, &ss);
    if (rv)
	return rv;

    ss.flags |= ASYNC_LOW_LATENCY;

    return ioctl(fd, TIOCSSERIAL, &ss);
}

static int mytcgetattr(int fd, my_termios *tio)
{
    return ioctl(fd, TCGETS2, tio);
}

static int mytcsetattr(int fd, const my_termios *tio)
{
    return ioctl(fd, TCSETS2, tio);
}

static int mycfsetbaud(my_termios *tio, unsigned long baud)
{
    tio->c_cflag &= ~(CBAUD | CIBAUD);
    tio->c_cflag |= BOTHER;
    tio->c_ispeed = tio->c_ospeed = baud;
    return 0;
}

static int mytcflush(int fd, int queue)
{
    return ioctl(fd, TCFLSH, queue);
}

# else /* not Linux */

#  include "baudtospeed.h"

typedef struct termios my_termios;

static int mytcsetup(int fd)
{
    (void)fd;
    return 0;
}

# define mytcgetattr(x,y) tcgetattr(x, y)
# define mytcsetattr(x,y) tcsetattr(x, TCSANOW, y)
# define mytcflush(x,y)   tcflush(x, y)

static int mycfsetbaud(my_termios *tio, unsigned long baud)
{
    speed_t speed = baudtospeed(baud);
    if (speed == B0) {
	errno = EINVAL;
	return -1;
    }
    return cfsetospeed(tio, speed) | cfsetispeed(tio, speed);
}

# endif /* not Linux */

#ifdef CRTSCTS
/* All good */
#elif defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
# define CRTSCTS (CCTS_OFLOW|CRTS_IFLOW)
#else
# define CRTSCTS 0
#endif

static int config_port(int fd, unsigned long baud, enum flowctrl flowctrl)
{
    my_termios tio;

    if (!baud) {
	errno = EINVAL;
	return -1;
    }

    mytcsetup(fd);		/* Ignore failures here */

    if (mytcgetattr(fd, &tio))
	return -1;

    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		     | INLCR | IGNCR | ICRNL | IXON);
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_oflag &= ~OPOST;
    tio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS | CLOCAL);
    tio.c_cflag |= HUPCL | CREAD | CS8;
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    switch (flowctrl) {
    case FLOW_NONE:
	tio.c_cflag |= CLOCAL;
	break;

    case FLOW_RTS:
	tio.c_cflag |= CRTSCTS;
	break;

    case FLOW_DTR:
	/* Not clear how to support on Unix */
	break;
    }

    if (mycfsetbaud(&tio, baud))
	return -1;

    if (mytcsetattr(fd, &tio))
	return -1;

    return mytcflush(fd, TCIOFLUSH);
}

#else

/*
 * Don't know how to configure serial on this system; either the user has
 * to do it manually, or it is not necessary...
 */
static int config_port(int fd, unsigned long baud, enum flowctrl flowctrl)
{
    (void)fd;
    (void)baud;
    (void)flowctrl;

    errno = ENOTTY;
    return -1;
}

#endif

int open_serial(const char *port, unsigned long baud, enum flowctrl flowctrl)
{
    char *path = port_path(port);
    int fd = -1;

    if (!path)
	goto fail;

    fd = open(path, O_RDWR);
    free(path);
    if (fd < 0)
	goto fail;

    /*
     * Allow a port to not be an actual serial port;
     * if it is some other kind of device then config_port() fails.
     */
    if (config_port(fd, baud, flowctrl) && errno != ENOTTY)
	goto fail;

    return fd;

fail:
    if (fd >= 0)
	close(fd);

    return -1;
}
