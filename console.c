/*
 * console.c
 *
 * Manage attach/detach of a text console for stdin, stdout, stderr
 */

#include "console.h"

#if defined(HAVE_ATTACHCONSOLE) || !defined(HAVE_DAEMON)

static int redirect_stdio(const char *whereto)
{
    int fd = open(whereto, O_RDWR);
    if (fd < 0)
	return -1;

    fflush(NULL);

    dup2(fd, STDERR_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDIN_FILENO);

    if (fd > STDERR_FILENO)
	close(fd);

    return 0;
}

#endif

#ifdef HAVE_ATTACHCONSOLE

/* A Windows GUI app detaches from the console by default */
void attach_console(void)
{
    atexit(detach_console);

    if (!AttachConsole(ATTACH_PARENT_PROCESS))
	return;			/* Attach failed */

    if (redirect_stdio("\\\\?\\CON:"))
	detach_console();
}

void detach_console(void)
{
    redirect_stdio("\\\\?\\NUL:");
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
