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
#include "hostfile.h"
#include "serial.h"
#include "network.h"
#include "trace.h"
#include "options.h"

#include <signal.h>
#include <setjmp.h>

extern const char *program_name;

#define DEFAULT_PORT	4680
#define DEFAULT_SERVICE	"pun80"
#define DEFAULT_BAUD	19200

static jmp_buf terminate_buf;

static void sigint(int sig)
{
    (void)sig;
    longjmp(terminate_buf, 1);
}
static void sigint_init(void)
{
    if (setjmp(terminate_buf)) {
	fflush(NULL);
	exit(0);		/* Termination signal */
    }
    signal(SIGINT, sigint);
}

struct client_thread;
typedef int (*client_func)(struct client_thread *);

struct client_thread {
    SDL_Thread *tp;		/* Thread pointer */
    struct client_thread *reap;	/* Dead thread reaper chain */
    int rv;			/* Return value */
    int error;			/* errno, if any */
    int rfd;			/* Read file descriptor */
    int wfd;			/* Write file descriptor */
    client_func task;		/* Thread function */
    client_func ioerr;		/* Call this on EOF or I/O error */

    /* Port information if applicable, used by ioerr */
    struct {
	const char *name;	/* Name of port if applicable */
	unsigned int speed;	/* Speed of port */
    } port;
};

static ssize_t abcprint_daemon_send(void *pvt, const void *data, size_t len)
{
    struct client_thread *self = pvt;
    /* The generic code handles partial writes, so this is easy on our part */
    return write(self->wfd, data, len);
}

static bool errno_is_resume(int err)
{
#ifdef EAGAIN
    if (err == EAGAIN)
	return true;
#endif
#ifdef EWOULDBLOCK
    if (err == EWOULDBLOCK)
	return true;
#endif
#ifdef EINTR
    if (err == EINTR)
	return true;
#endif
    return false;
}

struct client_sync {
    SDL_mutex   *m_launch;
    SDL_mutex   *m_quit;
    SDL_cond    *c_quit;
    struct client_thread *list;
    struct client_thread *reap;
    unsigned int count;			/* Active thread counter */
} client;

#define BUF_SIZE 512

static void abcprint_close(struct client_thread *self)
{
    if (self->rfd >= 0) {
	close(self->rfd);
	if (self->wfd == self->rfd)
	    self->wfd = -1;
	self->rfd = -1;
    }
    if (self->wfd >= 0) {
	close(self->wfd);
	self->wfd = -1;
    }
}

static int abcprint_client_task(struct client_thread *self)
{
    char *ibuf = malloc(BUF_SIZE);
    ssize_t b = -1;
    struct abcprint *abcprint = NULL;

    if (!ibuf) {
	self->error = ENOMEM;
	goto quit;
    }

    if (self->rfd < 0 || self->wfd < 0) {
	self->error = EBADF;
	goto quit;
    }

    abcprint = abcprint_init(abcprint_daemon_send, self);
    if (!abcprint)
	goto quit;

    while (1) {
	self->error = 0;
	while (1) {
	    errno = 0;
	    b = read(self->rfd, ibuf, BUF_SIZE);
	    if (b <= 0) {
		if (errno_is_resume(errno))
		    continue;
		self->error = errno;
		break;
	    }
	    abcprint_recv(abcprint, ibuf, b);
	}

	if (!self->ioerr || self->ioerr(self) < 0) {
	    abcprint_close(self);
	    break;
	}
    }

quit:
    if (abcprint)
	abcprint_shutdown(abcprint);
    if (ibuf)
	free(ibuf);
    return b;
}

static int abcprint_launch_thread(void *selfp)
{
    struct client_thread *self = selfp;
    int rv;

    SDL_LockMutex(client.m_launch);
    assert(self->tp);
    SDL_UnlockMutex(client.m_launch);

    self->rv = rv = self->task(self);

    SDL_LockMutex(client.m_quit);
    self->reap  = client.reap;
    client.reap = self;
    SDL_CondSignal(client.c_quit);
    SDL_UnlockMutex(client.m_quit);

    return rv;			/* Reaper will unlock client.m_quitting */
}

static struct client_thread *
abcprint_start_thread(const struct client_thread *ctparm)
{
    struct client_thread *ct = malloc(sizeof *ct);
    if (!ct)
	return NULL;

    *ct = *ctparm;

    SDL_LockMutex(client.m_launch);
    ct->tp = SDL_CreateThread(abcprint_launch_thread, ct);
    if (ct->tp) {
	client.count++;
    } else {
	free(ct);
	ct = NULL;
    }
    SDL_UnlockMutex(client.m_launch);

    return ct;
}

static int abcprint_serial_open(struct client_thread *self)
{
    abcprint_close(self);

    while (1) {
	int fd = open_serial(self->port.name, self->port.speed, FLOW_RTS);
	self->rfd = self->wfd = fd;
	if (fd >= 0) {
	    self->error = 0;
	    return fd;
	} else if (errno == EBUSY || !opts.retry_port) {
	    self->error = errno;
	    free((void *)self->port.name);
	    return -1;
	}
	sleep(opts.retry_port);
    }
}

