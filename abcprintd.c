/*
 * abcprintd.c
 *
 * Interface to hardware printer/file interface via
 * a physical or virtual serial port. In the future might also
 * add network interface.
 *
 * This is now integrated in abc80sim via the --server option.
 */

#include "compiler.h"
#include "abcprintd.h"
#include "serial.h"

#define BUF_SIZE 512

static ssize_t abcprint_daemon_send(void *pvt, const void *data, size_t len)
{
    int fd = *(int *)pvt;

    /* The generic code handles partial writes, so this is easy on our part */
    return write(fd, data, len);
}

int abcprint_daemon_open(const char *port, unsigned long baud)
{
    if (!baud)
	baud = 19200;	/* Suitable for ABC800; for USB doesn't matter */

    return open_serial(port, baud, FLOW_RTS);
}

int abcprint_daemon_thread(int fd)
{
    char ibuf[BUF_SIZE];
    ssize_t b;
    struct abcprint *abcprint;

    if (fd < 0) {
	errno = EBADF;
	return -1;
    }

    abcprint = abcprint_init(abcprint_daemon_send, &fd);
    if (!abcprint)
	return -1;

    while (1) {
	do {
	    errno = 0;
	    b = read(fd, ibuf, sizeof ibuf);
	    if (b <= 0 && errno != EAGAIN)
		return b;
	} while (b <= 0);

	abcprint_recv(abcprint, ibuf, b);
    }
}
