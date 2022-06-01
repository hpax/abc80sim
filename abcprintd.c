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

extern const char *program_name;

#define DEFAULT_PORT	4680
#define DEFAULT_SERVICE	"pun80"
#define DEFAULT_BAUD	19200

struct client_thread;
typedef int (*client_func)(struct client_thread *);

struct client_thread {
    SDL_Thread *tp;		/* Thread pointer */
    struct client_thread *reap;	/* Reaper chain */
    int rv;			/* Return value */
    int fd;			/* Worker file descriptor */
    client_func task;		/* Thread function */
};

static ssize_t abcprint_daemon_send(void *pvt, const void *data, size_t len)
{
    struct client_thread *self = pvt;
    /* The generic code handles partial writes, so this is easy on our part */
    return write(self->fd, data, len);
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
    struct client_thread *reap;
    unsigned int count;			/* Active thread counter */
} client;

#define BUF_SIZE 512

static int abcprint_client_task(struct client_thread *self)
{
    char *ibuf = malloc(BUF_SIZE);
    ssize_t b = -1;
    struct abcprint *abcprint;

    if (!ibuf)
	goto quit;

    if (self->fd < 0) {
	errno = EBADF;
	goto quit;
    }

    abcprint = abcprint_init(abcprint_daemon_send, &self->fd);
    if (!abcprint)
	goto quit;

    while (1) {
	errno = 0;
	b = read(self->fd, ibuf, sizeof ibuf);
	if (b < 0 && errno_is_resume(errno))
	    continue;
	if (b <= 0)
	    break;

	abcprint_recv(abcprint, ibuf, b);
    }

quit:
    /* abcprint_shutdown() */
    if (ibuf)
	free(ibuf);
    return b;
}

static int abcprint_thread(void *selfp)
{
    struct client_thread *self = selfp;
    int rv;

    SDL_LockMutex(client.m_launch);
    assert(self->tp);
    SDL_UnlockMutex(client.m_launch);

    rv = self->task(self);

    SDL_LockMutex(client.m_quit);
    self->rv    = rv;
    self->reap  = client.reap;
    client.reap = self;
    SDL_CondSignal(client.c_quit);
    SDL_UnlockMutex(client.m_quit);

    return rv;			/* Reaper will unlock client.m_quitting */
}

static struct client_thread *abcprint_start_thread(int fd, client_func task)
{
    struct client_thread *ct = calloc(1, sizeof *ct);
    if (!ct)
	return NULL;

    ct->fd   = fd;
    ct->task = task;

    SDL_LockMutex(client.m_launch);
    ct->tp = SDL_CreateThread(abcprint_thread, ct);
    if (ct->tp) {
	client.count++;
    } else {
	free(ct);
	ct = NULL;
    }
    SDL_UnlockMutex(client.m_launch);

    return ct;
}

static struct client_thread *
abcprint_serial_start(char *port, unsigned long baud, bool legacy)
{
    int fd;

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

    fd = open_serial(port, baud, FLOW_RTS);

    if (fd < 0) {
	fprintf(stderr, "%s: %s: %s\n",	program_name, port, strerror(errno));
	return NULL;
    }

    return abcprint_start_thread(fd, abcprint_client_task);
}

static struct sockset servsocks = { .psock = NULL, .nsock = 0 };

static int abcprint_listen_task(struct client_thread *self)
{
    (void)self;
    while (servsocks.nsock) {
	int fd = sockset_accept(&servsocks);
	if (fd < 0)
	    break;

	abcprint_start_thread(fd, abcprint_client_task);
    }
    return 0;
}

int abcprint_run_servers(struct file_list *ports, unsigned long baud)
{
    char *filename;

    socket_init();

    if (!baud)
	baud = DEFAULT_BAUD;

    client.m_launch = SDL_CreateMutex();
    client.m_quit   = SDL_CreateMutex();
    client.c_quit   = SDL_CreateCond();

    SDL_LockMutex(client.m_quit);

    while ((filename = filelist_pop(ports, NULL))) {
	int af = AF_UNSPEC;
	char *c1 = NULL;
	size_t netpfx = 0;

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

    if (servsocks.nsock)
	abcprint_start_thread(0, abcprint_listen_task);

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
