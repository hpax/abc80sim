/*
 * Load a file into memory. The syntax is:
 *
 * [[memspace][@address][,format],]filename
 *
 * If "format" is unspecified, this code will automatically try to detect
 * one of Intel Hex, S-records, or binary.
 *
 * The memory spaces supported are:
 *
 * null  - discard
 * rom	 - BASIC, DOS, option ROM, printer ROM, etc.
 *         loads into both rom40 and rom80 space for the first 16K (ABC80 only)
 * rom40 - 40-character BASIC ROM
 * rom80 - 80-character BASIC ROM
 * ram   - regular working memory; includes paged-out memory
 *         (ABC80 > 32K, ABC802 etc.)
 * vram  - text video memory
 * cpu   - memory as seen by CPU
 *
 * Not yet implemented:
 *    sram  - auxiliary SRAM for ABC80 128K+
 *    fg/hr - high res graphics memory
 */

#include "compiler.h"
#include "sysload.h"
#include "hostfile.h"
#include "screen.h"
#include "z80.h"

static size_t load_data(struct as *as, size_t offs,
			const uint8_t *data, size_t len)
{
    if (as->flags & AS_NOLOAD) {
	/* No need to jump through all the hoops... */
	return offs + len;
    } else {
	while (len--)
	    do_as_load(as, offs++, *data++);
	return offs;
    }
}

/*
 * Return the hex value of a single character, or negative if invalid.
 * The hex digit must be upper case or considered invalid.
 */
static inline int hexval(int c)
{
    if (c <= '9')
	return c - '0';
    if (c < 'A')
	return -1;
    if (c <= 'F')
	return c - 'A' + 10;
    return -1;
}

static inline bool is_eoln(int c)
{
    return c == '\n' || c == '\r';
}

static inline bool is_eof(int c)
{
    return c == EOF || c == ('Z' & 0x1f);
}

static inline bool is_white(int c)
{
    return (c >= '\a' && c <= '\r') || c == 0x7f || c == 0xff;
}

static int load_ihex(FILE *file, struct as *as, size_t offset)
{

    int c;
    int hval = 0;
    int bytes = 0;
    int left = 0;
    int lpos = 0;
    uint8_t ldata[255+5];	/* Record data including metadata */
    uint8_t *p = NULL;
    uint8_t csum = 0;
    size_t baseaddr = 0;

    rewind(file);

    while (1) {
	if (ferror(file))
	    return -1;

	c = fgetc(file);

	/* End of line/end of file? */
	if (is_eoln(c) || is_eof(c)) {
	    if (lpos > 0) {
		unsigned int len = ldata[0];
		size_t laddr = (ldata[1] << 8) + ldata[2];
		uint8_t ltype = ldata[3];

		if (csum)
		    return -1;	/* Invalid checksum */

		if (left)
		    return -1;	/* Truncated record */

		switch (ltype) {
		case 0:		/* Data */
		{
		    size_t addr = laddr + baseaddr;
		    load_data(as, addr + offset, ldata+4, len);
		    bytes += len;
		    break;
		}

		case 1:		/* End of file */
		    return (len == 0) ? bytes : -1;

		case 2:		/* Segment address */
		    if (len != 2)
			return -1;
		    baseaddr = laddr << 4;
		    break;

		case 4:		/* Linear address */
		    if (len != 2)
			return -1;
		    baseaddr = laddr << 16;
		    break;

		case 3:		/* Start address CS:IP */
		case 5:		/* Start address linear */
		    if (len != 4)
			return -1;
		    /* Otherwise ignore */
		    break;

		default:	/* Unknown record type */
		    return -1;
		}
	    }

	    if (is_eof(c))
		return bytes;	/* Done! */

	    lpos = 0;
	    continue;
	}

	/* Whitespace or similar? */
	if (is_white(c))
	    continue;

	/* All other control characters -> invalid, even in apparent comment */
	if (c < ' ')
	    return -1;

	/* Skip this line (comment?) */
	if (lpos < 0)
	    continue;

	/* Seems line a comment? */
	if (c == ';' || c == '#') {
	    lpos = -1;
	    continue;
	}

	if (!lpos) {
	    if (c != ':')
		return -1;	/* Invalid */
	    hval = 0;
	    left = 5;		/* Length of header+checksum in bytes */
	    csum = 0;
	    p = ldata;
	} else {
	    int hdig = hexval(c);

	    if (hdig < 0)
		return hdig;	/* Invalid */

	    hval = (hval << 4) + hdig;

	    if (lpos & 1)
		continue;	/* Middle of a byte */

	    if (!left)
		return -1;	/* Overrun */

	    *p++ = hval;
	    csum += hval;
	    left--;
	    if (lpos == 2)
		left += hval; /* Add data length to bytes needed */
	    hval = 0;
	}
	lpos++;
    }
}

