#include "abcprintd.h"
#include "hostfile.h"
#include "abcfile.h"
#include "trace.h"
#include "print.h"

const char *fileop_path = "abcdir";

#define BUF_SIZE 512

static inline struct fileop_file *getfile(struct abcprint *me, uint16_t ix)
{
    return &me->filemap[ix];
}

/* In the future this may be used to allocate filemap storage */
#define getfile_alloc(me,ix) getfile(me,ix)

static inline bool file_open(const struct fileop_file *ff)
{
    return ff && ff->hf;
}

static inline bool file_binary(const struct fileop_file *ff)
{
    return ff->binary;
}

static inline enum host_file_mode file_mode(const struct fileop_file *ff)
{
    return ff->hf->mode;
}

static void trace_data(const void *data, size_t len, const char *pfx)
{
    size_t i;
    const uint8_t *dp = data;

    if (!tracing(TRACE_PR))
        return;

    fprintf(tracef, "PR:  %-5s: ", pfx);

    for (i = 0; i < 16; i++) {
        if (i >= len)
            fprintf(tracef, "  ");
        else
            fprintf(tracef, "%02x", dp[i]);

        putc(i == 8 ? '-' : ' ', tracef);
    }

    fprintf(tracef, "%c  [", (len > 16) ? '+' : ' ');

    for (i = 0; i < 16; i++) {
        char c;

        c = (i >= len) ? ' ' : dp[i];
        if (c < 32 || c > 126)
            c = '.';

        putc(c, tracef);
    }
    putc(']', tracef);
    if (len > 16)
        fprintf(tracef, "+ (%lu bytes)", (unsigned long)len);
    putc('\n', tracef);
}

static void pr_send(struct abcprint *me, const void *buf, size_t len)
{
    const uint8_t *p = buf;

    trace_data(buf, len, "SEND");
    while (len) {
	size_t sent = me->sd.func(me->sd.pvt, p, len);
	if (sent == (size_t)-1)
	    sent = 0;
	len -= sent;
	p += sent;
    }
}

static void send_reply(struct abcprint *me, int status)
{
    unsigned char reply[4];

    reply[0] = 0xff;
    reply[1] = me->cmd[0];
    reply[2] = me->cmd[1];
    reply[3] = status;

    pr_send(me, reply, 4);
}

/* Returns the status code, use send_reply(me, do_close(ix)) if reply desired */
static int do_close(struct abcprint *me)
{
    if (file_open(me->ff)) {
        close_file(&me->ff->hf);
        return 0;
    } else {
        return 128 + 45;        /* "Fel logiskt filnummer" */
    }
}

static void do_closeall(struct abcprint *me, bool reply)
{
    size_t ix;

    for (ix = 0; ix <= 65535; ix++) {
	struct fileop_file *ff = getfile(me, ix);
	if (file_open(ff))
	    close_file(&ff->hf);
    }

    if (reply)
        send_reply(me, 0);
}

static void do_open(struct abcprint *me, char *name)
{
    int err;
    char path_buf[64];
    int openflags;
    enum host_file_mode mode;
    struct fileop_file *ff;
    struct host_file *hf;
    uint8_t cmd0 = me->cmd[0];

    if (!fileop_path) {
        send_reply(me, 128 + 42);   /* Skivan ej klar */
        return;
    }

    do_close(me);

    unmangle_filename(path_buf, name);

    if (!path_buf[0]) {
        /* Empty filename (readdir) */

        mode = ((cmd0 & 3) == 0) ? HF_DIRECTORY : HF_FAIL;
        openflags = 0;
    } else {
        /* Actual filename */

        mode = HF_BINARY;
        mode |= (cmd0 & 2) ? 0 : HF_RETRY;
        openflags = (cmd0 & 2) ? (O_RDWR | O_TRUNC | O_CREAT) : O_RDWR;
    }

    hf = open_host_file(mode, fileop_path, path_buf, openflags);
    ff = getfile_alloc(me, me->ix);
    ff->hf = hf;
    ff->binary = cmd0 & 1;

    switch (errno) {
#if 0                           /* Enable this? */
    case EACCES:
        err = 128 + 39;
        break;
    case EROFS:
        err = 128 + 43;
        break;
    case EIO:
    case ENOTDIR:
        err = 128 + 48;
        break;
#endif
    default:
	err = 128;		/* Hittar ej filen (default error) */
        break;
    }

    send_reply(me, hf ? 0 : err);
}

