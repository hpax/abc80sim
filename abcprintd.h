#ifndef ABCPRINTD_H
#define ABCPRINTD_H

#include "compiler.h"

/* abcprint instance */
struct abcprint;

typedef size_t (*send_func)(void *, const void *, size_t);

extern struct abcprint *abcprint_init(send_func, void *);
extern void abcprint_reset(struct abcprint *);
extern void abcprint_recv(struct abcprint *, const void *, size_t);
extern const char *fileop_path, *lpr_command;
extern FILE *console_file;

#endif
