#include "abcprintd.h"

const char *fileop_prefix;

#define BUF_SIZE 512

static unsigned char output_buf[BUF_SIZE];
static int output_head, output_tail;

static char *make_path(const char *filename)
{
  char *p;
  int pl = strlen(fileop_prefix);
  int fl = strlen(filename);

  p = malloc(pl + fl + 2);
  if (!p) {
    perror("malloc");
    exit(1);
  }

  sprintf(p, "%s%s%s", fileop_prefix,
	  (pl > 0 &&
#ifdef WIN32
	   fileop_prefix[pl-1] != ':' &&
	   fileop_prefix[pl-1] != '\\' &&
#endif
	   fileop_prefix[pl-1] != ':') ? "/" : "",
	  filename);

  return p;
}

int abcprint_read(void)
{
  int c;

  if (output_head == output_tail) {
    return -1;
  } else {
    c = output_buf[output_head];
    output_head = (output_head + 1) % BUF_SIZE;
    return c;
  }
}

int abcprint_poll(void)
{
  return output_head != output_tail;
}

static void output_bytes(const void *buf, size_t count)
{
  const unsigned char *bp = buf;

  while (count--) {
    int nt = (output_tail + 1) % BUF_SIZE;

    if (nt == output_head)
      return;			/* Output buffer full - data lost */

    output_buf[output_tail] = *bp++;
    output_tail = nt;
  }
}

static enum {
  st_op,
  st_filename,
  st_len,
  st_data
} state = st_op;
static int byte_count = 4;
static unsigned char cmd[4];
static unsigned char *bytep = cmd;
static struct file_data *filemap[65536];

struct file_data {
  FILE *f;
  DIR *d;
};

static void send_reply(int status)
{
  unsigned char reply[4];

  reply[0] = 0xff;
  reply[1] = cmd[0];
  reply[2] = cmd[1];
  reply[3] = status;

  output_bytes(reply, 4);
}

/* Returns the status code, use send_reply(do_close(ix)) if reply desired */
static int do_close(uint16_t ix)
{
  struct file_data *fm = filemap[ix];

  if (fm) {
    if (fm->f)
      fclose(fm->f);
    if (fm->d)
      closedir(fm->d);
    free(fm);
    filemap[ix] = NULL;
    return 0;
  } else {
    return 128+45;		/* "Fel logiskt filnummer" */
  }
}

static void do_closeall(void)
{
  int ix;

  for (ix = 0; ix <= 65535; ix++)
    if (filemap[ix])
      do_close(ix);

  if (cmd[0] == 0xA8)
    send_reply(0);
}

static void unmangle_filename(char *out, const char *in)
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
    if (*in != ' ')
      out += wctomb(out, my_tolower[(unsigned char)*in]);
    in++;
  }

  if (memcmp(in, "   ", 3) && memcmp(in, "Ufd", 3)) {
    out += wctomb(out, L'.');
    for (i = 0; i < 3; i++) {
      if (*in != ' ')
	out += wctomb(out, my_tolower[(unsigned char)*in]);
      in++;
    }
  }

  *out = '\0';
}

static void do_open(uint16_t ix, char *name)
{
  int err;
  struct file_data *fm;
  static const char *modes[6] = {"r+t", "r+b", "w+t", "w+b", "rt", "rb" };
  char path_buf[64];
  char *path;

  if (!fileop_prefix) {
    send_reply(128+42);		/* Skivan ej klar */
    return;
  }

  do_close(ix);

  unmangle_filename(path_buf, name);
  path = make_path(path_buf[0] ? path_buf : ".");
  if (!path) {
    send_reply(128+42);
    return;
  }

  fm = calloc(1, sizeof(struct file_info *));
  if (!fm) {
    free(path);
    send_reply(128+42);
    return;
  }

  if (!path_buf[0]) {
    /* Empty filename (readdir) */

    if ((cmd[0] & 3) == 0) {
      fm->d = opendir(path);
    } else {
      errno = ENOENT;	/* File not found */
    }
  } else {
    /* Actual filename */

    fm->f = fopen(path, modes[cmd[0] & 3]);
    if (!fm->f && errno == EACCES && !(cmd[0] & 2)) {
      fm->f = fopen(path, modes[(cmd[0] & 1)+4]);
    }
  }

  free(path);

  if (!fm->f && !fm->d) {
    free(fm);
    switch (errno) {
#if 0				/* Enable this? */
    case EACCES:
      err = 128+39;
      break;
    case EROFS:
      err = 128+43;
      break;
    case EIO:
    case ENOTDIR:
      err = 128+48;
      break;
#endif
    default:
      err = 128+21;
      break;
    }
    send_reply(err);
    return;
  }

  filemap[ix] = fm;

  send_reply(0);
}