static void do_read_block(struct abcprint *me, uint16_t len)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;
    int dlen;

    if (!file_open(ff)) {
        send_reply(me, 128 + 45);
        return;
    }
    hf = ff->hf;

    if (file_mode(ff) == HF_DIRECTORY) {
        send_reply(me, 128 + 37);   /* Felaktigt recordformat */
        return;
    }

    clearerr(hf->f);
    dlen = fread(me->data + 2, 1, len, hf->f);
    if (dlen == 0) {
        if (ferror(hf->f)) {
            switch (errno) {
            case EBADF:
                err = 128 + 44; /* Logisk fil ej öppen */
                break;
            case EIO:
                err = 128 + 35; /* Checksummafel vid läsning */
                break;
            default:
                err = 128 + 48; /* Fel i biblioteket */
                break;
            }
        } else {
            /* EOF - definitely not the default return! */
	    err = 128 + 34;	/* Slut på filen (är det rätt?) */
        }
        send_reply(me, err);
        return;
    }

    send_reply(me, 0);

    me->data[0] = len;
    me->data[1] = len >> 8;
    pr_send(me, me->data, len + 2);
}

/* Common routine for all commands which need seek */
static int seeker(struct abcprint *me, uint64_t pos)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;

    if (!file_open(ff))
        return 128 + 45;         /* Fel logiskt filnummer */

    hf = ff->hf;
    if (file_mode(ff) == HF_DIRECTORY)
        return 128 + 37;         /* Felaktigt recordformat */

    if (fseeko(hf->f, pos, SEEK_SET) == -1)
        return 128 + 38;         /* Recordnummer utanför filen */

    return 0;
}

static void do_seek(struct abcprint *me, uint64_t pos)
{
    send_reply(me, seeker(me, pos));
}

static void do_pread(struct abcprint *me, uint16_t blk)
{
    int err;

    err = seeker(me, me->blksize * (long)blk);
    if (err)
        send_reply(me, err);
    else
        do_read_block(me, me->blksize);
}

static void do_input(struct abcprint *me)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;
    char data1[255 + 2];        /* Max number of bytes to return + 2 */
    char *p, *q, c;
    int dlen;
    struct dirent *de;
    struct stat st;

    if (!file_open(ff)) {
        send_reply(me, 128 + 45);
        return;
    }
    hf = ff->hf;

    if (file_mode(ff) != HF_DIRECTORY) {
        clearerr(hf->f);
        if (!fgets((char *)me->data, sizeof me->data, hf->f)) {
            if (ferror(hf->f)) {
                switch (errno) {
                case EBADF:
                    err = 128 + 44;     /* Logisk fil ej öppen */
                    break;
                case EIO:
                    err = 128 + 35;     /* Checksummafel vid läsning */
                    break;
                default:
                    err = 128 + 48;     /* Fel i biblioteket */
                    break;
                }
            } else {
                /* EOF */
                err = 128; /* Slut på filen (default error) */
            }
        } else {
            /* Strip CR and change LF -> CR LF */
            if (!file_binary(ff)) {
                for (p = (char *)me->data, q = data1 + 2; (c = *p); p++) {
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
                dlen = q - (data1 + 2);
            } else {
                dlen = strnlen(data1 + 2, sizeof data1 - 2);
            }
            err = 0;
        }
    } else if (hf->d) {
        while ((de = readdir(hf->d))) {
            if (de->d_name[0] != '.' &&
                (dlen = mangle_for_readdir(data1 + 2, de->d_name))) {
                if (!stat_file(fileop_path, de->d_name, &st) &&
                    S_ISREG(st.st_mode))
                    break;
            }
        }
        if (de) {
            unsigned long blocks, pad;
            blocks = (st.st_size + me->blksize - 1) / me->blksize;
            pad = me->blksize * blocks - st.st_size;
            /* pad = unused bytes in the last block */
            dlen += sprintf(data1 + 2 + dlen, ",%lu,%lu\r\n", blocks, pad);
            err = 0;
        } else {
            err = 128;		/* End of file (default error) */
        }
    } else {
        err = 128 + 44;
    }

    send_reply(me, err);
    if (!err) {
        data1[0] = dlen;
        data1[1] = dlen >> 8;
        pr_send(me, data1, dlen + 2);
    }
}

static void do_print(struct abcprint *me, uint16_t len, bool eolcvt)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;

#ifdef __WIN32__
    /* ABC sends CR LF as line endings, which matches Windows */
    eolcvt = false;
