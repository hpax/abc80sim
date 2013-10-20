#ifndef ABCPRINTD_H
#define ABCPRINTD_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef O_TEXT
# define O_TEXT	0
#endif
#ifndef O_BINARY
# define O_BINARY 0
#endif

extern int lpr_argc;
extern const char **lpr_argv;
extern void abcprint_init(void);
extern void abcprint(const void *, size_t);
extern int abcprint_read(void);
extern int abcprint_poll(void);
extern bool file_op(unsigned char);
extern const char *fileop_prefix;

#endif
