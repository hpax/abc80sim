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

    fprintf(tracef, "PR:  %-7s : ", pfx);

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

static unsigned int pr_send(struct abcprint *me, const void *buf,
			    size_t len, const char *what)
{
    const uint8_t *p = buf;

    if (tracing(TRACE_PR))
	trace_data(buf, len, what);

    while (len) {
	ssize_t sent = me->sd.func(me->sd.pvt, p, len);
	if (sent < 0)
	    sent = 0;
	len -= sent;
	p += sent;
    }

    return 0;			/* For convenience: no more data */
}

static unsigned int send_data(struct abcprint *me, const void *buf, size_t len)
{
    return pr_send(me, buf, len, "return");
}

static unsigned int send_reply(struct abcprint *me, int status)
{
    unsigned char reply[4];

    reply[0] = 0xff;
    reply[1] = me->cmd[0];
    reply[2] = me->cmd[1];
    reply[3] = status;

    return pr_send(me, reply, 4, "reply");
}

/* Returns error code */
static int do_blksize(struct abcprint *me, unsigned int arg)
{
    if (arg < 1 || arg > 65535)
	return 128 + 11;

    me->blksize = arg;
    return 0;
}

static unsigned int fop_blksize(struct abcprint *me)
{
    return send_reply(me, do_blksize(me, me->arg));
}

/* Returns the error code if applicable */
static int do_close(struct abcprint *me)
{
    if (file_open(me->ff)) {
        close_file(&me->ff->hf);
        return 0;
    } else {
        return 128 + 45;        /* "Fel logiskt filnummer" */
    }
}

static unsigned int fop_close(struct abcprint *me)
{
    return send_reply(me, do_close(me));
}

/* Close all files without sending a reply */
static int do_close_all(struct abcprint *me)
{
    size_t ix;

    for (ix = 0; ix <= 65535; ix++) {
	struct fileop_file *ff = getfile(me, ix);
	if (file_open(ff))
	    close_file(&ff->hf);
    }

    return 0;
}

static unsigned int do_init(struct abcprint *me, unsigned int blksz)
{
    do_blksize(me, blksz);
    return do_close_all(me);
}

static unsigned int fop_init(struct abcprint *me)
{
    return do_init(me, 253);
}

static unsigned int fop_initsz(struct abcprint *me)
{
    return do_init(me, me->arg);
}

static unsigned int fop_closeall(struct abcprint *me)
{
    return send_reply(me, do_close_all(me));
}

static unsigned int fop_open(struct abcprint *me)
{
    int err;
    char path_buf[64];
    int openflags;
    enum host_file_mode mode;
    struct fileop_file *ff;
    struct host_file *hf;
    uint8_t cmd0 = me->cmd[0];
    char *name = me->argbuf.c;

    if (!fileop_path) {
        return send_reply(me, 128 + 42);   /* Skivan ej klar */
    }

    do_close(me);

    if (name[0] == ' ') {
        /* Empty filename (readdir) */
	path_buf[0] = '\0';
        mode = ((cmd0 & 3) == 0) ? HF_DIRECTORY : HF_FAIL;
        openflags = 0;
    } else {
        /* Actual filename */
	unmangle_filename(path_buf, name);
        mode = HF_BINARY;
        mode |= (cmd0 & 2) ? 0 : HF_RETRY;
        openflags = (cmd0 & 2) ? (O_RDWR | O_TRUNC | O_CREAT) : O_RDWR;
    }

    hf = open_host_file(mode, fileop_path, path_buf, openflags);
    ff = getfile_alloc(me, me->ix);
    ff->hf = hf;
    ff->binary = cmd0 & 1;

    if (hf) {
	err = 0;
    } else {
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
    }

    return send_reply(me, err);
}

static unsigned int do_read_block(struct abcprint *me, unsigned int len)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;
    int dlen;

    if (!file_open(ff)) {
        return send_reply(me, 128 + 45);
    }
    hf = ff->hf;

    if (file_mode(ff) == HF_DIRECTORY) {
        return send_reply(me, 128 + 37);   /* Felaktigt recordformat */
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
        return send_reply(me, err);
    }

    send_reply(me, 0);

    me->data[0] = len;
    me->data[1] = len >> 8;
    return send_data(me, me->data, len + 2);
}

static unsigned int fop_get(struct abcprint *me)
{
    return do_read_block(me, me->arg);
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

static unsigned int fop_seek(struct abcprint *me)
{
    return send_reply(me, seeker(me, me->arg));
}

static unsigned int fop_pread(struct abcprint *me)
{
    int err;

    err = seeker(me, me->blksize * me->arg);
    if (err)
        return send_reply(me, err);
    else
        return do_read_block(me, me->blksize);
}

static unsigned int fop_input(struct abcprint *me)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;
    char data1[255 + 2];        /* Max number of bytes to return + 2 */
    char *p, *q, c;
    int dlen;
    struct dirent *de;
    struct stat st;

    if (!file_open(ff))
	return send_reply(me, 128 + 45);

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
    if (err)
	return 0;

    data1[0] = dlen;
    data1[1] = dlen >> 8;
    return send_data(me, data1, dlen + 2);
}

static unsigned int do_write(struct abcprint *me, bool eolcvt)
{
    size_t len = me->datalen;
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
	    size_t i;
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
    return send_reply(me, err);
}

static unsigned int arg_len(struct abcprint *me)
{
    /* Argument received is data length */
    return me->arg;
}
static unsigned int arg_blkno(struct abcprint *me)
{
    /*
     * Argument received is block number, blksize
     * data bytes follow
     */
    return me->blksize;
}

