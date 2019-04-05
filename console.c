/*
 * console.c
 *
 * Manage attach/detach of a text console for stdin, stdout, stderr
 */

#include "console.h"

#ifndef O_TEXT
#define O_TEXT 0
#endif

static int redirect_stdio(const char *in, const char *out)
{
    int err = 0;
    int infd = -1;
    int outfd = -1;

    fflush(NULL);

    infd = open(in, O_RDWR | O_TEXT);
    if (infd >= 0) {
        if (!out)
            outfd = infd;
    } else {
        infd = open(in, O_RDONLY | O_TEXT);
    }

    if (infd < 0) {
        err = -1;
    } else {
        dup2(infd, STDIN_FILENO);
    }

    if (outfd < 0) {
        out = out ? out : in;
        outfd = open(out, O_RDWR | O_TEXT);
        if (outfd < 0)
            outfd = open(out, O_WRONLY | O_TEXT);
    }

    if (outfd < 0) {
        err = -1;
    } else {
        dup2(outfd, STDOUT_FILENO);
        dup2(outfd, STDERR_FILENO);
    }

    if (infd > STDERR_FILENO)
        close(infd);

    if (outfd != infd && outfd > STDERR_FILENO)
        close(outfd);

    return err;
}

#ifdef __WIN32__

void detach_console(void)
{
    redirect_stdio("\\Device\\Null", NULL);
    FreeConsole();
}

#else /* not __WIN32__ */

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#ifndef HAVE_SETSID
#define setsid() ((void)0)
#endif

void detach_console(void)
{
    pid_t pid;

    redirect_stdio(_PATH_DEVNULL, NULL);

    pid = fork();

    if (pid < 0)
        return;
    else if (pid > 0)
        _exit(0);

    setsid();
}

#endif
