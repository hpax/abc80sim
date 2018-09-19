#ifndef HOSTFILE_H
#define HOSTFILE_H

#include "compiler.h"

enum host_file_mode {
    HF_BINARY    = 0,		/* Raw binary */
    HF_TEXT      = 1,		/* Text mode compatible with ASCII */
    HF_UNICODE   = 2		/* Platform preferred Unicode encoding */
};

#define TMPFILE_MODE	(S_IRUSR|S_IWUSR)
#define FILE_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define DIR_MODE	(FILE_MODE|S_IXUSR|S_IGXGRP|S_IXOTH)

struct host_file {
    FILE *f;
    struct host_file **prevp, *next;
    size_t namelen;
    int fd;
    int openflags;
    bool nuke;
    enum host_file_mode mode;
    char filename[1];
};

/* This file should now be kept, not deleted on close */
static inline void keep_file(struct host_file *file)
{
    file->nuke = false;
}

/* Open a host filesystem file */
extern struct host_file *
open_host_file(enum host_file_mode mode, const char *dir,
	       const char *filename, int openflags, mode_t filemode);

/* Create a numbered dump file */
extern struct host_file *
dump_file(enum host_file_mode mode, const char *dir, const char *pattern);

/* Create a temporary file */
extern struct host_file *
temp_file(enum host_file_mode mode, const char *prefix);

/* Close and optionally delete a host file */
extern int close_file(struct host_file **temp);

/* Initialize the hostfile subsystem */
void hostfile_init(void);

#endif /* HOSTFILE_H */
