#include "abcprintd.h"
#include "hostfile.h"

const char *fileop_path = "abcdir";

#define BUF_SIZE 512

static enum {
  st_op,
  st_data,
  st_open,
  st_read,
  st_print,
  st_seek,
  st_rename,
  st_delete
} state = st_op;
static unsigned int byte_count = 4;
static unsigned char cmd[4];
static unsigned char *bytep = cmd;
static struct host_file *filemap[65536];
static unsigned char data[65536+2];

static void send_reply(int status)
{
  unsigned char reply[4];

  reply[0] = 0xff;
  reply[1] = cmd[0];
  reply[2] = cmd[1];
  reply[3] = status;

  abcprint_send(reply, 4);
}

/* Returns the status code, use send_reply(do_close(ix)) if reply desired */
static int do_close(uint16_t ix)
{
  if (filemap[ix]) {
    close_file(&filemap[ix]);
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
  char path_buf[64];
  int openflags;
  enum host_file_mode mode;
  struct host_file *hf;

  if (!fileop_path) {
    send_reply(128+42);		/* Skivan ej klar */
    return;
  }

  do_close(ix);

  unmangle_filename(path_buf, name);

  if (!path_buf[0]) {
    /* Empty filename (readdir) */

    mode = ((cmd[0] & 3) == 0) ? HF_DIRECTORY : HF_FAIL;
    openflags = 0;
  } else {
    /* Actual filename */

    mode  = (cmd[0] & 1) ? HF_BINARY : HF_TEXT;
    mode |= (cmd[0] & 2) ? 0 : HF_RETRY;
    openflags = (cmd[0] & 2) ? (O_RDWR|O_TRUNC|O_CREAT) : O_RDWR;
  }

  hf = open_host_file(mode, fileop_path, path_buf, openflags);
  filemap[ix] = hf;

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

  send_reply(hf ? 0 : err);
}

static void do_read_block(uint16_t ix, uint16_t len)
{
  struct host_file *hf = filemap[ix];
  int err;
  int dlen;

  if (!hf) {
    send_reply(128+45);
    return;
  }

  if (hf->f) {
    send_reply(128+37);		/* Felaktigt recordformat */
    return;
  }

  clearerr(hf->f);
  dlen = fread(data+2, 1, len, hf->f);
  if (dlen == 0) {
    if (ferror(hf->f)) {
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
    return;
  }

  send_reply(0);

  data[0] = len;
  data[1] = len >> 8;
  abcprint_send(data, len+2);
}

static void do_seek(uint16_t ix, uint64_t pos)
{
  struct host_file *hf = filemap[ix];
  int err;

  if (!hf) {
    err = 128+45;		/* Fel logiskt filnummer */
  } else if (!hf->f) {
    err = 128+37;		/* Felaktigt recordformat */
  } else if (fseek(hf->f, pos, SEEK_SET) == -1) {
    err = 128+38;		/* Recordnummer utanför filen */
  } else {
    err = 0;
  }

  send_reply(err);
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
  struct host_file *hf = filemap[ix];
  int err;
  char data1[255+2];		/* Max number of bytes to return + 2 */
  char *p, *q, c;
  int dlen;
  struct dirent *de;
  struct stat st;

  if (!hf) {
    send_reply(128+45);
    return;
  }

  if (hf->f) {
    clearerr(hf->f);
    if (!fgets((char *)data, sizeof data, hf->f)) {
      if (ferror(hf->f)) {
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
      for (p = (char *)data, q = data1+2 ; (c = *p) ; p++) {
	if (q == &data1[sizeof data1])
	  break;
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
  } else if (hf->d) {
      while ( (de = readdir(hf->d)) ) {
	if (de->d_name[0] != '.' &&
	    (dlen = mangle_for_readdir(data1+2, de->d_name))) {
	  if (!stat_file(fileop_path, de->d_name, &st) &&
	      S_ISREG(st.st_mode))
	    break;
	}
      }
      if (de) {
	unsigned long blocks = (st.st_size + 252)/253;
	unsigned long pad = 253*blocks - st.st_size;
	/* pad = unused bytes in the last block */
	dlen += sprintf(data1+2+dlen, ",%lu,%lu\r\n", blocks, pad);
	err = 0;
      } else {
	err = 128+34;
      }
  } else {
    err = 128+44;
  }

  send_reply(err);
  if (!err) {
    data1[0] = dlen;
    data1[1] = dlen >> 8;
    abcprint_send(data1, dlen+2);
  }
}

static void do_print(uint16_t ix, uint16_t len)
{
  struct host_file *hf = filemap[ix];
  int err;

  if (!hf) {
    err = 128+45;
  } else if (!hf->f) {
    err = 128+39;		/* Directories are readonly */
  } else if (fwrite(data, 1, len, hf->f) != len || fflush(hf->f)) {
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
  } else {
    err = 0;
  }
  send_reply(err);
}

static void do_rename(const char *files)
{
  char old_name[64], new_name[64];
  int err;

  unmangle_filename(old_name, files);
  unmangle_filename(new_name, files+11);

  if (!rename(old_name, new_name)) {
    err = 0;
  } else {
    switch (errno) {
    case EACCES:
      err = 128+39;		/* Filen skrivskyddad */
      break;
    case EROFS:
      err = 128+43;		/* Skivan skrivskyddad */
      break;
    case ENOENT:
      err = 128+21;		/* Hittar ej filen */
      break;
    case ENOSPC:
      err = 128+41;		/* Skivan full */
      break;
    case EISDIR:
      err = 128+40;	        /* Filen raderingsskyddad */
      break;
    case EIO:
      err = 128+36;	        /* Checksummafel vid skrivning */
      break;
    default:
      err = 128+48;		/* Fel i biblioteket */
      break;
    }
  }

  send_reply(err);
}

static void do_delete(const char *file)
{
  char path_buf[64];
  int err;

  unmangle_filename(path_buf, file);

  if (!remove(path_buf)) {
    err = 0;
  } else {
    switch (errno) {
    case EROFS:
      err = 128+43;		/* Skivan skrivskyddad */
      break;
    case ENOENT:
      err = 128+21;		/* Hittar ej filen */
      break;
    case ENOSPC:
      err = 128+41;		/* Skivan full */
      break;
    case EISDIR:
    case EPERM:
    case EACCES:
      err = 128+40;	        /* Filen raderingsskyddad */
      break;
    case EIO:
      err = 128+36;	        /* Checksummafel vid skrivning */
      break;
    default:
      err = 128+48;		/* Fel i biblioteket */
      break;
    }
  }

  send_reply(err);
}

typedef union argbuf {
  uint8_t b[32];
  char c[32];
  uint64_t q;
} argbuf;

static inline uint64_t get_qword(const argbuf *v)
{
#ifdef WORDS_LITTLEENDIAN
  return v->q;
#else
  return v.b[0] +
    ((uint64_t)v->b[1] << 8) +
    ((uint64_t)v->b[2] << 16) +
    ((uint64_t)v->b[3] << 24) +
    ((uint64_t)v->b[4] << 32) +
    ((uint64_t)v->b[5] << 40) +
    ((uint64_t)v->b[6] << 48) +
    ((uint64_t)v->b[7] << 56);
#endif
}

bool file_op(unsigned char c)
{
  static argbuf argbuf;
  static unsigned int datalen = 0;
  uint16_t ix;
  uint64_t arg;

  *bytep++ = c;
  if (--byte_count)
    return true;		/* More to do... */

  /* Otherwise, we have a full deck of *something* */
  ix  = (cmd[3] << 8) + cmd[2];
  arg = get_qword(&argbuf);
  memset(&argbuf, 0, sizeof argbuf);
  bytep = argbuf.b;

  switch (state) {
  case st_op:
    switch (cmd[0]) {
    case 0xA0:			/* OPEN TEXT */
    case 0xA1:			/* PREPARE TEXT */
    case 0xA2:			/* OPEN BINARY */
    case 0xA3:			/* PREPARE BINARY */
      byte_count = 11;
      state = st_open;
      break;

    case 0xA4:			/* INPUT */
      do_input(ix);
      break;

    case 0xA5:			/* READ BLOCK */
      byte_count = 2;
      state = st_read;
      break;

    case 0xA6:			/* PRINT */
      byte_count = 2;
      state = st_print;
      break;

    case 0xA7:			/* CLOSE */
      send_reply(do_close(ix));
      break;

    case 0xA8:			/* CLOSEALL */
    case 0xA9:
      do_closeall();
      break;

    case 0xAA:			/* RENAME */
      byte_count = 22;
      state = st_rename;
      break;

    case 0xAB:			/* DELETE */
      byte_count = 11;
      state = st_delete;
      break;

    case 0xB0:			/* SEEK1 */
    case 0xB1:			/* SEEK2 */
    case 0xB2:			/* SEEK3 */
    case 0xB3:			/* SEEK4 */
    case 0xB4:			/* SEEK5 */
    case 0xB5:			/* SEEK6 */
    case 0xB6:			/* SEEK7 */
    case 0xB7:			/* SEEK8 */
      byte_count = cmd[0] - 0xAF;
      state = st_seek;
      break;

    default:
      /* Unknown command */
      send_reply(128+11);
      break;
    }
    break;

  case st_open:
    do_open(ix, argbuf.c);
    break;

  case st_read:
    do_read_block(ix, arg);
    break;

  case st_print:
    bytep = data;
    byte_count = arg;
    state = st_data;
    break;

  case st_seek:
    do_seek(ix, arg);
    break;

  case st_data:
    do_print(ix, datalen);
    break;

  case st_rename:
    do_rename(argbuf.c);		/* Unimplemented command */
    break;

  case st_delete:
    do_delete(argbuf.c);
    break;
  }

  datalen = byte_count;

  if (byte_count) {
    return true;
  } else {
    /* back to command mode */
    bytep = cmd;
    byte_count = 4;
    state = st_op;
    return false;
  }
}