static void do_read_block(uint16_t ix, uint16_t len)
{
  struct file_data *fm = filemap[ix];
  int err;
  unsigned char *data;
  int dlen;

  if (!fm || !fm->f) {
    send_reply(128+45);
    return;
  }

  data = calloc(len+2, 1);
  if (!data) {
    send_reply(128+42);		/* "Skivan ej klar" */
    return;
  }

  clearerr(fm->f);
  dlen = fread(data+2, 1, len, fm->f);
  if (dlen == 0) {
    if (ferror(fm->f)) {
      switch (errno) {
      case EBADF:
	err = 128+44;		/* Logisk fil ej öppen */
	break;
      case EIO:
	err = 128+35;		/* Checksummafel vid läsning */
	break;
      default:
	err = 128+48;		/* Fel i biblioteket */
	break;
      }
    } else {
      /* EOF */
      err = 128+34;		/* Slut på filen */
    }
    send_reply(err);
    free(data);
    return;
  }

  send_reply(0);

  data[0] = len;
  data[1] = len >> 8;
  output_bytes(data, len+2);
  free(data);
}

/*
 * Returns length for OK, 0 for failure
 */
static int mangle_for_readdir(char *dst, const char *src)
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
  char mangle_buf[12], unmangle_buf[64];

  s = src;

  memset(mangle_buf, ' ', 11);
  mangle_buf[11] = '\0';

  mbtowc(NULL, NULL, 0);	/* Reset the shift state */

  d = mangle_buf;
  while (d < mangle_buf+11 && (n = mbtowc(&sc, s, (size_t)~0)) > 0) {
    s += n;

    if ( (scp = wcschr(srcset, sc)) ) {
      dc = dstset[scp - srcset];
    } else {
      dc = '_';
    }

    if ( dc == '.' )
      d = mangle_buf+8;
    else
      *d++ = dc;
  }

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


static void do_input(uint16_t ix)
{
  struct file_data *fm = filemap[ix];
  int err;
  char data[BUF_SIZE], data1[2*BUF_SIZE];
  char *path;
  char *p, *q, c;
  int dlen;
  struct dirent *de;
  struct stat st;

  if (!fm) {
    send_reply(128+45);
    return;
  }

  if (fm->f) {
    clearerr(fm->f);
    if (!fgets(data, sizeof data, fm->f)) {
      if (ferror(fm->f)) {
	switch (errno) {
	case EBADF:
	  err = 128+44;		/* Logisk fil ej öppen */
	  break;
	case EIO:
	  err = 128+35;		/* Checksummafel vid läsning */
	  break;
	default:
	  err = 128+48;		/* Fel i biblioteket */
	  break;
	}
      } else {
	/* EOF */
	err = 128+34;		/* Slut på filen */
      }
    } else {
      /* Strip CR and change LF -> CR LF */
      for (p = data, q = data1+2 ; (c = *p) ; p++) {
	switch (c) {
	case '\r':
	  break;
	case '\n':
	  *q++ = '\r';
	  /* fall through */
	default:
	  *q++ = c;
	  break;
	}
      }
      dlen = q - (data1+2);
      err = 0;
    }
  } else if (fm->d) {
      while ( (de = readdir(fm->d)) ) {
	if (de->d_name[0] != '.' &&
	    (dlen = mangle_for_readdir(data1+2, de->d_name))) {
	  bool ok;
	  path = make_path(de->d_name);
	  if (!path) {
	    err = 128+42;
	    goto err;
	  }
	  ok = !stat(path, &st) && S_ISREG(st.st_mode);
	  free(path);
	  if (ok)
	    break;
	}
      }
      if (de) {
	dlen += sprintf(data1+2+dlen, ",%lu\r\n",
			((unsigned long)st.st_size + 252)/253);
	err = 0;
      } else {
	err = 128+34;
      }
  } else {
    err = 128+44;
  }

 err:
  send_reply(err);
  if (!err) {
    data1[0] = dlen;
    data1[1] = dlen >> 8;
    output_bytes(data1, dlen+2);
  }
}

