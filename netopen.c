/*
 * netopen.c
 *
 * Open a TCP network connection to the specified hostname and port.
 * Deal with the annoying fact that socket calls on Windows return an
 * OS handle and not a file descriptor...
 */

#include "compiler.h"
#include "network.h"

#ifdef _WIN32

static WSADATA wd;

static void end_winsock(void)
{
    if (wd.wVersion) {
	WSACleanup();
	memset(&wd, 0, sizeof wd);
    }
}

void socket_init(void)
{
    if (wd.wVersion)
	return;			/* Already initialized */

    if (!WSAStartup(MAKEWORD(2,2), &wd))
	atexit(end_winsock);
}

/*
 * Careful here: this is not an idempotent operation, as it creates a
 * few filehandle. Thus, socket_to_fd(fd_to_socket(sockfd)) is roughly
 * equivalent to dup(sockfd).
 */
static int socket_to_fd(SOCKET sock)
{
    int fd;

    if (sock == INVALID_SOCKET)
	return -1;

    fd = _open_osfhandle((intptr_t)sock, O_RDWR|O_BINARY);
    if (fd < 0)
	closesocket(sock);
    return fd;
}

/*
 * This is an idempotent operation: fd_to_socket(sockfd) will always
 * return the same value and will not modify any state.
 */
static SOCKET fd_to_socket(int fd)
{
    return (SOCKET)_get_osfhandle(fd);
}

static int socket_nonblock(SOCKET sock)
{
    int on = 1;
    return ioctlsocket(sock, FIONBIO, &on);
}

/*
 * The Windows equivalent to O_CLOEXEC is !HANDLE_FLAG_INHERIT (see
 * SetHandleInformation()) and the bInheritHandle variable of the
 * security attributes of a handle. The default is to *not* inherit,
 * so equivalent to O_CLOEXEC = 1.
 */
static int socket_cloexec(SOCKET sock)
{
    (void)sock;
    return 0;
}

#ifdef AF_INET6
# define HAVE_GETADDRINFO 1
#endif

#else

static int socket_nonblock(SOCKET sock)
{
#if !defined(SOCK_NONBLOCK) && defined(F_SETFD) && defined(O_NONBLOCK)
    int o_nonblock = O_NONBLOCK;
    return fcntl(sock, F_SETFL, &o_nonblock);
#else
    (void)sock;
    return 0;
#endif
}

static int socket_cloexec(SOCKET sock)
{
#if !defined(SOCK_CLOEXEC) && defined(F_SETFD) && defined(FD_CLOEXEC)
    int fd_cloexec = FD_CLOEXEC;
    return fcntl(sock, F_SETFD, fd_cloexec);
#else
    (void)sock;
    return 0;
#endif
}

#endif /* WIN32 */

/*
 * Returns a malloc'd buffer in *name, free to free both name and serv.
 * Note that getaddrinfo wants NULL, not empty strings!
 */
static int split_service(const char *spec, char **name, char **serv)
{
    char *np, *sp;

    np = strdup(spec);
    if (!np)
	return -1;
    *name = np;
    *serv = "";

    sp = strrchr(np, '\0');
    while (*sp != ':') {
	if (sp == np || *sp == ']') {
	    /* No port */
	    return 0;
	}
	sp--;
    }
    *sp++ = '\0';
    *serv = sp;
    return 0;
}

static int setsockopt_int(SOCKET sock, int level, int optname, int val)
{
    return setsockopt(sock, level, optname, &val, sizeof val);
}

