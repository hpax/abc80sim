#ifndef HOSTFILE_H
#define HOSTFILE_H

#include "compiler.h"

static inline bool is_stdio(const char *filename)
{
    return !filename || !filename[0] || (filename[0] == '-' && !filename[1]);
}

enum host_file_mode {
    HF_BINARY = 0,              /* Raw binary */
    HF_TEXT = 1,                /* Text mode compatible with ASCII */
    HF_UNICODE = 2,             /* Platform preferred Unicode encoding */
    HF_DIRECTORY = 3,           /* opendir() on a directory */
    HF_TYPE_MASK = 0x0f,

    HF_PRIVATE = 0x10,          /* Only user permissions */
    HF_RETRY = 0x20,            /* If O_RDWR retry with O_RDONLY on failure */
    HF_FAIL = 0x40              /* Don't actually try to open, return ENOENT */
};

struct host_file {
    FILE *f;
    DIR *d;
    struct host_file **prevp, *next;
    const char *filename;
    off_t filesize;
    int fd;
    int openflags;
    bool nuke;			/* Delete file on close */
    enum host_file_mode mode;
    uint8_t *map;               /* Memory-mapped contents */
    size_t mlen;                /* Length of memory map */
    size_t flen;                /* Length of true file in memory map */
#ifdef _WIN32
    HANDLE maphandle;           /* Special Windows drain bramage */
#endif
};

/* This file should now be kept, not deleted on close */
static inline void keep_file(struct host_file *file)
{
    file->nuke = false;
}

/* Mark file for delete on close */
static inline void nuke_file(struct host_file *file)
{
    file->nuke = true;
}

static inline bool is_path_separator(char c)
{
    switch (c) {
    case '/':
#ifdef _WIN32
    case ':':
    case '\\':
#endif
        return true;
    default:
        return false;
    }
}

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)    /* Hope this works */
#endif

static inline bool file_rdok(struct host_file *hf)
{
    return (hf->openflags & O_ACCMODE) != O_WRONLY;
}

static inline bool file_wrok(struct host_file *hf)
{
    return (hf->openflags & O_ACCMODE) != O_RDONLY;
}

/* Open a host filesystem file */
extern struct host_file *open_host_file(enum host_file_mode mode,
                                        const char *dir, const char *filename,
                                        int openflags);

/* Map (or read) the file contents into memory */
extern void *map_file(struct host_file *file, size_t len);

/* Create a numbered dump file */
extern struct host_file *
dump_file(enum host_file_mode mode, const char *path, const char *dir,
	  const char *prefix, const char *suffix);

/* Create a temporary file */
extern struct host_file *temp_file(enum host_file_mode mode,
                                   const char *prefix);

/* Write contents back to disk if necessary */
extern void flush_file(struct host_file *file);

/* Close and optionally delete a host file */
extern int close_file(struct host_file **temp);

/* Stat a combined path in the filesystem */
extern int stat_file(const char *dir, const char *filename, struct stat *st);

/* Check for special filenames, to be avoided. Do not include a path. */
extern bool special_filename(const char *filename);

/* Rewind a file or directory */
extern void rewind_file(struct host_file *file);

/* Read a directory */
extern struct dirent *read_dir(struct host_file *file);

/* Initialize the hostfile subsystem */
extern void hostfile_init(void);

/* Point to a filename, without any path */
extern const char *host_strip_path(const char *path);

/* Combine a directory and filename */
extern char *concat_path(const char *dir, const char *file);

/*
 * Rename an open file within a directory, with overwrite semantics
 * (if possible)
 */
extern int rename_file(struct host_file *hf, const char *newname);

/* Simple linked list of filenames */
struct file_node;
struct file_list {
    struct file_node *first, *last;
};

extern void filelist_add_file(struct file_list *, const char *, int);
extern void filelist_add_list(struct file_list *, const char *, int);
extern void filelist_free(struct file_list *);
extern char *filelist_peek(struct file_list *, int *);
extern char *filelist_pop(struct file_list *, int *);

#define PRIV_MODE	(S_IRUSR|S_IWUSR)
#define FILE_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define DIR_MODE	(FILE_MODE|S_IXUSR|S_IXGRP|S_IXOTH)

#ifdef HAVE__MKDIR
#define make_dir(x) _mkdir(x)
#else
#define make_dir(x) mkdir((x), DIR_MODE)
#endif

#endif /* HOSTFILE_H */