static int load_srec(FILE *file, struct as *as, size_t offset)
{
    int c;
    int hval = 0;
    int bytes = 0;
    int left = 0;
    int lpos = 0;
    uint8_t ldata[255+7];	/* Record data including metadata */
    uint8_t *p = NULL;
    uint8_t csum = 0;

    rewind(file);

    while (1) {
	if (ferror(file))
	    return -1;

	c = fgetc(file);

	/* End of line/end of file? */
	if (is_eoln(c) || is_eof(c)) {
	    if (lpos > 0) {
		static const int addrlen[10] =
		    { 2, 2, 3, 4, 2, 2, 3, 4, 3, 4 };
		unsigned int ltype = p[0];
		int len = p[1];
		int alen = addrlen[p[0]];
		size_t addr = 0;
		const uint8_t *dp;

		if (left)
		    return -1;	/* Truncated record */

		len -= alen + 1; /* Count data bytes only */
		if (len < 0) {
		    /* Record too short: need minimum address + checksum */
		    return -1;
		}

		/* Note: type is not included in the checksum */
		if (csum - ltype != 0xff)
		    return -1;	/* Invalid checksum */

		dp = ldata+2;	/* Address field */
		addr = 0;
		while (alen--)
		    addr = (addr << 8) + *dp++;

		switch (ldata[0]) {
		case 0:
		case 4:
		case 5:
		case 6:
		    /* Ignore record */
		    break;
		case 1:
		case 2:
		case 3:
		    /* Data record */
		    load_data(as, addr + offset, dp, len);
		    bytes += len;
		    break;
		case 7:
		case 8:
		case 9:
		    /* Start of program, also end of file */
		    return bytes;
		}
	    }

	    if (is_eof(c))
		return bytes;	/* Done! */

	    lpos = 0;
	    continue;
	}

	/* Whitespace or similar? */
	if (is_white(c))
	    continue;

	/* All other control characters -> invalid, even in apparent comment */
	if (c < ' ')
	    return -1;

	/* Skip this line (comment?) */
	if (lpos < 0)
	    continue;

	/* Seems line a comment? */
	if (c == ';' || c == '#') {
	    lpos = -1;
	    continue;
	}

	if (!lpos) {
	    if (c != 'S')
		return -1;	/* Invalid */
	    hval = 0;
	    left = 2;		/* Type and byte count */
	    csum = 0;
	    p = ldata;
	} else {
	    int hdig = hexval(c);

	    if (hdig < 0)
		return hdig;	/* Invalid */

	    hval = (hval << 4) + hdig;

	    /*
	     * The ! here is correct. The first "byte" is the type,
	     * which has only one digit.
	     */
	    if (!(lpos & 1))
		continue;	/* Middle of a byte */

	    if (!left)
		return -1;	/* Overrun */

	    *p++ = hval;
	    csum += hval;
	    left--;
	    if (lpos == 3)
		left += hval;	/* Add byte count for rest of record */

	    hval = 0;
	}
	lpos++;
    }
}

static int load_bin(FILE *file, struct as *as, size_t addr)
{
    int c;
    int bytes = 0;

    rewind(file);

    while (1) {
	if (ferror(file))
	    return -1;
	c = fgetc(file);
	if (c == EOF)
	    return bytes;
	do_as_load(as, addr++, c);
	bytes++;
    }
}

typedef int (*load_func)(FILE *file, struct as *as, size_t addr);

struct file_format {
    const char *name;
    load_func loader;
};

static const struct file_format file_formats[] =
{
    { "ihex", load_ihex },
    { "srec", load_srec },
    { "bin",  load_bin },
    { "any",  NULL }
};

/* Iterates through the loaders until one succeeds */
static int load_any(FILE *file, struct as *as, size_t addr)
{
    const struct file_format *fmt;
    int bytes = -1;

    for (fmt = file_formats; fmt->loader; fmt++) {
	/* Validate the file contents by loading to null */
	if (fmt->loader(file, null_as, addr) < 0)
	    continue;

	bytes = fmt->loader(file, as, addr);
	if (bytes >= 0)
	    break;
    }
    return bytes;
}