#endif

    if (!file_open(ff)) {
        err = 128 + 45;
	goto fail;
    }
    hf = ff->hf;

    if (file_mode(ff) == HF_DIRECTORY) {
        err = 128 + 39;         /* Directories are readonly */
	goto fail;
    }

    hf = ff->hf;

    clearerr(hf->f);
    if (len) {
	if (eolcvt && !file_binary(ff)) {
	    int i;
	    for (i = 0; i < len - 1; i++) {
		char c = me->data[i];
		if (c == '\r')
		    continue;       /* CR LF -> LF */
		else
		    putc(c, hf->f);
	    }
	    /* Last char is always verbatim */
	    fputc(me->data[len - 1], hf->f);
	} else {
	    fwrite(me->data, 1, len, hf->f);
	}
	fflush(hf->f);
    }

    if (!ferror(hf->f)) {
	err = 0;
    } else {
	switch (errno) {
	case EACCES:
	    err = 128 + 39; /* Filen skrivskyddad */
	    break;
	case ENOSPC:
	case EFBIG:
	    err = 128 + 41; /* Skivan full */
	    break;
	case EROFS:
	    err = 128 + 43; /* Skivan skrivskyddad */
	    break;
	case EBADF:
	    err = 128 + 44; /* Logisk fil ej öppen */
	    break;
	case EIO:
	    err = 128 + 36; /* Checksummafel vid skrivning */
	    break;
	default:
	    err = 128 + 48; /* Fel i biblioteket */
	    break;
	}
    }

fail:
    send_reply(me, err);
}

static void do_pwrite(struct abcprint *me, uint16_t blk)
{
    int err;

    err = seeker(me, me->blksize * (long)blk);
    if (err)
        send_reply(me, err);
    else
        do_print(me, me->blksize, false);
}

static void do_rename(struct abcprint *me, const char *files)
{
    char old_name[64], new_name[64];
    int err;

    unmangle_filename(old_name, files);
    unmangle_filename(new_name, files + 11);

    if (!rename(old_name, new_name)) {
        err = 0;
    } else {
        switch (errno) {
        case EACCES:
            err = 128 + 39;     /* Filen skrivskyddad */
            break;
        case EROFS:
            err = 128 + 43;     /* Skivan skrivskyddad */
            break;
        case ENOENT:
            err = 128 + 21;     /* Hittar ej filen */
            break;
        case ENOSPC:
            err = 128 + 41;     /* Skivan full */
            break;
        case EISDIR:
            err = 128 + 40;     /* Filen raderingsskyddad */
            break;
        case EIO:
            err = 128 + 36;     /* Checksummafel vid skrivning */
            break;
        default:
            err = 128 + 48;     /* Fel i biblioteket */
            break;
        }
    }

    send_reply(me, err);
}

static void do_delete(struct abcprint *me, const char *file)
{
    char path_buf[64];
    int err;

    unmangle_filename(path_buf, file);

    if (!remove(path_buf)) {
        err = 0;
    } else {
        switch (errno) {
        case EROFS:
            err = 128 + 43;     /* Skivan skrivskyddad */
            break;
        case ENOENT:
            err = 128 + 21;     /* Hittar ej filen */
            break;
        case ENOSPC:
            err = 128 + 41;     /* Skivan full */
            break;
        case EISDIR:
        case EPERM:
        case EACCES:
            err = 128 + 40;     /* Filen raderingsskyddad */
            break;
        case EIO:
            err = 128 + 36;     /* Checksummafel vid skrivning */
            break;
        default:
            err = 128 + 48;     /* Fel i biblioteket */
            break;
        }
    }

    send_reply(me, err);
}

/* Revert the state machine to its initial state (no command in progress) */
static void fileop_goto_init_state(struct abcprint *me)
{
    memset(&me->cmd, 0, sizeof me->cmd);
    memset(&me->argbuf, 0, sizeof me->argbuf);
    me->bytep = me->cmd;
    me->byte_count = 4;
    me->fstate = st_op;
}

/* Initialize or terminate the fileop session */
void fileop_reset(struct abcprint *me)
{
    me->blksize = 253;
    fileop_goto_init_state(me);
}

static inline uint64_t get_qword(const argbuf *v)
{
#ifdef WORDS_LITTLEENDIAN
    return v->q;
#else
    return v->b[0] +
        ((uint64_t) v->b[1] << 8) +
        ((uint64_t) v->b[2] << 16) +
        ((uint64_t) v->b[3] << 24) +
        ((uint64_t) v->b[4] << 32) +
        ((uint64_t) v->b[5] << 40) +
        ((uint64_t) v->b[6] << 48) +
        ((uint64_t) v->b[7] << 56);
#endif
}