static unsigned int fop_print(struct abcprint *me)
{
    return do_write(me, true);
}
static unsigned int fop_put(struct abcprint *me)
{
    return do_write(me, false);
}
static unsigned int fop_pwrite(struct abcprint *me)
{
    int err;

    err = seeker(me, me->blksize * me->arg);
    if (err)
        return send_reply(me, err);
    else
        return do_write(me, false);
}

static unsigned int fop_rename(struct abcprint *me)
{
    const char *files = me->argbuf.c;
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

    return send_reply(me, err);
}

static unsigned int fop_delete(struct abcprint *me)
{
    const char *file = me->argbuf.c;
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

    return send_reply(me, err);
}

/* Invalid file operation */
static unsigned int fop_invalid(struct abcprint *me)
{
    return send_reply(me, 128 + 11); /* Förstår ej */
}

/* Revert the state machine to its initial state (no command in progress) */
static void fileop_goto_init_state(struct abcprint *me)
{
    memset(&me->cmd, 0, sizeof me->cmd);
    memset(&me->argbuf, 0, sizeof me->argbuf);
    me->arg = 0;
    me->fop = NULL;
    me->bytep = me->cmd;
    me->byte_count = 4;
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

typedef unsigned int (*fop_func)(struct abcprint *);

struct fop {
    unsigned int byte_count;	/* Argument bytes needed */
    const char *name;		/* Command name for tracing */
    fop_func runs[2];		/* Command phases */
};

/* If byte_count < 0, then the value is to be interpreted as a data length */

/* Command info starting at 0xA0... */
#define FIRST_CMD 0xA0
static const struct fop fops[] = {
    { 11, "OPEN_A", { fop_open, NULL } },  /* A0: OPEN ASCII */
    { 11, "OPEN_B", { fop_open, NULL } },  /* A1: OPEN BINARY */
    { 11, "PREP_A", { fop_open, NULL } },  /* A2: PREPARE ASCII */
    { 11, "PREP_B", { fop_open, NULL } },  /* A3: PREPARE BINARY */
    {  0, "INPUT",  { fop_input, NULL } },   /* A4: INPUT */
    {  2, "GET",    { fop_get, NULL } },     /* A5: READ BLOCK (GET) */
    {  2, "PRINT",  { arg_len, fop_print } },   /* A6: PRINT */
    {  0, "CLOSE",  { fop_close, NULL } },  /* A7: CLOSE */
    {  0, "CLOSALL", { fop_closeall, NULL } }, /* A8: CLOSE ALL */
    {  0, "INIT",   { fop_init, NULL } },    /* A9: close all and reset state */
    { 22, "RENAME", { fop_rename, NULL } },  /* AA: RENAME */
    { 11, "DELETE", { fop_delete, NULL } },  /* AB: DELETE (KILL) */
    {  2, "PREAD",  { fop_pread, NULL } },   /* AC: PREAD */
    {  2, "PWRITE", { arg_blkno, fop_pwrite } },  /* AD: PWRITE */
    {  2, "BLKSIZE", { fop_blksize, NULL } }, /* AE: SET BLOCK SIZE */
    {  2, "INITSZ", { fop_initsz, NULL } },  /* AF: INIT BLOCK SIZE */
    {  0, "SEEK0", { fop_seek, NULL } },   /* B0: SEEK0 (REWIND) */
    {  1, "SEEK1", { fop_seek, NULL } },   /* B1: SEEK1 */
    {  2, "SEEK2", { fop_seek, NULL } },   /* B2: SEEK2 */
    {  3, "SEEK3", { fop_seek, NULL } },   /* B3: SEEK3 */
    {  4, "SEEK4", { fop_seek, NULL } },   /* B4: SEEK4 */
    {  5, "SEEK5", { fop_seek, NULL } },   /* B5: SEEK5 */
    {  6, "SEEK6", { fop_seek, NULL } },   /* B6: SEEK6 */
    {  7, "SEEK7", { fop_seek, NULL } },   /* B7: SEEK7 */
    {  8, "SEEK8", { fop_seek, NULL } },   /* B8: SEEK8 */
    {  2, "PUT", { arg_len, fop_put } },     /* B9: PUT */
    {  0, "invalid", { fop_invalid, NULL } }  /* invalid command opcode */
};

bool file_op(struct abcprint *me, unsigned char c)
{
    *me->bytep++ = c;
    if (--me->byte_count)
        return true;            /* More to do... */

    /* Otherwise, we have a full deck of *something* */
    me->ix    = (me->cmd[3] << 8) + me->cmd[2];
    me->ff    = getfile(me, me->ix);
    me->arg   = get_qword(&me->argbuf);

    if (!me->fop) {
	size_t cmdix = me->cmd[0] - FIRST_CMD;

	if (cmdix >= ARRAY_SIZE(fops))
	    cmdix = ARRAY_SIZE(fops) - 1;
	me->fop = &fops[cmdix];
	me->byte_count = me->fop->byte_count;
	me->datalen = 0;
	me->bytep = me->bufp = me->argbuf.b;
	me->fseq = 0;

        if (tracing(TRACE_PR)) {
            fprintf(tracef, "PR:  %-7s : %02X %02x %04x",
		    me->fop->name, me->cmd[0], me->cmd[1], me->ix);
	    if (me->byte_count)
		fprintf(tracef, " <need %u bytes>", me->byte_count);
	    fputc('\n', tracef);
	}
    } else {
	if (tracing(TRACE_PR))
	    trace_data(me->bufp, me->bytep - me->bufp, me->fop->name);
    }

    if (me->byte_count)
	return true;

    me->byte_count = me->fop->runs[me->fseq](me);
    if (me->byte_count) {
	me->datalen = me->byte_count;
	me->bytep = me->bufp = me->data;
	me->fseq++;
	return true;
    } else {
	fileop_goto_init_state(me);
	return false;
    }
}