static SOCKET
make_socket(const addrinfo_t *ai, struct sockset *passive, bool strict)
{
    SOCKET sock;
    struct pollfd *ps;
    int socktype = ai->ai_socktype;

#ifdef SOCK_NONBLOCK
    socktype |= passive ? SOCK_NONBLOCK : 0;
#endif
#ifdef SOCK_CLOEXEC
    socktype |= SOCK_CLOEXEC;
#endif

    sock = socket(ai->ai_family, socktype, ai->ai_protocol);
    if (sock == INVALID_SOCKET)
	return INVALID_SOCKET;

    socket_cloexec(sock);
    setsockopt_int(sock, SOL_SOCKET, SO_KEEPALIVE, 1);
    if (ai->ai_protocol == IPPROTO_TCP)
	setsockopt_int(sock, IPPROTO_TCP, TCP_NODELAY, 1);

    (void)strict;
#if defined(SOL_IPV6) && defined(IPV6_V6ONLY)
    if (strict && ai->ai_family == AF_INET6) {
	setsockopt_int(sock, SOL_IPV6, IPV6_V6ONLY, 1);
    }
#endif

    if (ai->ai_addr->sa_family == AF_UNSPEC)
	ai->ai_addr->sa_family = ai->ai_family;

    if (!passive) {
	if (!connect(sock, ai->ai_addr, ai->ai_addrlen))
	    return sock;
    } else {
	setsockopt_int(sock, SOL_SOCKET, SO_REUSEADDR, 1);
	socket_nonblock(sock);

	if (!bind(sock, ai->ai_addr, ai->ai_addrlen)) {
	    ps = realloc(passive->psock, sizeof(*ps)*(passive->nsock + 1));
	    if (ps && !listen(sock, 0)) {
		passive->psock = ps;
		ps += passive->nsock++;
		ps->fd = sock;	/* A SOCKET, not an fd... */
		ps->events = POLLIN; /* accept() is considered input */
		return sock;
	    }
	}
    }

    closesocket(sock);
    return INVALID_SOCKET;
}

#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif
#ifndef AI_V4MAPPED
# define AI_V4MAPPED 0
#endif
#ifndef AI_PASSIVE
# define AI_PASSIVE 0
#endif

#ifdef HAVE_GETADDRINFO

int open_socket(int af, const char *spec, const char *defsvc,
		unsigned int defport, struct sockset *passive)
{
    SOCKET sock;
    struct addrinfo *ai, *ap;
    struct addrinfo hints;
    int err;
    char *name, *serv;
    char port_buf[sizeof defport * 3];
    int passive_found = 0;

    if (split_service(spec, &name, &serv))
	return -1;

    printf("spec %s name %s serv %s passive %u\n", spec, name, serv, !!passive);

    while (1) {
	const char *n = *name ? name : NULL;
	const char *s = *serv ? serv : (defsvc && *defsvc) ? defsvc : NULL;

	memset(&hints, 0, sizeof hints);
	hints.ai_family   = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_ADDRCONFIG | AI_V4MAPPED;
	if (passive)
	    hints.ai_flags |= AI_PASSIVE;

	printf("getaddrinfo %s %s", n, s);
	ai = NULL;
	err = getaddrinfo(n, s, &hints, &ai);
	printf(" : %s\n", err ? gai_strerror(err) : "ok");
	if (err && serv != port_buf) {
	    /* May have to fall back to the default port number */
	    snprintf(port_buf, sizeof port_buf, "%u", defport);
	    serv = port_buf;
	    continue;
	}
	break;
    }

    free(name);

    if (passive) {
	/*
	 * For some odd reason, at least on glibc 2.34 getaddrinfo()
	 * returns the AF_INET any address before the AF_INET6 one,
	 * which causes obvious problems, so in that case, try to
	 * create an IPv6 listening port *first*... unless, of course,
	 * we are asking to be IPv4 only.
	 */
	for (ap = ai; ap; ap = ap->ai_next) {
	    const sockaddr_any_t *aps = (const sockaddr_any_t *)ap->ai_addr;

	    if (af == AF_UNSPEC && ap->ai_family == AF_INET &&
		aps->sin.sin_addr.s_addr == INADDR_ANY) {
		struct sockaddr_in6 in6addr = {
		    .sin6_addr = IN6ADDR_ANY_INIT,
		    .sin6_port = aps->sin.sin_port
		};

		struct addrinfo aix = *ap;
		aix.ai_family  = AF_INET6;
		aix.ai_addr    = (struct sockaddr *)&in6addr;
		aix.ai_addrlen = sizeof in6addr;

		sock = make_socket(&aix, passive, false);
		if (sock != INVALID_SOCKET)
		    passive_found++;
	    }

	    sock = make_socket(ap, passive, af == AF_INET6);
	    if (sock != INVALID_SOCKET)
		passive_found++;
	}
    } else {
	for (ap = ai; ap; ap = ap->ai_next) {
	  sock = make_socket(ap, NULL, false);
	    if (sock != INVALID_SOCKET)
		break;
	}
    }

    if (ai)
	freeaddrinfo(ai);

    return passive ? passive_found : socket_to_fd(sock);
}

