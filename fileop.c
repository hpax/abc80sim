#include "abcprintd.h"
#include "hostfile.h"
#include "abcfile.h"
#include "trace.h"
#include "print.h"
#include "ilog2.h"

const char *fileop_path = "abcdir";

#define USE_FILEOP_PR	1
#define LEGACY_PRA_PRB	1
#define BUF_SIZE 512

/*
 * Find a volume structure by name. For right now a simple linear
 * search; might want to improve this in the future, but it is unlikely
 * to matter since the list is fairly short and the ABC will do a
 * linear search on its end.
 */
static pure_func struct volume *
get_volume(struct abcprint *me, const char *name)
{
    int i;

    for (i = 0; i < me->vols; i++) {
	if (!memcmp(me->volumes[i].name, name, 3))
	    return &me->volumes[i];
    }

    return NULL;
}

static void add_volume(struct abcprint *me, const char *name,
		       int mode, int prio, const char *path,
		       const struct chardev *dev)
{
    struct volume *vol = &me->volumes[me->vols++];

    memcpy(vol->name, name, 3);
    vol->mode = mode;
    vol->prio = prio;
    vol->path = path ? strdup(path) : NULL;
    vol->dev  = dev;
}

/* Clean up volume information */
static void cleanup_volumes(struct abcprint *me)
{
    for (size_t i = 0; i < (size_t)me->vols; i++) {
	if (me->volumes[i].path)
	    free((void *)me->volumes[i].path);
    }
    memset(me->volumes, 0, sizeof me->volumes);
}

static const struct chardev chardev_pr;

/* Scan for volumes */
static void init_volumes(struct abcprint *me)
{
    struct host_file *hf;
    struct volume *vol;

    cleanup_volumes(me);	/* In case this is being re-executed */

    if (!fileop_path || !*fileop_path)
	return;

    me->vols = 0;

    /* Default volumes */
    add_volume(me, "NET", 2, 1, fileop_path, NULL);
#if LEGACY_PRA_PRB
    /* Legacy volumes */
    add_volume(me, "PRA", 1, 1, fileop_path, NULL);
    add_volume(me, "PRB", 2, 1, fileop_path, NULL);
#endif
#if USE_FILEOP_PR
    add_volume(me, "PR ", 1, 1, NULL, &chardev_pr);
#endif

    hf = open_host_file(HF_DIRECTORY, NULL, fileop_path, 0);
    if (hf) {
	struct dirent *de;
	while ((de = read_dir(hf))) {
	    char volname[16];
	    struct stat st;
	    const int prio = 2;	/* Explicit volume */
	    const int mode = 2;	/* Binary */

	    if (!mangle_volname(volname, de->d_name))
		continue;

	    if (stat_file(fileop_path, de->d_name, &st) ||
		!S_ISDIR(st.st_mode))
		continue;	/* Not a directory */

	    /*
	     * Did this volume already exist? Let a low priority override
	     * a higher priority, and if the priority is the same, the
	     * mode.
	     */
	    vol = get_volume(me, volname);
	    if (!vol) {
		if (me->vols >= MAX_VOLS)
		    continue;	/* Already full */

		vol = &me->volumes[me->vols++];
	    }

	    if (prio > vol->prio) {
		memcpy(vol->name, volname, 4);
		if (vol->path)
		    free((void *)vol->path);
		vol->path = concat_path(fileop_path, de->d_name);
		vol->mode = mode;
		vol->dev  = NULL;
	    }
	}
    }
}

static struct fileop_file *getfile(struct abcprint *me, uint16_t ix)
{
    files_mask_t mask = me->open_mask;
    struct fileop_file *ff = &me->files[0];

    while (mask) {
	if ((mask & 1) && (ff->ix == ix))
	    return ff;
	ff++;
	mask >>= 1;
    }
    return NULL;		/* File not found */
}

static inline bool file_open(const struct fileop_file *ff)
{
    return ff && ff->hf;
}

