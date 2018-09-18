/*
 * Create a temporary file, trying our best to do so safely within
 * the limitations of the operating system we run on...
 */

#include "compiler.h"
#include "tempfile.h"

#ifdef HAVE_MKSTEMP

FILE *temp_file(char **filenamep, enum temp_file_mode mode)
{
    static const char template[] = "abc80_print_XXXXXX";
    char *filename = NULL;
    int err;
    int fd = -1;
    FILE *f = NULL;

    *filenamep = NULL;

    filename = strdup(template);
    if (!filename)
	goto err;

    fd = mkstemp(filename);
    if (fd < 0)
	goto err;

    f = fdopen(fd, (mode == TF_BINARY) ? "w+b" : "w+t");
    if (!f)
	goto err;

    *filenamep = filename;
    return f;

err:
    err = errno;
    if (fd >= 0) {
	close(fd);
	remove(filename);
    }
    if (filename)
	free(filename);
    errno = err;
    return NULL;
}

#else

/* Hack it with tmpnam() */

#ifndef TMP_MAX
# define TMP_MAX 65536
#endif
#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif
#ifndef O_SHORT_LIVED
# define O_SHORT_LIVED 0
#endif

#if defined(__WIN32__) && defined(_O_U16TEXT)
# define UNICODE_O_FLAGS _O_U16TEXT
#else
# define UNICODE_O_FLAGS O_TEXT
#endif

FILE *temp_file(char **filenamep, enum temp_file_mode mode)
{
    char *filename = NULL;
    int err;
    int fd = -1;
    FILE *f = NULL;
    int attempts = TMP_MAX;
    int openflags;
    static const int mode_openflags[] =
    {
	[TF_BINARY]  = O_BINARY,
	[TF_TEXT]    = O_TEXT,
	[TF_UNICODE] = UNICODE_O_FLAGS
    };

    openflags = O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_SHORT_LIVED;
    openflags |= mode_openflags[mode];

    *filenamep = NULL;

    do {
	if (filename)
	    free(filename);

	filename = tempnam(NULL, "abc80_print_");
	if (!filename)
	    goto err;

	fd = open(filename, openflags, S_IREAD|S_IWRITE);
    } while (fd < 0 && errno == EEXIST && --attempts);

    if (fd < 0)
	goto err;

    f = fdopen(fd, "w+");
    if (!f)
	goto err;

    *filenamep = filename;
    return f;

err:
    err = errno;
    if (fd >= 0) {
	close(fd);
	remove(filename);
    }
    if (filename)
	free(filename);
    errno = err;
    return NULL;
}

#endif
