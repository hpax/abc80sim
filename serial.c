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

    if (baud) {
	dcb.BaudRate = baud;
    }
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

#include "baudtospeed.h"

/*
 * POSIX systems
 */
# ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif

#ifdef CRTSCTS
/* All good */
#elif defined(CCTS_OFLOW) && defined(CRTS_IFLOW)
# define CRTSCTS (CCTS_OFLOW|CRTS_IFLOW)
#else
# define CRTSCTS 0
#endif

static int config_port(int fd, unsigned long baud, enum flowctrl flowctrl)
{
    struct termios tio;

    if (tcgetattr(fd, &tio))
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

    if (baud) {
	if (cfsetospeed(&tio, baud) || cfsetispeed(&tio, baud))
	    return -1;
    }

    return tcsetattr(fd, TCSANOW, &tio);
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

static inline int lock_port(int fd)
{
    (void)fd;			/* If it compiles to nothing */

#ifdef HAVE_FLOCK
    if (flock(fd, LOCK_EX|LOCK_NB))
	return -1;
#endif

#ifdef TIOCEXCL
    ioctl(fd, TIOCEXCL, 0);
#endif

    return 0;
}

static int open_lock_port(const char *path)
{
    int fd = -1;

    do {
#ifdef HAVE__SOPEN
	fd = _sopen(path, O_RDWR|O_CLOEXEC, _SH_DENYRW);
#else
	fd = open(path, O_RDWR|O_CLOEXEC);

	if (fd >= 0 && lock_port(fd)) {
	    /* Lock failure */
	    close(fd);
	    errno = EBUSY;
	    fd = -1;
	    break;
	}
#endif
    } while (fd < 0 && errno == EINTR);

    return fd;
}

int open_serial(const char *port, unsigned long baud, enum flowctrl flowctrl)
{
    char *path = port_path(port);
    int fd = -1;

    if (!path)
	goto fail;

    fd = open_lock_port(path);
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