static void do_print(uint16_t ix, uint16_t len, void *data)
{
  struct file_data *fm = filemap[ix];
  int err;

  if (!fm || !fm->f) {
    send_reply(128+45);
    return;
  }

  err = 0;
  if (fwrite(data, 1, len, fm->f) != len || fflush(fm->f)) {
    switch (errno) {
    case EACCES:
      err = 128+39;		/* Filen skrivskyddad */
      break;
    case ENOSPC:
    case EFBIG:
      err = 128+41;		/* Skivan full */
      break;
    case EROFS:
      err = 128+43;		/* Skivan skrivskyddad */
      break;
    case EBADF:
      err = 128+44;		/* Logisk fil ej öppen */
      break;
    case EIO:
      err = 128+36;		/* Checksummafel vid skrivning */
      break;
    default:
      err = 128+48;		/* Fel i biblioteket */
      break;
    }
  }
  send_reply(err);
}

bool file_op(unsigned char c)
{
  static unsigned char *data;
  static char namebuf[12];
  static unsigned char lenbuf[2];
  uint16_t ix, len;

  *bytep++ = c;
  if (--byte_count)
    return true;		/* More to do... */

  /* Otherwise, we have a full deck of *something* */

  ix  = (cmd[3] << 8) + cmd[2];
  len = (lenbuf[1] << 8) + lenbuf[0];

  switch (state) {
  case st_op:
    switch (cmd[0]) {
    case 0xA0:			/* OPEN TEXT */
    case 0xA1:			/* PREPARE TEXT */
    case 0xA2:			/* OPEN BINARY */
    case 0xA3:			/* PREPARE BINARY */
      bytep = (void *)namebuf;
      byte_count = 11;
      state = st_filename;
      return true;

    case 0xA4:			/* INPUT */
      do_input(ix);
      break;

    case 0xA5:			/* READ BLOCK */
    case 0xA6:			/* PRINT */
      bytep = lenbuf;
      byte_count = 2;
      state = st_len;
      return true;

    case 0xA7:			/* CLOSE */
      send_reply(do_close(ix));
      break;

    case 0xA8:			/* CLOSEALL */
    case 0xA9:
      do_closeall();
      break;

    default:
      /* Unknown command */
      send_reply(128+11);
      break;
    }
    break;

  case st_filename:
    do_open(ix, namebuf);
    break;

  case st_len:
    switch (cmd[0]) {
    case 0xA5:			/* READ BLOCK */
      do_read_block(ix, len);
      break;
    case 0xA6:			/* PRINT */
      if (data)
	free(data);
      data = malloc(len);
      if (!data)
	send_reply(128+42);	/* "Skivan ej klar" */
      bytep = data;
      byte_count = len;
      state = st_data;
      return true;
    }
    break;

  case st_data:
    do_print(ix, len, data);
    free(data);
    data = NULL;
    break;
  }

  /* unless otherwise specified, back to command mode */
  bytep = cmd;
  byte_count = 4;
  state = st_op;
  return false;
}
