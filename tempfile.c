/*
 * Create a temporary file, trying our best to do so safely within
 * the limitations of the operating system we run on...
 */

#include "compiler.h"
#include "tempfile.h"

#define FILE_PREFIX "abc80sim_"

#if defined(__WIN32__) && defined(_O_U16TEXT)
# define UNICODE_O_FLAGS _O_U16TEXT
#else
# define UNICODE_O_FLAGS O_TEXT
#endif

static inline int mode_openflags(enum temp_file_mode mode)
{
    switch (mode) {
    case TF_BINARY:
	return O_BINARY;
    case TF_TEXT:
	return O_TEXT;
    case TF_UNICODE:
	return UNICODE_O_FLAGS;
    default:
	return 0;
    }
}

#ifndef HAVE__SETMODE
# define _setmode(x,y) ((void)(x), (void)(y))
#endif

/* Common routine to finish the job once we have a name and fd */
static struct temp_file *finish_temp_file(struct temp_file *temp);

#if 0 //def HAVE_MKSTEMP

struct temp_file *temp_file(enum temp_file_mode mode)
{
    static const char template[] = FILE_PREFIX "XXXXXX";
    struct temp_file *temp = NULL;

    temp = malloc(sizeof *temp + sizeof template - 1);
    if (!temp)
	return NULL;

    temp->mode = mode;

    memcpy(temp->filename, template, sizeof template);

    temp->fd = mkstemp(temp->filename);
    if (temp->fd < 0) {
	free(temp);
	return NULL;
    }
    _setmode(temp->fd, mode_openflags(mode));

    return finish_temp_file(temp);
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

struct temp_file *temp_file(enum temp_file_mode mode)
{
    struct temp_file *temp;
    char *filename = NULL;
    int fd;
    int err;
    int attempts = TMP_MAX;
    size_t fnlen;
    const int openflags = O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_SHORT_LIVED| \
	mode_openflags(mode);

    do {
	if (filename)
	    free(filename);

	filename = tempnam(NULL, FILE_PREFIX);
	if (!filename)
	    return NULL;

	fd = open(filename, openflags, S_IREAD|S_IWRITE);
    } while (fd < 0 && errno == EEXIST && --attempts);

    if (fd < 0) {
	free(filename);
	return NULL;
    }

    fnlen = strlen(filename);
    temp = malloc(sizeof *temp + fnlen);
    memcpy(temp->filename, filename, fnlen+1);
    temp->fd = fd;
    temp->mode = mode;
    free(filename);

    return finish_temp_file(temp);
}

#endif

/* Common routine to finish the job once we have a name and fd */
static struct temp_file *finish_temp_file(struct temp_file *temp)
{
    temp->f = fdopen(temp->fd, "w+");
    if (!temp->f) {
	int err = errno;
	close(temp->fd);
	remove(temp->filename);
	free(temp);
	errno = err;
	return NULL;
    }
    return temp;
}

int close_temp(struct temp_file **tempp)
{
    struct temp_file *temp;
    int err = 0;

    if (!tempp || !(temp = *tempp))
	return 0;

    if (temp->f)
	err = fclose(temp->f);

    if (temp->filename[0])
	err |= remove(temp->filename);

    free(temp);
    *tempp = NULL;

    return err;
}