#else

/* Legacy IPv4-only API */

int open_socket(int af, const char *spec, unsigned int defport, bool passive)
{
    struct hostent *he;
    SOCKET sock;
    struct in_addr **haddr;
    struct in_addr iaddr;
    struct in_addr *ihaddr[2];
    struct sockaddr_in si;
    addrinfo_t ai;
    unsigned int port;
    sock_open_func what = passive ? bind : connect;
    char *name, *serv;
    int passive_found = 0;

    if (af == AF_UNSPEC)
	af = AF_INET;
    else if (af != AF_INET)
	return -1;		/* Only AF_INET supported */

    if (split_service(spec, &name, &serv))
	return -1;

    ai.ai_family   = af;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_protocol = IPPROTO_TCP;
    ai.ai_addrlen  = sizeof iaddr;

    ihaddr[0] = &iaddr;
    ihaddr[1] = NULL;
    haddr = &ihaddr[0];

    if (!*name) {
	iaddr.s_addr = passive ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    } else if (inet_aton(name, &iaddr)) {
	/* Got it... */
    } else {
	he = gethostbyname(name);
	if (!he || he->h_addrtype != ai.ai_family)
	    return -1;
	haddr = (struct in_addr **)he->h_addr_list;
	ai.ai_addrlen = he->h_length;
    }

    port = htons(defport);
    if (serv && *serv) {
	struct servent *se = getservbyname(serv, "tcp");
	if (se) {
	    port = se->s_port;
	} else {
	    char *ep;
	    unsigned long uls = htons(strtoul(serv, &ep, 10));
	    if (uls <= 0xffff && !*ep) {
		port = htons(uls);
	    }
	    /* otherwise use the default port number */
	}
    }

    free(name);

    while (*haddr) {
	ai.ai_addr = *haddr++;
	sock = make_socket(ap, passive);
	if (sock != INVALID_SOCKET) {
	    if (!passive)
		break;		/* Connection established */

	    passive_found++;
	}
    }

    return passive ? passive_found : socket_to_fd(sock);
}

#endif

#ifndef POLLRDHUP
# define POLLRDHUP 0
#endif

static int sock_accept(SOCKET sock)
{
    SOCKET as;

#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC)
    as = accept4(sock, NULL, NULL, SOCK_CLOEXEC);
#else
    as = accept(sock, NULL, NULL);
    socket_cloexec(as);
#endif
    return socket_to_fd(as);
}

/*
 * Listen for a connection to a set of listening sockets.
 * Returns a file descriptor, even on Windows, or a negative error value.
 */
int sockset_accept(struct sockset *set)
{
    while (set->nsock) {
	int pv = poll(set->psock, set->nsock, -1);
	if (pv < 0) {
	    if (sockerr(EAGAIN) || sockerr(EWOULDBLOCK) || sockerr(ENOMEM))
		continue;
	    return -sock_errno();
	}

	if (pv == 0)
	    return -socke(EWOULDBLOCK);

	for (unsigned int i = 0; i < set->nsock; i++) {
	    struct pollfd *ps = &set->psock[i];
	    if (ps->revents & POLLIN) {
		int fd = sock_accept(ps->fd);
		if (fd >= 0)
		    return fd;
	    } else if (ps->revents & POLLNVAL) {
		ps->fd = INVALID_SOCKET;
	    } else if (ps->revents & POLLERR) {
		closesocket(ps->fd);
		ps->fd = INVALID_SOCKET;
	    }
	}
    }

    return -socke(EINVAL);	/* Empty list of sockets */
}
