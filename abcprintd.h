#ifndef ABCPRINTD_H
#define ABCPRINTD_H

#include "compiler.h"

#include <ctype.h>
#include <locale.h>
#include <wchar.h>

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
