#ifndef ABCPRINTD_H
#define ABCPRINTD_H

#include "compiler.h"

#include <ctype.h>
#include <locale.h>
#include <wchar.h>

extern void abcprint_init(void);
extern void abcprint(const void *, size_t);
extern int abcprint_read(void);
extern int abcprint_poll(void);
extern bool file_op(unsigned char);
extern const char *fileop_path, *lpr_command;

#endif
