#ifndef ABCFILE_H
#define ABCFILE_H

#include "compiler.h"

/*
 * A buffer size large enough to be able to handle any unmangled ABC
 * filename (8.3) - *including* characters possibly extended to
 * multibyte characters and a final null. This assumes a maximum of 4
 * bytes/character, which should be very conservative even with UTF-8.
 */
#define UNMANGLED_NAME_BUFSIZE 48
typedef char unmangled_name[UNMANGLED_NAME_BUFSIZE];

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
enum volname_ok {
    VOL_OK,			/* Valid volume name */
    VOL_ONEWAY,			/* Valid volume name, but not invertible */
    VOL_ERR			/* Invalid volume name */
};
enum volname_ok mangle_volname(char *dst, const char *src);
unsigned int init_abcdata(struct abcdata *abc, const void *data, size_t len);
bool get_abc_block(void *block, struct abcdata *abc);
pure_func int strcmp_abc(const char *s1, const char *s2);

#endif /* ABCFILE_H */
