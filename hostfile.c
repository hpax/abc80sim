/*
 * File operations related to the host filesystem
 */

#include "compiler.h"
#include "hostfile.h"

static inline enum host_file_mode mode_type(enum host_file_mode mode)
{
    return mode & HF_TYPE_MASK;
}

#define PRIV_MODE	(S_IRUSR|S_IWUSR)
#define FILE_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define DIR_MODE	(FILE_MODE|S_IXUSR|S_IXGRP|S_IXOTH)

#ifdef HAVE__MKDIR
# define make_dir(x) _mkdir(x)
#else
# define make_dir(x) mkdir((x), DIR_MODE)
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

#ifndef O_ACCMODE
# define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR) /* Hope this works */
#endif

#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif
#ifndef O_SHORT_LIVED
# define O_SHORT_LIVED 0
#endif
#ifndef O_DIRECTORY
# define O_DIRECTORY 0
#endif

/* List of all host files */
static struct host_file *list;

/* Common routine to finish the job once we have a name and fd */
static struct host_file *finish_host_file(struct host_file *hf);

static inline int mode_openflags(enum host_file_mode mode)
{
    switch (mode_type(mode)) {
    case HF_BINARY:
	return O_BINARY;
    case HF_TEXT:
	return O_TEXT;
    case HF_UNICODE:
	return UNICODE_O_FLAGS;
    case HF_DIRECTORY:
	return O_DIRECTORY;
    default:
	return 0;
    }
}

#ifndef HAVE__SETMODE
# define _setmode(x,y) ((void)(x), (void)(y))
#endif

static inline bool filename_is_absolute(const char *name)
{
  if (*name == '/')
    return true;

#ifdef __WIN32__
  if (*name == '\\' || strchr(name, ':'))
    return true;
#endif

  return false;
}

int stat_file(const char *dir, const char *filename, struct stat *st)
{
    size_t dl;
    char *path;
    int rv, err;

    if (!dir)
	dir = "";

    dl = strlen(dir);
    asprintf(&path, "%s%s%s", dir,
	     (dl && !is_path_separator(dir[dl-1])) ? "/" : "",
	     filename ? filename : ".");
    if (!path)
	return -1;

    rv = stat(path, st);
    err = errno;
    free(path);
    errno = err;

    return rv;
}

/*
 * Common routine for opening a host file formed from a directory
 * and a file name
 */
struct host_file *open_host_file(enum host_file_mode mode, const char *dir,
				 const char *filename, int openflags)
{
    size_t dl, fl;
    char *p;
    struct host_file *hf;

    if (!dir)
	dir = "";

    if (!filename) {
	filename = ".";
	if (mode_type(mode) != HF_DIRECTORY)
	    mode = HF_FAIL;
    }

    if (mode & HF_FAIL) {
	errno = ENOENT;
	return NULL;
    }

    dl = strlen(dir);
    fl = strlen(filename);

    hf = calloc(sizeof *hf + dl + fl + 1, 1);
    if (!hf)
	return NULL;

    hf->fd        = -1;
    hf->mode      = mode;
    hf->openflags = openflags | mode_openflags(mode);
    hf->nuke      = !!(openflags & O_EXCL);

    p = hf->filename;
    if (dl > 0 && !filename_is_absolute(filename)) {
	p = mempcpy(p, dir, dl);
	if (!is_path_separator(p[-1]))
	    *p++ = '/';
    }
    p = mempcpy(p, filename, fl+1);
    hf->namelen = p - hf->filename;

    if (mode_type(mode) == HF_DIRECTORY) {
	hf->d = opendir(hf->filename);
    } else {
	mode_t filemode = (mode & HF_PRIVATE) ? PRIV_MODE : FILE_MODE;
	for (;;) {
	    hf->fd = open(hf->filename, hf->openflags, filemode);
	    if (hf->fd >= 0 || !(mode & HF_RETRY))
		break;

	    hf->openflags = (hf->openflags & ~O_ACCMODE) | O_RDONLY;
	    hf->openflags &= ~(O_CREAT|O_EXCL|O_APPEND);
	    mode &= ~HF_RETRY;
	}
    }

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

    if (!dir)
	dir = "";

    /* If it is a directory name, try to create it if it doesn't exist */
    if (dir[0])
	make_dir(dir);

    for (n = 1; n <= 9999; n++) {
	asprintf(&filename, pattern, n);
	if (!filename)
	    return NULL;
	hf = open_host_file(mode, dir, filename, openflags);
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

    mode |= HF_PRIVATE;

    if (!prefix)
	prefix = "";

    pfxlen = strlen(prefix);

    hf = calloc(sizeof *hf + pfxlen + 6, 1);
    if (!hf)
	return NULL;

    hf->mode = mode;
    hf->openflags = O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_SHORT_LIVED;
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

struct host_file *temp_file(enum host_file_mode mode)
{
    char *filename = NULL;
    int err;
    int attempts = TMP_MAX;
    size_t namelen;
    const int openflags = O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_SHORT_LIVED;

    mode |= HF_PRIVATE;

    do {
	filename = tempnam(NULL, TEMPFILE_PREFIX);
	if (!filename)
	    return NULL;

	hf = open_host_file(mode, NULL, filename, openflags);
	err = errno;
	free(filename);
    } while (!hf && err == EEXIST && --attempts);

    errno = err;
    return hf;
}

#endif

/* Common routine to finish the job once we have a name and fd */

static struct host_file *finish_host_file(struct host_file *hf)
{
    const char *opt;

    if (!hf)
	return NULL;

    if (hf->mode == HF_DIRECTORY) {
	if (!hf->d)
	    goto err;
    } else {
	if (hf->fd < 0)
	    goto err;

	switch (hf->openflags & O_ACCMODE) {
	case O_RDONLY:
	    opt = "r";
	    break;
	case O_WRONLY:
	    opt = (hf->openflags & O_APPEND) ? "a" : "w";
	    break;
	default:
	    opt = (hf->openflags & O_APPEND) ? "a+" :
		(hf->openflags & (O_CREAT|O_TRUNC)) ? "w+" : "r+";
	    break;
	}

	hf->f = fdopen(hf->fd, opt);
	if (!hf->f)
	    goto err;
    }

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

    if (file->prevp) {
      *file->prevp = file->next; /* Remove from linked list */
      if (file->next)
	  file->next->prevp = file->prevp;
    }

    if (file->d) {
	if (closedir(file->d))
	    err = err ? err : errno;
    } else {
	if (file->f) {
	    if (fclose(file->f))
		err = err ? err : errno;
	    else
		file->fd = -1;	/* fclose() closes the file descriptor too */
	}

	if (file->fd >= 0) {
	    if (close(file->fd))
		err = err ? err : errno;
	}

	if (file->nuke && file->fd >= 0 && file->filename[0]) {
	    if (remove(file->filename))
		err = err ? err : errno;
	}
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

/*
 * Strip the path from a (host) filename
 */
const char *host_strip_path(const char *path)
{
  const char *p;

  p = strrchr(path, '\0');
  while (--p >= path) {
    if (is_path_separator(*p))
      break;
  }

  return p+1;
}