static struct client_thread *
abcprint_serial_start(char *port, unsigned long baud, bool legacy)
{
    struct client_thread ctparm = { .task = abcprint_client_task };

    if (!legacy) {
	char *c2 = strrchr(port, ':');
	unsigned long bparam;
	if (c2) {
	    *c2++ = '\0';
	    if (*c2) {
		bparam = strtoul(c2, NULL, 10);
		if (bparam)
		    baud = bparam;
	    }
	}
    }

    ctparm.ioerr     = opts.retry_port ? abcprint_serial_open : NULL;
    ctparm.port.name = strdup(port);
    ctparm.port.speed = baud;
    ctparm.rfd = ctparm.wfd = -1;

    if (abcprint_serial_open(&ctparm) < 0) {
	fprintf(stderr, "%s: %s: %s\n", program_name, port,
		strerror(ctparm.error));
	return NULL;
    }

    return abcprint_start_thread(&ctparm);
}

static struct sockset servsocks = { .psock = NULL, .nsock = 0 };

static int abcprint_listen_task(struct client_thread *self)
{
    struct client_thread ctparm = { .task = abcprint_client_task };

    (void)self;

    while (servsocks.nsock) {
	int fd;

	ctparm.rfd = ctparm.wfd = fd = sockset_accept(&servsocks);
	if (fd < 0)
	    break;

	abcprint_start_thread(&ctparm);
    }
    return 0;
}

int abcprint_run_servers(struct file_list *ports, unsigned long baud)
{
    char *filename;

    /*
     * When tracing in server mode, we can't buffer arbitrarily;
     * both because there might be more than one client, and because
     * we don't want to hold on to the data too long - and there should
     * be a lot less of it anyway.
     */
    if (tracef) {
	fflush(tracef);
	setvbuf(tracef, NULL, _IOLBF, 0);
    }

    sigint_init();
    socket_init();

    if (!baud)
	baud = DEFAULT_BAUD;

    client.m_launch = SDL_CreateMutex();
    client.m_quit   = SDL_CreateMutex();
    client.c_quit   = SDL_CreateCond();

    SDL_LockMutex(client.m_quit);

    while ((filename = filelist_pop(ports, NULL))) {
	struct client_thread ctparm = { .task = abcprint_client_task };
	int af = AF_UNSPEC;
	char *c1 = NULL;
	size_t netpfx = 0;

	if (!strcmp(filename, "-") || !strcasecmp(filename, "stdio")) {
	    ctparm.rfd = fileno(stdin);
	    ctparm.wfd = fileno(stdout);
	    abcprint_start_thread(&ctparm);
	    continue;
	}

	if (!strncasecmp(filename, "fd:", 3)) {
	    ctparm.rfd = ctparm.wfd = -1;
	    sscanf(filename, "%d,%d", &ctparm.rfd, &ctparm.wfd);
	    if (ctparm.wfd < 0)
		ctparm.wfd = ctparm.rfd;
	    if (ctparm.rfd >= 0)
		abcprint_start_thread(&ctparm);
	    continue;
	}

	if (!strncasecmp(filename, "ip", 2))
	    netpfx = 2;
	else if (!strncasecmp(filename, "net", 3) ||
		 !strncasecmp(filename, "tcp", 3) ||
		 !strncasecmp(filename, "ipv", 3))
	    netpfx = 3;

	if (netpfx) {
	    if (filename[netpfx] == '4') {
		af = AF_INET;
		netpfx++;
	    } else if (filename[netpfx] == '6') {
		af = AF_INET6;
		netpfx++;
	    }

	    if (filename[netpfx] == ':') {
		c1 = &filename[++netpfx];
	    } else if (!filename[netpfx]) {
		c1 = "";
	    } else {
		/* Not a valid network prefix, leave c1 == NULL */
	    }
	}

	if (c1) {
	    /* It is a network connection */
	    open_socket(af, c1, DEFAULT_SERVICE, DEFAULT_PORT, &servsocks);
	} else if (!strncasecmp(filename, "serial:", 7)) {
	    /* Prefixed syntax: parameters at end supported */
	    abcprint_serial_start(filename+7, baud, false);
	} else {
	    /* Legacy syntax: a port device name without any parameters */
	    abcprint_serial_start(filename, baud, true);
	}
    }

    if (servsocks.nsock) {
	struct client_thread ltparm = { .task = abcprint_listen_task };
	abcprint_start_thread(&ltparm);
    }

    if (!client.count)
	return 1;		/* Nothing ever started... */

    unsigned int reaped = 0;

    while (1) {
	unsigned int nclients;

	SDL_LockMutex(client.m_launch);
	client.count = nclients = client.count - reaped;
	reaped = 0;
	SDL_UnlockMutex(client.m_launch);

	if (!nclients)
	    break;

	if (!client.reap)
	    SDL_CondWait(client.c_quit, client.m_quit);

	while (client.reap) {
	    struct client_thread *victim = client.reap;
	    client.reap = victim->reap;

	    SDL_WaitThread(victim->tp, NULL);
	    free(victim);
	    reaped++;
	}
    }

    return 0;
}
