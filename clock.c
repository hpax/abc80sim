#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>

#include "z80.h"
#include "screen.h"

extern int flush_pending;

extern int events_in_queue;
extern int event_pending;
extern int xfd;
static fd_set fds;
static struct timeval select_timeout;


/*
 * Trig a non maskable interrupt in the Z80 on the clock signal.
 */
/*ARGSUSED*/
static void
clock_handler(int sig, int code, struct sigcontext *scp, char *addr)
{

    flush_pending = 1;
    z80_state.nminterrupt = 1;

    select(FD_SETSIZE, &fds, NULL, NULL, &select_timeout );
    if (FD_ISSET(xfd, &fds)) {
        events_in_queue = 1;
    } else {
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
    }
    event_pending = events_in_queue;

    /*
     * For systems which have "oneshot" as default.
     */
    signal(SIGVTALRM, clock_handler);
}


/*
 * Set up signal handler and schedule a signal every 20 ms.
 */
void
clock_init(void)
{
    struct itimerval timer;

    signal(SIGVTALRM, clock_handler);

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 20000;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 20000;
    setitimer(ITIMER_VIRTUAL, &timer, NULL);

    FD_ZERO(&fds);
    FD_SET(xfd, &fds);
    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = 0;
}
