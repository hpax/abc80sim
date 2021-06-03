#ifndef ABCFILE_H
#define ABCFILE_H

#include "compiler.h"

struct abcdata {
    void *buf;			/* Allocated buffer */
    const void *data;
    size_t len;
    size_t blocks;		/* ABC data blocks */
    bool is_text;
};

void unmangle_filename(char *out, const char *in);
void mangle_filename(char *dst, const char *src);
int mangle_for_readdir(char *dst, const char *src);
int mangle_volname(char *dst, const char *src);
unsigned int init_abcdata(struct abcdata *abc, const void *data, size_t len);
bool get_abc_block(void *block, struct abcdata *abc);
int strcmp_abc(const char *s1, const char *s2);

#endif /* ABCFILE_H */
