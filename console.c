/*
 * console.c
 *
 * Manage attach/detach of a text console for stdin, stdout, stderr
 */

#include "console.h"

#if defined(HAVE_ATTACHCONSOLE) || !defined(HAVE_DAEMON)

static int redirect_stdio(const char *from, const char *to)
{
    bool err = false;

    fflush(NULL);

    if (!freopen(from, "r+t", stdin))
	err = !freopen(from, "r+t", stdin);

    if (!freopen(to, "w+t", stdout))
	err |= !freopen(to, "wt", stdout);

    if (!freopen(to, "w+t", stderr))
	err |= !freopen(to, "wt", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);

    return -err;
}

#endif

#ifdef HAVE_ATTACHCONSOLE

/* A Windows GUI app detaches from the console by default */
void attach_console(void)
{
    atexit(detach_console);

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
	return;			/* Attach failed */

    if (redirect_stdio("CONIN$", "CONOUT$"))
	detach_console();

    /* We are probably displaying a command prompt, so start with a newline */
    putchar('\n');
}

void detach_console(void)
{
    redirect_stdio("\\Device\\Null", "\\Device\\Null");
    FreeConsole();
}

#else

void attach_console(void)
{
    /* Do nothing */
}

# ifdef HAVE_DAEMON

void detach_console(void)
{
    daemon(true, false);
}

# elif defined(HAVE_FORK) || defined(HAVE_VFORK)

#  ifndef _PATH_DEVNULL
#   define _PATH_DEVNULL "/dev/null"
#  endif

#  ifndef HAVE_SETSID
#   define setsid() ((void)0)
#  endif

void detach_console(void)
{
    pid_t pid;

    redirect_stdio(_PATH_DEVNULL);

    pid = vfork();

    if (pid < 0)
	return;
    else if (pid > 0)
	_exit(0);

    setsid();
}

# endif
#endif
