/*
 * Filename and file data conversion functions
 */

#include "abcfile.h"
#include "hostfile.h"           /* For host_strip_path() */
#include "charset.h"

#include <wchar.h>

static void unmangle(char *dst, const char *src, const wchar_t *table)
{
    int i;
    mbstate_t ps;

    memset(&ps, 0, sizeof ps);

    for (i = 0; i < 8; i++) {
        if (*src != ' ')
            dst += wcrtomb(dst, table[(unsigned char)*src], &ps);
        src++;
    }

    if (memcmp(src, "   ", 3) && memcmp(src, "Ufd", 3)) {
        dst += wcrtomb(dst, L'.', &ps);
        for (i = 0; i < 3; i++) {
            if (*src != ' ')
                dst += wcrtomb(dst, table[(unsigned char)*src], &ps);
            src++;
        }
    }

    *dst = '\0';
}

void unmangle_filename(char *dst, const char *src)
{
    unmangle(dst, src, abc_to_unicode_lc_tbl);
}

void mangle_filename(char *dst, const char *src)
{
    static const wchar_t srcset[] =
        L"0123456789."
        L"ÉABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÅÜÆØ"
        L"éabcdefghijklmnopqrstuvwxyzäöåüæø";
    static const char dstset[] =
        "0123456789."
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^[\\"
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^[\\";
    char *d;
    const char *s;
    wchar_t sc;
    const wchar_t *scp;
    char dc;
    int n;
    mbstate_t ps;

    /* Skip any path prefix */
    s = host_strip_path(src);

    memset(dst, ' ', 11);
    dst[11] = '\0';

    memset(&ps, 0, sizeof ps);	/* Reset the shift state */

    d = dst;
    while (d < dst + 11) {
	n = mbrtowc(&sc, s, MB_LEN_MAX, &ps);
	if (n <= 0)
	    break;

        s += n;

        if ((scp = wcschr(srcset, sc))) {
            dc = dstset[scp - srcset];
        } else {
            dc = '_';
        }

        if (dc == '.')
            d = dst + 8;
        else
            *d++ = dc;
    }
}

/*
 * Returns length for OK, 0 for failure
 *
 * Similar to mangle_filename(), but return "FILE.EXT" instead of
 * "FILE    EXT", and verifies that the filename is round-trip-safe.
 */
int mangle_for_readdir(char *dst, const char *src)
{
    int n;
    const char *s;
    char *d;
    char mangle_buf[12];
    unmangled_name unmangle_buf;

    mangle_filename(mangle_buf, src);
    unmangle_filename(unmangle_buf, mangle_buf);

    if (strcmp(unmangle_buf, src))
        return 0;               /* Not round-trippable */

    d = dst;
    s = mangle_buf;

    /* Compact to 8.3 notation */
    for (n = 0; n < 8; n++) {
        if (*s != ' ')
            *d++ = *s;
        s++;
    }
    if (memcmp(s, "   ", 3)) {
        *d++ = '.';
        for (n = 0; n < 3; n++) {
            if (*s != ' ')
                *d++ = *s;
            s++;
        }
    }
    *d = '\0';

    return d - dst;
}

/*
 * Similar to mangle_filename(), but for volume names; the input must
 * be VOL in with the first character being alphabetic [A-ZÄÖÅ]; only
 * 1-3 characters allowed. If uconly is set, the name must be in upper
 * case.
 */
enum volname_ok mangle_volname(char *dst, const char *src)
{
    char mangle_buf[12];
    unmangled_name unmangle_buf;

    mangle_filename(mangle_buf, src);

    if (mangle_buf[0] < 'A' || mangle_buf[0] > ']')
	return VOL_ERR;

    if (memcmp(mangle_buf+3, "        ", 8))
	return VOL_ERR;

    unmangle(unmangle_buf, mangle_buf, abc_to_unicode_uc_tbl);

    memcpy(dst, mangle_buf, 3);
    dst[3] = '\0';

    return strcmp(unmangle_buf, src) ? VOL_ONEWAY : VOL_OK;
}

/*
 * Check a memory-mapped file or memory buffer to see if it appears to
 * be a conventional text file, as opposed to a binary file or a text
 * file in ABC-binary format.  The heuristic used is that a file
 * that contains a NUL or ETX byte, or any byte with the high bit set
 * is assumed to be binary.
 *
 * Initialize a struct abcdata with the resulting information.
 * Returns the number of ABC blocks that this buffer will produce.
 */
unsigned int init_abcdata(struct abcdata *abc, const void *data, size_t len)
{
    const uint8_t *p = data;
    size_t left = len;
    size_t cc = 0;

    if (!data) {
	/* abc->buf and abc->len already initialized */
	data = abc->buf;
	len = abc->len;
    } else {
	abc->buf = NULL; /* Can be set by caller if it needs freeing */
	abc->len = len;
    }
    abc->data = data;
    abc->is_text = false;

    cc = 0;
    while (left--) {
        uint8_t c = *p++;

        if (c >= 0x80 || c == 0 || c == 3) {
            /* Binary file */
            return abc->blocks = (len + 252) / 253;   /* Just the data */
        }
        cc += (c != '\r');
    }

    abc->is_text = true;
    /* Each block will need ETX + EOF block */
    return abc->blocks = (cc + 251) / 252 + 1;
}

/*
 * Build an ABC data block from a memory buffer containing either
 * a binary file or a conventional text file in memory.
 * Return true if this is the final (EOF) block.
 */
bool get_abc_block(void *block, struct abcdata *abc)
{
    size_t l = abc->len;
    const uint8_t *p = abc->data;
    uint8_t *q = block;
    size_t ob;
    bool done;

    if (!abc->is_text) {
        /* It is a binary file */

        ob = l < 253 ? l : 253;

        memcpy(q, p, ob);
        p += ob;
        l -= ob;
        done = !l;              /* If no more data this is the last block */
    } else {
        /* It is a text file */

        ob = 0;
        done = false;

        while (l && ob < 252) {
            uint8_t c = *p++;
            l--;

            /* Convert CR LF or LF -> CR */
            switch (c) {
            case '\r':
                break;
            case '\n':
                c = '\r';
                /* fall through */
            default:
                q[ob++] = c;
                break;
            }
        }

        if (!ob) {
            /* This is apparently the EOF block */
            memset(q, 0, ob = 6);
            done = true;
        }

        q[ob++] = 0x03;         /* ETX = end of block */
    }

    if (ob < 253)
        memset(q + ob, 0, 253 - ob);

    abc->data = p;
    abc->len = l;
    return done;
}

/*
 * Comparison function for strings in ABC format; this is simply an
 * ASCII sort except that [ \ ] and { | } are permuted to match å, ä, ö
 */
pure_func int strcmp_abc(const char *s1, const char *s2)
{
    int rv = 0;

    while (1) {
	unsigned char a = *s1++;
	unsigned char b = *s2++;
	rv = a - b;

	if (!a || !b)
	    break;

	if (!rv)
	    continue;

	if ((a | 0x20) == '}') {
	    if (rv == 1 || rv == 2)
		rv -= 3;
	} else if ((b | 0x20) == '}') {
	    if (rv == -1 || rv == -2)
		rv += 3;
	}
	break;
    }

    return rv;
}