bool file_op(struct abcprint *me, unsigned char c)
{
    uint64_t arg;

    *me->bytep++ = c;
    if (--me->byte_count)
        return true;            /* More to do... */

    /* Otherwise, we have a full deck of *something* */
    me->ix = (me->cmd[3] << 8) + me->cmd[2];
    me->ff = getfile(me, me->ix);
    arg = get_qword(&me->argbuf);
    me->bytep = me->argbuf.b;

    switch (me->fstate) {
    case st_op:
        switch (me->cmd[0]) {
        case 0xA0:             /* OPEN TEXT */
        case 0xA1:             /* OPEN BINARY */
        case 0xA2:             /* PREPARE TEXT */
        case 0xA3:             /* PREPARE BINARY */
            me->byte_count = 11;
            me->fstate = st_open;
            break;

        case 0xA4:             /* INPUT */
            do_input(me);
            break;

        case 0xA5:             /* READ BLOCK */
            me->byte_count = 2;
            me->fstate = st_read;
            break;

        case 0xA6:             /* PRINT */
        case 0xB9:             /* PUT */
            me->byte_count = 2;
            me->fstate = st_print;
            break;

        case 0xA7:             /* CLOSE */
            send_reply(me, do_close(me));
            break;

        case 0xA8:             /* CLOSEALL */
            do_closeall(me, true);
            break;

        case 0xA9:             /* INIT */
            me->blksize = 253;
            do_closeall(me, false);
            break;

        case 0xAE:             /* SET BLOCK SIZE */
        case 0xAF:             /* INITSZ */
            me->byte_count = 2;
            me->fstate = st_blksize;
            break;

        case 0xAA:             /* RENAME */
            me->byte_count = 22;
            me->fstate = st_rename;
            break;

        case 0xAB:             /* DELETE */
            me->byte_count = 11;
            me->fstate = st_delete;
            break;

        case 0xAC:             /* PREAD */
            me->byte_count = 2;
            me->fstate = st_pread;
            break;

        case 0xAD:             /* PWRITE */
            me->byte_count = 2;
            me->fstate = st_pwrite;
            break;

        case 0xB0:             /* SEEK0 == REWIND */
            do_seek(me, 0);
            break;

        case 0xB1:             /* SEEK1 */
        case 0xB2:             /* SEEK2 */
        case 0xB3:             /* SEEK3 */
        case 0xB4:             /* SEEK4 */
        case 0xB5:             /* SEEK5 */
        case 0xB6:             /* SEEK6 */
        case 0xB7:             /* SEEK7 */
        case 0xB8:             /* SEEK8 */
            me->byte_count = me->cmd[0] - 0xb0;
            me->fstate = st_seek;
            break;

        default:
            /* Unknown command */
            send_reply(me, 128 + 11);
            break;
        }
        if (tracing(TRACE_PR)) {
            static const char *const cmdnames[0x20] = {
                "OPEN A", "OPEN B", "PREP A", "PREP B",
                "INPUT", "GET", "PRINT", "CLOSE",
                "CALL", "CALLNR", "RENAME", "DELETE",
                "PREAD", "PWRITE", "BLKSIZ", "INITSZ",
                "REWIND", "SEEK1", "SEEK2", "SEEK3",
                "SEEK4", "SEEK5", "SEEK6", "SEEK7",
                "SEEK8", "PUT", NULL, NULL,
                NULL, NULL, NULL, NULL
            };
            int cnum = me->cmd[0] - 0xa0;
            const char *cmdname = (cnum >= 0x20) ? NULL : cmdnames[cnum];

            fprintf(tracef, "PR:  CMD  : %-6s %02x %02x %04x <need %u bytes>\n",
                    cmdname ? cmdname : "???", me->cmd[0], me->cmd[1],
		    me->ix, me->byte_count);
        }
        break;

    case st_open:
        trace_data(me->argbuf.b, 11, "OPEN");
        do_open(me, me->argbuf.c);
        break;

    case st_read:
        trace_data(me->argbuf.b, 2, "READ");
        do_read_block(me, arg);
        break;

    case st_print:
        trace_data(me->argbuf.b, 2, "WRTE");
        me->bytep = me->data;
        me->byte_count = arg;
        me->fstate = st_print2;
        break;

    case st_print2:
        trace_data(me->data, me->datalen, "DATA");
        do_print(me, me->datalen, me->cmd[0] == 0xA6);
        break;

    case st_pwrite:
        trace_data(me->argbuf.b, 2, "PWRT");
        me->bytep = me->data;
        me->byte_count = 253;
        me->fstate = st_pwrite2;
        break;

    case st_pwrite2:
        trace_data(me->data, me->datalen, "DATA");
        do_pwrite(me, arg);
        break;

    case st_pread:
        trace_data(me->argbuf.b, 2, "PRED");
        do_pread(me, arg);
        break;

    case st_seek:
        trace_data(me->argbuf.b, me->cmd[0] - 0xb0, "SEEK");
        do_seek(me, arg);
        break;

    case st_rename:
        trace_data(me->argbuf.b, 11, "REN1");
        trace_data(me->argbuf.b + 11, 11, "REN2");
        do_rename(me, me->argbuf.c);
        break;

    case st_delete:
        trace_data(me->argbuf.b, 11, "DEL ");
        do_delete(me, me->argbuf.c);
        break;

    case st_blksize:
        trace_data(me->argbuf.b, 2, "SIZE");
        me->blksize = arg;
        if (me->cmd[0] == 0xAF)
            do_closeall(me, false);
        break;
    }

    me->datalen = me->byte_count;

    if (me->byte_count) {
        return true;
    } else {
	fileop_goto_init_state(me);
        return false;
    }
}