static inline files_mask_t OPEN_MASK(unsigned int i)
{
    return ((files_mask_t)1) << i;
}

/* Allocate a new file structure */
static struct fileop_file *alloc_file(struct abcprint *me)
{
    unsigned int i = tzcount_files_mask(~me->open_mask);
    if (i >= MAX_FILES)
	return NULL;

    me->open_mask |= OPEN_MASK(i);
    return &me->files[i];
}

static inline bool file_binary(const struct fileop_file *ff)
{
    return !!(ff->opencmd & FF_BINARY);
}

static inline enum host_file_mode file_mode(const struct fileop_file *ff)
{
    return ff->hf->mode & HF_TYPE_MASK;
}

#define TRACE_LINE 16
static void trace_data(const void *data, int len, const char *pfx)
{
    int i;
    const uint8_t *dp = data;

    while (len > 0) {
	fprintf(tracef, "PR:  %-7s : ", pfx);

	for (i = 0; i < TRACE_LINE; i++) {
	    if (i >= len)
		fprintf(tracef, "  ");
	    else
		fprintf(tracef, "%02x", dp[i]);

	    putc(i == 8 ? '-' : ' ', tracef);
	}

	fprintf(tracef, "   [");

	for (i = 0; i < TRACE_LINE; i++) {
	    char c;

	    if (i >= len)
		break;

	    c = dp[i];
	    if (c < 32 || c > 126)
		c = '.';

	    putc(c, tracef);
	}
	fprintf(tracef, "]\n");
	len -= TRACE_LINE;
	dp += TRACE_LINE;
    }
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
    return pr_send(me, buf, len, "data");
}

static unsigned int send_reply(struct abcprint *me, int status)
{
    unsigned char reply[4];
    char err_txt[8];
    const char *what = NULL;

    reply[0] = 0xff;
    reply[1] = me->cmd[0];
    reply[2] = me->cmd[1];
    reply[3] = status;

    if (tracing(TRACE_PR)) {
	if (!status) {
	    what = "ok";	/* Operation completed */
	} else if (status == 0x80) {
	    what = "fail";	/* Default failure for this operation */
	} else if (status & 0x80) {
	    snprintf(err_txt, sizeof err_txt, "err %u", status & 0x7f);
	    what = err_txt;
	} else {
	    /* Successful return but with extra status, normally a bitmask */
	    snprintf(err_txt, sizeof err_txt, "ok %02x", status);
	}
    }

    return pr_send(me, reply, 4, what);
}

/* Returns error code */
static int do_blksize(struct abcprint *me, unsigned int arg)
{
    const unsigned int databuf_slack = 4;
    unsigned int datasize;

    if (arg < 1 || arg > 65535)
	return 128 + 11;

    datasize = (arg + databuf_slack + 255) & ~255;
    if (datasize != me->datasize) {
	me->data = realloc(me->data, datasize);
	me->datasize = datasize;
    }

    me->blksize = arg;
    return 0;
}

static unsigned int fop_blksize(struct abcprint *me)
{
    return send_reply(me, do_blksize(me, me->arg));
}

/* Returns the error code if applicable */
static int do_close(struct abcprint *me, struct fileop_file *ff)
{
    files_mask_t mask;
    int err;

    if (!ff)
        return 128 + 49;        /* "Fel fysiskt filnummer" */

    assert(ff->i < MAX_FILES);
    mask = OPEN_MASK(ff->i);
    assert(me->open_mask & mask);

    err = 0;
    if (ff->vol->dev) {
	chardev_close close_func = ff->vol->dev->close;
	if (close_func)
	    err = close_func(me, ff);

	/* FALL THROUGH: still do the same cleanup if applicable */
    }

    if (ff->hf)
	close_file(&ff->hf);

    if (ff->abc) {
	if (ff->abc->buf)
	    free((void *)ff->abc->buf);
	free(ff->abc);
	ff->abc = NULL;
    }

    ff->ix = ff->opencmd = 0;
    me->open_mask &= ~mask;
    return err;
}

