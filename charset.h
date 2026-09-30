#ifndef CHARSET_H
#define CHARSET_H 1

#include "compiler.h"
#include <wchar.h>

extern const wchar_t abc_to_unicode_tbl[257];
extern const wchar_t abc_to_unicode_uc_tbl[257];
extern const wchar_t abc_to_unicode_lc_tbl[257];

static inline wchar_t abc_to_unicode(uint8_t c)
{
    return abc_to_unicode_tbl[c];
}
static inline wchar_t abc_to_unicode_uc(uint8_t c)
{
    return abc_to_unicode_uc_tbl[c];
}
static inline wchar_t abc_to_unicode_lc(uint8_t c)
{
    return abc_to_unicode_lc_tbl[c];
}

extern int unicode_to_abc(int c);

#endif /* CHARSET_H */