int load_sysfile(const char *filespec)
{
    FILE *f;
    const char *p = filespec;
    const char *comma;
    struct as *as;
    load_func loader = NULL;
    size_t addr = 0;
    int rv;

    as = get_addrspace("cpu", 3);

    while ((comma = strchr(p, ','))) {
	struct as *wms;
	const struct file_format *fmt;
	size_t len = comma-p;
	const char *at = strchr(p, '@');

	if (at && at < comma) {
	    char *ep;
	    addr = strtoul(at+1, &ep, 0);
	    if (ep != comma)
		return -1;
	    len = at-p;
	}

	wms = get_addrspace(p, len);
	if (wms) {
	    as = wms;
	    goto next;
	}

	fmt = file_formats;
	do {
	    if (!strncmp(fmt->name, p, len) && !fmt->name[len]) {
		loader = fmt->loader;
		goto next;
	    }
	} while ((fmt++)->loader);

	/* Doesn't match anything, assume it is actually the filename */
	break;

    next:
	p = comma+1;
	continue;
    }

    if (!as)
	return -1;		/* No memspace, and "cpu" undefined */

    f = fopen(p, "rb");
    if (!f) {
	fprintf(stderr, "Can't open file: %s: %s\n",
		p, strerror(errno));
	return -1;
    }

    if (!loader)
	rv = load_any(f, as, addr);
    else
	rv = loader(f, as, addr);

    fclose(f);

    do_as_sync(as);

    return rv;
}


/*
 * Dump an arbitrary memory namespace space to a file
 */
const char *memdump_path;

static void dump_memory_xlt(struct host_file *hf, struct xlt_addr *xlt)
{
    while (xlt->len) {
	size_t len;
	struct as_data asd;
	struct xlt_addr pxlt;

	len = as_translate_iter(xlt, &pxlt);
	if (!len)
	    break;

	asd = do_as_dump(pxlt.ao.as, pxlt.ao.offs);
	len = min(asd.len, len);
	if (!len)
	    break;

	fwrite(asd.data, 1, len, hf->f);

	xlt->len -= len;
	xlt->ao.offs += len;
    }
}

static void dump_memory_one_map(struct host_file *hf, struct as *as)
{
    struct xlt_addr xlt;

    xlt.ao.as   = as;
    xlt.ao.offs = 0;
    xlt.len     = as->len;

    dump_memory_xlt(hf, &xlt);
}

static void dump_as(struct as *as, const char *path)
{
    struct host_file *hf;

    if ((as->flags & AS_NODUMP) || !as->len)
	return;			/* Empty namespace */

    hf = dump_file(HF_BINARY, path, memdump_path, as->dump_name, ".bin");
    if (!hf)
        return;

    if (as->flags & AS_ONE_MAP) {
	dump_memory_one_map(hf, as);
    } else {
	unsigned int orig_map = as->map;
	unsigned int m;

	for (m = 0; m < as->nmaps; m++) {
	    as_set_map(as, m);
	    dump_memory_one_map(hf, as);
	}

	as_set_map(as, orig_map);
    }

    if (!ferror(hf->f))
        keep_file(hf);          /* It's good */

    close_file(&hf);
}

void dump_memory(const char *namespace)
{
    struct as *as;

    as = get_addrspace(namespace, strlen(namespace));
    if (!as)
	return;			/* Nothing to dump */

    dump_as(as, NULL);
}

/* Dump all memory spaces and other dumpables into a separate directory */
void dump_all(void)
{
    struct as *as;
    char *dirpath = NULL;
    struct host_file *hf;
    int i;

    /* Create a dump directory */
    for (i = 1; i <= 9999; i++) {
	char dirname[16];
	int err;

	snprintf(dirname, sizeof dirname, "dump%04d", i);
	dirpath = concat_path(memdump_path, dirname);
	if (!make_dir(dirpath)) {
	    err = 0;
	    break;
	}

	err = errno;
	free(dirpath);
	dirpath = NULL;

	if (err != EEXIST)
	    break;
    }
    if (!dirpath)
	return;

    for (as = addrspaces; as; as = as->next)
	dump_as(as, dirpath);

    abc_screenshot(dirpath);

    hf = dump_file(HF_TEXT, dirpath, NULL, "regs", ".txt");
    if (hf) {
	z80_dumpregs(hf->f, NULL);
	if (!ferror(hf->f))
	    keep_file(hf);

	close_file(&hf);
    }

    free(dirpath);
}