static unsigned int fop_close(struct abcprint *me)
{
    return send_reply(me, do_close(me, me->ff));
}

/* Close all files without sending a reply */
static int do_close_all(struct abcprint *me)
{
    files_mask_t mask = me->open_mask;
    struct fileop_file *ff = &me->files[0];

    while (mask) {
	if (mask & 1)
	    do_close(me, ff);
	ff++;
	mask >>= 1;
    }

    assert(!me->open_mask);
    return 0;
}

static void do_init(struct abcprint *me, unsigned int blksz)
{
    do_blksize(me, blksz);
    do_close_all(me);
    init_volumes(me);
}

static unsigned int fop_initsz(struct abcprint *me)
{
    do_init(me, me->arg);
    me->pktmode = true;
    return send_reply(me, 0);
}

static unsigned int fop_init(struct abcprint *me)
{
    me->arg = 253;
    return fop_initsz(me);
}

static unsigned int fop_closeall(struct abcprint *me)
{
    return send_reply(me, do_close_all(me));
}

/*
 * Read one directory entry string into a buffer, return length,
 * including CR LF but not including NUL. Return 0 if EOF.
 */
/* Bytes needed in a directory string buffer (safe estimate) */
#define DIRSTR_BUF (12+3+2*3*sizeof(unsigned long)+21+2)
static unsigned int read_dir_entry(struct abcprint *me, char *buf)
{
    struct dirent *de;
    struct stat st;
    unsigned int dlen = 0;
    struct fileop_file *ff = me->ff;
    bool longfmt = !(ff->opencmd & FF_PREPARE);
    struct host_file *hf = ff->hf;

    if (!hf || !hf->d)
	return 0;

    while ((de = readdir(hf->d))) {
	unsigned int nlen;
	if (de->d_name[0] != '.' &&
	    (nlen = mangle_for_readdir(buf, de->d_name))) {
	    if (!stat_file(hf->filename, de->d_name, &st) &&
		S_ISREG(st.st_mode)) {
		dlen = nlen;
		break;
	    }
	}
   }

    if (dlen) {
	if (longfmt) {
	    unsigned long blocks, pad;
	    struct tm tm;
	    memset(&tm, 0, sizeof tm);

	    localtime_r(&st.st_mtime, &tm);

	    blocks = (st.st_size + me->blksize - 1) / me->blksize;
	    pad = me->blksize * blocks - st.st_size;

	    /* pad = unused bytes in the last block */
	    dlen += snprintf(buf + dlen, DIRSTR_BUF - 2 - dlen,
			     ",%lu,%lu,\"%04d-%02d-%02d %02d.%02d.%02d\"",
			     blocks, pad, tm.tm_year + 1900, tm.tm_mon + 1,
			     tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	buf[dlen++] = '\r';
	buf[dlen++] = '\n';
    }

    buf[dlen] = '\0';
    return dlen;
}

/*
 * Read directory information into an abcdata buffer which we
 * can then emit as a block file.
 */
struct line_list {
    struct line_list *next;
    size_t len;
    char line[1];
};
static int qsort_compare_ll(const void *l1, const void *l2)
{
    const struct line_list * const *ll1p = l1;
    const struct line_list *ll1 = *ll1p;
    const struct line_list * const *ll2p = l2;
    const struct line_list *ll2 = *ll2p;

    return strcmp_abc(ll1->line, ll2->line);
}

static unsigned int
read_dir_data(struct abcprint *me, struct fileop_file *ff)
{
    char dirname_buf[DIRSTR_BUF];
    struct abcdata *abc;
    size_t lines = 0;
    size_t bytes = 0;
    unsigned int dlen;
    struct line_list *ll = NULL, *lp, **lla, **lap;
    char *cp;
    unsigned int blk_bytes;

    if (!ff || !ff->hf || !ff->hf->d)
	return 128 + 48;	/* Fel i biblioteket */

    ff->abc = abc = calloc(1, sizeof *abc);
    if (!abc)
	return 128 + 3;		/* Minnet fullt */

    while ((dlen = read_dir_entry(me, dirname_buf))) {
	struct line_list *lp;
	dlen -= 1;			/* Drop final \n */
	lp = malloc(sizeof *lp + dlen);
	if (!lp)
	    continue;
	lp->next = ll;
	ll = lp;
	lp->len = dlen;
	memcpy(lp->line, dirname_buf, dlen);
	lp->line[dlen] = '\0';
	bytes += dlen;
	lines++;
    }

    lla = malloc((lines+1) * sizeof *lla);
    for (lap = lla, lp = ll; lp; lp = lp->next)
	*lap++ = lp;
    *lap = NULL;

    qsort(lla, lines, sizeof *lla, qsort_compare_ll);

    /* Now lla is an in-order list of directory entry strings */
    abc->blocks = (bytes+251)/252; /* Each block needs ETX or EOF */
    cp = calloc(abc->blocks, 253);
    abc->buf = cp;
    abc->data = cp;
    abc->len = abc->blocks * 253;

    blk_bytes = 252;
    lap = lla;
    while ((lp = *lap++)) {
	if (lp->len < blk_bytes) {
	    cp = mempcpy(cp, lp->line, lp->len);
	} else {
	    cp = mempcpy(cp, lp->line, blk_bytes);
	    *cp++ = 0x03;	/* ETX at end of block */
	    cp = mempcpy(cp, lp->line + blk_bytes, lp->len - blk_bytes);
	    blk_bytes += 252;
	}
	blk_bytes -= lp->len;
	free(lp);
    }

    free(lla);

    return 1;			/* It was a directory */
}

static unsigned int fop_open(struct abcprint *me)
{
    int err;
    int openflags;
    enum host_file_mode mode;
    struct fileop_file *ff = NULL;
    struct host_file *hf;
    uint8_t cmd0 = me->cmd[0];
    const struct volume *vol = get_volume(me, me->argbuf.c);
    char *name = me->argbuf.c + 3;

    if (!vol) {
	err = 128 + 42;   /* Skivan ej klar */
	goto fail;
    }

    do_close(me, me->ff);

    me->ff = ff = alloc_file(me);
    if (!ff) {
	err = 128 + 19;		/* Kan ej öppna fler filer */
	goto fail;
    }

    ff->vol = vol;
    ff->ix = me->ix;
    ff->opencmd = cmd0;
    unmangle_filename(ff->name, name);

    if (vol->dev) {
	chardev_open open_func = vol->dev->open;
	if (open_func)
	    err = open_func(me, ff);
	else
	    err = 0;
	goto fail;
    }

    if (name[0] == ' ') {
        /* Empty filename (readdir) */

	mode = HF_DIRECTORY;
        openflags = 0;
	ff->name[0] = '\0';	/* Drop any extension */
    } else {
        mode = HF_BINARY;
        mode |= (cmd0 & FF_PREPARE) ? 0 : HF_RETRY;
        openflags = (cmd0 & FF_PREPARE) ? (O_RDWR | O_TRUNC | O_CREAT) : O_RDWR;
    }

    ff->hf = hf = open_host_file(mode, vol->path, ff->name, openflags);
    if (hf) {
	err = 0;
	if (mode == HF_DIRECTORY && (cmd0 & FF_BINARY))
	    err = read_dir_data(me, ff);
    } else {
	switch (errno) {
	case EACCES:
	    err = 128 + 39;
	    break;
	case EROFS:
	    err = 128 + 43;
	    break;
	case EIO:
	    err = 128 + 35;
	    break;
	case ENOTDIR:
	    err = 128 + 48;
	    break;
	default:
	    err = 128 + 21;	/* Hittar ej filen (default error) */
	    break;
	}
    }

fail:
    if (ff && (err & 128))
	do_close(me, ff);

    return send_reply(me, err);
}

static unsigned int do_read_block(struct abcprint *me, unsigned int len)
{
    struct fileop_file *ff = me->ff;
    struct host_file *hf;
    int err;
    unsigned int dlen;

    assert(len < 65536);

    /* If read is supported to char devices in the future, fix this... */
    if (ff->vol->dev)
	return send_reply(me, 128 + 52); /* Ej till denna enhet */

    if (!file_open(ff))
        return send_reply(me, 128 + 45); /* Fel logiskt filnummer */

    hf = ff->hf;

    if (ff->abc) {
	dlen = min(((const char *)ff->abc->buf + ff->abc->len)
		   - (const char *)ff->abc->data, len);
	errno = 0;
	if (dlen > 0) {
	    memcpy(me->data + 2, ff->abc->data, dlen);
	    ff->abc->data = (const char *)ff->abc->data + dlen;
	}
    } else {
	if (file_mode(ff) == HF_DIRECTORY)
	    return send_reply(me, 128 + 37);   /* Felaktigt recordformat */

	clearerr(hf->f);
	dlen = fread(me->data + 2, 1, len, hf->f);
	if (!ferror(hf->f))
	    errno = 0;
    }

    if (dlen == 0) {
	switch (errno) {
	case 0:
            /* EOF - definitely not the default return! */
	    err = 128 + 34;	/* Slut på filen (är det rätt?) */
	    break;
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
        return send_reply(me, err);
    } else if (dlen < len) {
	memset(me->data + 2 + dlen, 0, len - dlen);
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

    if (ff->vol->dev)
	return 128 + 52;	/* Ej till denna enhet */

    if (!file_open(ff))
        return 128 + 45;         /* Fel logiskt filnummer */

    if (ff->abc) {
	if (ff->abc->is_text)
	    return 128 + 37;	/* Felaktigt recordformat */
	if (pos > ff->abc->blocks * 253)
	    return 128 + 38;	/* Recordnummer utanför filen */
	ff->abc->data = (const char *)ff->abc->buf + pos;
	return 0;
    }

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

    /* If read is supported to char devices in the future, fix this... */
    if (ff->vol->dev)
	return send_reply(me, 128 + 52);

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
	dlen = read_dir_entry(me, data1 + 2);
	err = dlen ? 0 : 128;	/* 128 = end of file */
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

#ifdef _WIN32
    /* ABC sends CR LF as line endings, which matches Windows */
    eolcvt = false;
#endif

    if (ff->vol->dev) {
	chardev_write write_func = ff->vol->dev->write;
	if (write_func)
	    err = write_func(me, ff, me->data, len);
	else
	    err = 128 + 52;
	goto fail;
    }

    if (!file_open(ff)) {
        err = 128 + 45;
	goto fail;
    }

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
    int err;
    struct fileop_file *ff = me->ff;
    char newname[16];

    if (!file_open(ff)) {
        err = 128 + 45;
	goto fail;
    }

    if (file_mode(ff) == HF_DIRECTORY) {
        err = 128 + 39;         /* Directories are readonly */
	goto fail;
    }

    unmangle_filename(newname, me->argbuf.c);

    if (!rename_file(ff->hf, newname)) {
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

fail:
    return send_reply(me, err);
}

/* Deletes and closes a file */
static unsigned int fop_delete(struct abcprint *me)
{
    int err;
    struct fileop_file *ff = me->ff;

    if (!file_open(ff)) {
        err = 128 + 45;
	goto fail;
    }

    if (file_mode(ff) == HF_DIRECTORY) {
        err = 128 + 39;         /* Directories are readonly */
	goto fail;
    }

    nuke_file(ff->hf);		/* Mark file for delete on close */
    err = do_close(me, ff);

fail:
    return send_reply(me, err);
}

/* Generic command, basically ioctl but with a string */
static unsigned int fop_cmd(struct abcprint *me)
{
    int err = 128 + 37;		/* Felaktigt recordformat = unsupported cmd */

    if (me->ix && !file_open(me->ff))
	err = 128 + 45;		/* File specified but does not exist */
    else if (me->datalen == 0)
	err = 0;		/* Null command */

    return send_reply(me, err);
}

/* Invalid file operation */
static unsigned int fop_invalid(struct abcprint *me)
{
    return send_reply(me, 128 + 52); /* Ej till denna enhet */
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
    me->endframe = NULL;
    me->csum = 0xff;
}

/* Reset the fileop session */
void fileop_reset(struct abcprint *me)
{
    do_init(me, 253);
    fileop_goto_init_state(me);
}

/*
 * Initialize the fileop data structures; the structure will be
 * initialized to all zero and fileop_reset() will called independently
 * afterwards
 */
void fileop_init(struct abcprint *me)
{
    for (size_t i = 0; i < MAX_FILES; i++)
	me->files[i].i = i;
}

/*
 * Shut down the fileop session and free all memory
 */
void fileop_shutdown(struct abcprint *me)
{
    do_close_all(me);
    cleanup_volumes(me);

    if (me->data) {
	free(me->data);
	me->data = NULL;
	me->datasize = 0;
    }
}

/* List available volumes; this includes the default volumes */
static unsigned int fop_listvol(struct abcprint *me)
{
    unsigned char *dp = me->data;
    unsigned char *vp = dp + 2;
    size_t bytes;
    int i;

    for (i = 0; i < me->vols; i++) {
	*vp++ = me->volumes[i].mode;
	vp = mempcpy(vp, me->volumes[i].name, 3);
    }

    *vp++ = '\0';
    bytes = vp - dp - 2;
    dp[0] = bytes;
    dp[1] = bytes >> 8;

    send_reply(me, 0);		/* This command is always successful */
    return send_data(me, dp, bytes + 2);
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
    bool isrst;			/* This command is allowed to reset sequence numbers */
    fop_func runs[2];		/* Command phases */
};

/* If byte_count < 0, then the value is to be interpreted as a data length */

/* Command info starting at 0xA0... */
#define FIRST_CMD 0xA0
static const struct fop fops[] = {
    { 14, "OPEN_A",  false, { NULL, fop_open } },  /* A0: OPEN ASCII */
    { 14, "OPEN_B",  false, { NULL, fop_open } },  /* A1: OPEN BINARY */
    { 14, "PREP_A",  false, { NULL, fop_open } },  /* A2: PREPARE ASCII */
    { 14, "PREP_B",  false, { NULL, fop_open } },  /* A3: PREPARE BINARY */
    {  0, "INPUT",   false, { NULL, fop_input } },   /* A4: INPUT */
    {  2, "GET",     false, { NULL, fop_get } },     /* A5: READ BLOCK (GET) */
    {  2, "PRINT",   false, { arg_len, fop_print } },   /* A6: PRINT */
    {  0, "CLOSE",   false, { NULL, fop_close } },  /* A7: CLOSE */
    {  0, "CLOSALL", true,  { NULL, fop_closeall } }, /* A8: CLOSE ALL */
    {  0, "INIT",    true,  { NULL, fop_init } },    /* A9: close all and reset state */
    { 11, "RENAME",  false, { NULL, fop_rename } },  /* AA: RENAME */
    {  0, "DELETE",  false, { NULL, fop_delete } },  /* AB: DELETE (KILL) */
    {  2, "PREAD",   false, { NULL, fop_pread } },   /* AC: PREAD */
    {  2, "PWRITE",  false, { arg_blkno, fop_pwrite } },  /* AD: PWRITE */
    {  2, "BLKSIZE", false, { NULL, fop_blksize } }, /* AE: SET BLOCK SIZE */
    {  2, "INITSZ",  false, { NULL, fop_initsz } },  /* AF: INIT BLOCK SIZE */
    {  0, "SEEK0",   false, { NULL, fop_seek } },   /* B0: SEEK0 (REWIND) */
    {  1, "SEEK1",   false, { NULL, fop_seek } },   /* B1: SEEK1 */
    {  2, "SEEK2",   false, { NULL, fop_seek } },   /* B2: SEEK2 */
    {  3, "SEEK3",   false, { NULL, fop_seek } },   /* B3: SEEK3 */
    {  4, "SEEK4",   false, { NULL, fop_seek } },   /* B4: SEEK4 */
    {  5, "SEEK5",   false, { NULL, fop_seek } },   /* B5: SEEK5 */
    {  6, "SEEK6",   false, { NULL, fop_seek } },   /* B6: SEEK6 */
    {  7, "SEEK7",   false, { NULL, fop_seek } },   /* B7: SEEK7 */
    {  8, "SEEK8",   false, { NULL, fop_seek } },   /* B8: SEEK8 */
    {  2, "PUT",     false, { arg_len, fop_put } },     /* B9: PUT */
    {  0, "LISTVOL", false, { NULL, fop_listvol } }, /* BA: LIST VOLUMES */
    {  2, "CMD",     false, { arg_len, fop_cmd } },      /* BB: GENERIC COMMAND */
    {  0, "invalid", false, { NULL, fop_invalid } }  /* invalid command opcode */
};

bool file_op(struct abcprint *me, unsigned char c)
{
    *me->bytep++ = c;
    me->csum += c;

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

	if (me->fop->isrst)	/* INIT commands force new sequence numbers */
	    me->nextseq = me->cmd[1];

        if (tracing(TRACE_PR)) {
            fprintf(tracef, "PR:  %-7s : FF %02X %02x %04x",
		    me->fop->name, me->cmd[0], me->cmd[1], me->ix);
	    if (me->cmd[1] != me->nextseq)
		fprintf(tracef, "  <seq err %02x expected %02x>",
			me->cmd[1], me->nextseq);
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

    if (me->endframe) {
	if (me->endframe[0] != 0xef || me->csum) {
	    if (tracing(TRACE_PR)) {
		fprintf(tracef, "PR:  %-7s : bad endframe signature %02x %02x (expected %02x %02x)\n",
			me->fop->name, me->endframe[0], me->endframe[1],
			0xef, (uint8_t)(me->endframe[1] - me->csum));
	    }
	    goto drop_frame;
	} else if (me->cmd[1] != me->nextseq) {
	    if (tracing(TRACE_PR)) {
		fprintf(tracef, "PR:  %-7s : dropping out of sequence frame (%02x expected %02x)\n",
			me->fop->name, me->cmd[1], me->nextseq);
	    }
	    goto drop_frame;
	} else {
	    /* Valid frame, increase sequence number */
	    me->nextseq++;
	}
    }

    fop_func do_next = me->fop->runs[me->fseq];

    me->byte_count = do_next ? do_next(me) : 0;

    int eof_size = 0;
    if (me->fseq == 0) {
	me->endframe = me->data + me->byte_count;
	eof_size = 2;
    }
    if (me->byte_count + eof_size) {
	me->datalen = me->byte_count;
	me->byte_count += eof_size;
	me->bytep = me->bufp = me->data;
	me->fseq++;
	if (tracing(TRACE_PR)) {
	    if (me->datalen) {
		fprintf(tracef, "PR:  %-7s : <expect %u more bytes%s>\n",
			me->fop->name, me->datalen,
			eof_size ? " + endframe" : "");
	    }
	}
	return true;
    }

drop_frame:
    fileop_goto_init_state(me);
    return false;
}

/*
 * Functions for printers as character devices
 */
static int chardev_pr_open(struct abcprint *me, struct fileop_file *ff)
{
    (void)me;
    char *p = strchr(ff->name, '.');
    if (p)
	*p = '\0';		/* Strip extension if present */
    return 0;
}

static int chardev_pr_write(struct abcprint *me, struct fileop_file *ff,
			    const void *data, size_t len)
{
    return printer_write(me, &ff->hf, ff->name, data, len);
}

static int chardev_pr_close(struct abcprint *me, struct fileop_file *ff)
{
    return printer_close(me, &ff->hf, ff->name);

}

static const struct chardev chardev_pr = {
    .open  = chardev_pr_open,
    .write = chardev_pr_write,
    .close = chardev_pr_close,
};
