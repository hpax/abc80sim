/*
 * Create a temporary file, trying our best to do so safely within
 * the limitations of the operating system we run on...
 */

#include "compiler.h"
#include "tempfile.h"

#if 0 //def HAVE_MKSTEMP

FILE *temp_file(char **filenamep, const char *mode)
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

    f = fdopen(fd, mode);
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

FILE *temp_file(char **filenamep, const char *mode)
{
    char *filename = NULL;
    int err;
    int fd = -1;
    FILE *f = NULL;
    int attempts = TMP_MAX;

    *filenamep = NULL;

    do {
	if (filename)
	    free(filename);

	filename = tempnam(NULL, "abc80_print_");
	if (!filename)
	    goto err;

	fd = open(filename,
		  O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_BINARY|O_SHORT_LIVED,
		  S_IREAD|S_IWRITE);
    } while (fd < 0 && errno == EEXIST && --attempts);

    if (fd < 0)
	goto err;

    f = fdopen(fd, mode);
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
