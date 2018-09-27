/*
 * Filename conversion functions
 */

#include "abcfile.h"

#include <wchar.h>

void unmangle_filename(char *dst, const char *src)
{
  static const wchar_t my_tolower[256] =
    L"\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017"
    L"\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"
    L" !\"#¤%&'()*+,-./0123456789:;<=>?"
    L"éabcdefghijklmnopqrstuvwxyzäöåü_"
    L"éabcdefghijklmnopqrstuvwxyzäöåü\377"
    L"\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
    L"\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
    L"\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
    L"\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
    L"\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317"
    L"\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
    L"\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357"
    L"\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377";
  int i;

  wctomb(NULL, 0);

  for (i = 0; i < 8; i++) {
    if (*src != ' ')
      dst += wctomb(dst, my_tolower[(unsigned char)*src]);
    src++;
  }

  if (memcmp(src, "   ", 3) && memcmp(src, "Ufd", 3)) {
    dst += wctomb(dst, L'.');
    for (i = 0; i < 3; i++) {
      if (*src != ' ')
	dst += wctomb(dst, my_tolower[(unsigned char)*src]);
      src++;
    }
  }

  *dst = '\0';
}

void mangle_filename(char *dst, const char *src)
{
  static const wchar_t srcset[] =
    L"0123456789_."
    L"ÉABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÅÜÆØ"
    L"éabcdefghijklmnopqrstuvwxyzäöåüæø";
  static const char dstset[] =
    "0123456789_."
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^[\\"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^[\\";
  char *d;
  const char *s;
  wchar_t sc;
  const wchar_t *scp;
  char dc;
  int n;

  /* Skip any path prefix */
  s = strchr(src, '\0');
  while (--s >= src) {
    if (*s == '/'
#ifdef __WIN32__
	|| *s == ':' || *s == '\\'
#endif
	)
      break;
  }
  s++;

  memset(dst, ' ', 11);
  dst[11] = '\0';

  mbtowc(NULL, NULL, 0);	/* Reset the shift state */

  d = dst;
  while (d < dst+11 && (n = mbtowc(&sc, s, (size_t)~0)) > 0) {
    s += n;

    if ( (scp = wcschr(srcset, sc)) ) {
      dc = dstset[scp - srcset];
    } else {
      dc = '_';
    }

    if ( dc == '.' )
      d = dst+8;
    else
      *d++ = dc;
  }
}


/*
 * Returns length for OK, 0 for failure
 */
int mangle_for_readdir(char *dst, const char *src)
{
  int n;
  const char *s;
  char *d;
  char mangle_buf[12], unmangle_buf[64];

  s = src;

  mangle_filename(mangle_buf, src);
  unmangle_filename(unmangle_buf, mangle_buf);

  if (strcmp(unmangle_buf, src))
    return 0;			/* Not round-trippable */

  /* Compact to 8.3 notation */
  d = dst;
  s = mangle_buf;
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
