/*
 * File operations related to the host filesystem
 */

#include "compiler.h"
#include "hostfile.h"

#ifdef HAVE__MKDIR
# define make_dir(x) _mkdir(x)
#else
# define make_dir(x) mkdir((x), 0777);
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif
#ifndef O_TEXT
# define O_TEXT 0
#endif
#if defined(__WIN32__) && defined(_O_U16TEXT)
# define UNICODE_O_FLAGS _O_U16TEXT
#else
# define UNICODE_O_FLAGS O_TEXT
#endif

/* List of all host files */
static struct host_file *list;

/* Common routine to finish the job once we have a name and fd */
static struct host_file *finish_host_file(struct host_file *hf);

static inline int mode_openflags(enum host_file_mode mode)
{
    switch (mode) {
    case HF_BINARY:
	return O_BINARY;
    case HF_TEXT:
	return O_TEXT;
    case HF_UNICODE:
	return UNICODE_O_FLAGS;
    default:
	return 0;
    }
}

#ifndef HAVE__SETMODE
# define _setmode(x,y) ((void)(x), (void)(y))
#endif

static inline bool is_path_separator(char c)
{
  switch (c) {
  case '/':
#ifdef __WIN32__
  case ':':
  case '\\':
#endif
    return true;
  default:
    return false;
  }
}

/*
 * Common routine for opening a host file formed from a directory
 * and a file name
 */
struct host_file *open_host_file(enum host_file_mode mode, const char *dir,
				 const char *filename, int openflags,
				 mode_t filemode)
{
    size_t dl, fl;
    char *p;
    struct host_file *hf;

    if (!dir)
	dir = "";

    dl = strlen(dir);
    fl = strlen(filename);

    hf = malloc(sizeof *hf + dl + fl + 1);
    if (!hf)
	return NULL;

    /* If a creation mode argument is passed, we are trying to create */
    if (filemode)
      openflags |= O_CREAT;
    else
      assert((openflags & (O_CREAT|O_EXCL)) == 0);

    hf->f         = NULL;
    hf->mode      = mode;
    hf->openflags = openflags | mode_openflags(mode);
    hf->nuke      = !!(openflags & O_EXCL);

    p = hf->filename;
    if (dl > 0) {
	p = mempcpy(p, dir, dl);
	if (!is_path_separator(p[-1]))
	    *p++ = '/';
    }
    p = mempcpy(p, filename, fl+1);
    hf->namelen = p - hf->filename;
    hf->fd = open(hf->filename, hf->openflags, filemode);

    return finish_host_file(hf);
}

/*
 * Create a numbered dump file for writing (only)
 */
struct host_file *
dump_file(enum host_file_mode mode, const char *dir, const char *pattern)
{
    int err;
    unsigned int n;
    struct host_file *hf;
    char *filename;
    const int openflags = O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW;

    /* If it is a directory name, try to create it if it doesn't exist */
    if (dir[0])
	make_dir(dir);

    for (n = 1; n <= 9999; n++) {
	asprintf(&filename, pattern, n);
	if (!filename)
	    return NULL;
	hf = open_host_file(mode, dir, filename, openflags, FILE_MODE);
	err = errno;
	free(filename);
	errno = err;

	if (hf || err != EEXIST)
	    return hf;
    }

    return hf;
}

#ifdef HAVE_MKSTEMP

struct host_file *temp_file(enum host_file_mode mode, const char *prefix)
{
    struct host_file *hf = NULL;
    size_t pfxlen;

    if (!prefix)
	prefix = "";

    pfxlen = strlen(prefix);

    hf = malloc(sizeof *hf + pfxlen + 6);
    if (!hf)
	return NULL;

    hf->mode = mode;
    hf->openflags = O_RDWR|O_CREAT|O_EXCL;
    hf->nuke = true;
    hf->namelen = pfxlen + 6;
    memcpy(hf->filename, prefix, pfxlen);
    memcpy(hf->filename + pfxlen, "XXXXXX", 7);

    hf->fd = mkstemp(hf->filename);
    if (hf->fd < 0) {
	free(hf);
	return NULL;
    }
    _setmode(hf->fd, mode_openflags(mode));

    return finish_host_file(hf);
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

struct host_file *temp_file(enum host_file_mode mode)
{
    char *filename = NULL;
    int err;
    int attempts = TMP_MAX;
    size_t namelen;
    const int openflags = O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_SHORT_LIVED;

    do {
	filename = tempnam(NULL, TEMPFILE_PREFIX);
	if (!filename)
	    return NULL;

	hf = open_host_file(NULL, filename, openflags, PVT_MODE);
	err = errno;
	free(filename);
    } while (!hf && err == EEXIST && --attempts);

    errno = err;
    return hf;
}

#endif

/* Common routine to finish the job once we have a name and fd */

#ifndef O_ACCMODE
# define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR) /* Hope this works */
#endif

static struct host_file *finish_host_file(struct host_file *hf)
{
    const char *opt;

    if (!hf)
	return NULL;

    if (hf->fd < 0)
	goto err;

    switch (hf->openflags & O_ACCMODE) {
    case O_RDONLY:
	opt = "r";
	break;
    case O_WRONLY:
	opt = "w";
	break;
    default:
	opt = (hf->openflags & O_CREAT) ? "w+" : "r+";
	break;
    }

    hf->f = fdopen(hf->fd, opt);
    if (!hf->f)
	goto err;

    hf->next  = list;
    hf->prevp = &list;
    if (list)
	list->prevp = &hf->next;
    list = hf;

    return hf;
    err:
	close_file(&hf);
	return NULL;
}

/* This function returns errno on failure, the errno variable is preserved */
int close_file(struct host_file **filep)
{
    struct host_file *file;
    int old_errno = errno;
    int err = 0;

    if (!filep || !(file = *filep))
	return 0;

    if (file->prevp)
      *file->prevp = file->next; /* Remove from linked list */

    if (file->f) {
	if (fclose(file->f))
	    err = errno;
	else
	    file->fd = -1;	/* fclose() closes the file descriptor too */
    }

    if (file->fd >= 0) {
	if (close(file->fd))
	    err = err ? err : errno;
    }

    if (file->nuke && file->filename[0]) {
	if (remove(file->filename))
	    err = err ? err : errno;
    }

    free(file);
    *filep = NULL;
    errno = old_errno;

    return err;
}

static void hostfile_cleanup(void)
{
    struct host_file *hf, *next;
    for (hf = list; hf; hf = next) {
	next = hf->next;
	close_file(&hf);
    }
}

void hostfile_init(void)
{
    atexit(hostfile_cleanup);
}
