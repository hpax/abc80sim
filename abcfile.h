#ifndef ABCFILE_H
#define ABCFILE_H

#include "compiler.h"

void unmangle_filename(char *out, const char *in);
void mangle_filename(char *dst, const char *src);
int mangle_for_readdir(char *dst, const char *src);

#endif /* ABCFILE_H */
