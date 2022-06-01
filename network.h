/*
 * network.h
 */

#ifndef NETWORK_H
#define NETWORK_H 1

#include "compiler.h"

#ifdef _WIN32

#include <ws2tcpip.h>
#ifdef HAVE_AFUNIX_H
# include <afunix.h>
#endif

#define setsockerr(x)	WSASetLastError(x)
static inline int sock_errno(void)
{
    return WSAGetLastError();
}
#define socke(x)        (WSA ## x)
#define sockerr(x)      (sock_errno() == (WSA ## x))

extern int socket_to_fd(SOCKET sock);
static inline SOCKET fd_to_socket(int fd)
{
    return (SOCKET)_get_osfhandle(fd);
}

extern void socket_init(void);

#else

/* Unix, or something else which implements BSD sockets correctly */

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#ifdef HAVE_NET_ETHERNET_H
# include <net/ethernet.h>
#endif
#ifdef HAVE_LINUX_IF_PACKET_H
# include <linux/if_packet.h>
#endif
#ifdef HAVE_LINUX_NETLINK_H
# include <linux/netlink.h>
#endif
/* Winsuck-compatibility */
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define setsockerr(x)	(errno = (x))
static inline int sock_errno(void)
{
    return errno;
}
#define socke(x)	(x)
#define sockerr(x)      (errno == (x))
#define socket_init()   ((void)0)
#define closesocket(x)  close(x)

#define socket_to_fd(sock) (sock)
#define fd_to_socket(sock) (sock)

#endif


#ifndef HAVE_SA_FAMILY_T
typedef unsigned short int sa_family_t;
#endif

typedef union {
    sa_family_t family;
    struct sockaddr sa;
#ifdef HAVE_STRUCT_SOCKADDR_IN
    struct sockaddr_in sin;
#endif
#ifdef HAVE_STRUCT_SOCKADDR_IN6
    struct sockaddr_in6 sin6;
#endif
#ifdef HAVE_STRUCT_SOCKADDR_UN
    struct sockaddr_un sun;
#endif
#ifdef HAVE_STRUCT_SOCKADDR_LL
    struct sockaddr_ll ll;
#endif
#ifdef HAVE_STRUCT_SOCKADDR_NL
    struct sockaddr_nl nl;
#endif
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE
    struct sockaddr_storage ss;
#endif

} sockaddr_any_t;

#ifdef HAVE_STRUCT_ADDRINFO
typedef struct addrinfo addrinfo_t;
#else
typedef struct my_addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
} addrinfo_t;
#endif

struct sockset {
    struct pollfd *psock;
    unsigned long  nsock;
};

extern int open_socket(int af, const char *spec, const char *defsvc,
		       unsigned int defport, struct sockset *passive);
extern int sockset_accept(struct sockset *set);

static inline int accept_socket(int sockfd) {
    return socket_to_fd(accept(fd_to_socket(sockfd), NULL, 0));
}

#endif /* NETWORK_H */
