/*
 * Load a file into memory. The syntax is:
 *
 * [[:]memspace][@address][+bytes][,format][,=]filename
 *
 * The use of = before the filename is strongly recommended.
 *
 * If "format" is unspecified, this code will automatically try to detect
 * one of Intel Hex, S-records, or binary.
 *
 * See HELP.txt for a list of memspaces.
 */

#include "compiler.h"
#include "sysload.h"
#include "hostfile.h"
#include "screen.h"
#include "z80.h"
#include "chartype.h"

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

#define LOAD_ERR SIZE_MAX

static size_t load_ihex(FILE *file, size_t len, struct as *as, size_t offset)
{
    int c;
    int hval = 0;
    size_t bytes = 0;
    int left = 0;
    int lpos = 0;
    uint8_t ldata[255+5];	/* Record data including metadata */
    uint8_t *p = NULL;
    uint8_t csum = 0;
    size_t baseaddr = 0;

    rewind(file);

    while (bytes < len) {
	if (ferror(file))
	    goto invalid;

	c = fgetc(file);

	/* End of line/end of file? */
	if (is_eoln(c) || is_eof(c)) {
	    if (lpos > 0) {
		if (lpos < 1+4*2) /* Must have :ccaaaatt at minimum */
		    goto invalid;

		unsigned int rlen = ldata[0];
		size_t laddr = (ldata[1] << 8) + ldata[2];
		uint8_t ltype = ldata[3];

		if (csum)
		    goto invalid;

		if (left)
		    goto invalid;

		switch (ltype) {
		case 0:		/* Data */
		{
		    size_t addr = laddr + baseaddr;
		    if (rlen > len - bytes)
			rlen = len - bytes;
		    load_data(as, addr + offset, ldata+4, rlen);
		    bytes += rlen;
		    break;
		}

		case 1:		/* End of file */
		    len = bytes;
		    break;

		case 2:		/* Segment address */
		    if (len != 2)
			goto invalid;
		    baseaddr = laddr << 4;
		    break;

		case 4:		/* Linear address */
		    if (len != 2)
			goto invalid;
		    baseaddr = laddr << 16;
		    break;

		case 3:		/* Start address CS:IP */
		case 5:		/* Start address linear */
		    if (len != 4)
			goto invalid;
		    /* Otherwise ignore */
		    break;

		default:	/* Unknown record type */
		    goto invalid;
		}
	    }

	    if (is_eof(c))
		len = bytes;	/* Done! */

	    lpos = 0;
	} else if (is_white(c)) {
	    /* Skip whitespace or similar */
	} else if (c < ' ') {
	    /* All other control characters invalid, even in comment */
	    goto invalid;
	} else if (lpos < 0) {
	    /* Skipping a comment */
	} else if (c == ';' || c == '#') {
	    /* Start of comment */
	    lpos = -1;
	} else if (!lpos++) {
	    if (c != ':')
		goto invalid;	/* Invalid */
	    hval = 0;
	    left = 5;		/* Length of header+checksum in bytes */
	    csum = 0;
	    p = ldata;
	} else {
	    int hdig = hexval(c);

	    if (hdig < 0 || !left)
		goto invalid;

	    /*
	     * The ! here is correct, as this is the byte count including
	     * this one, *including* the leading :
	     */
	    if (!(lpos & 1)) {
		hval = hdig << 4;
	    } else {
		/* Byte complete */

		hval += hdig;
		*p++ = hval;
		csum += hval;
		left--;
		if (lpos == 3)	  /* After :cc */
		    left += hval; /* Add data length to bytes needed */
		hval = 0;
	    }
	}
    }
    return bytes;

 invalid:
    return LOAD_ERR;
}

static size_t load_srec(FILE *file, size_t len, struct as *as, size_t offset)
{
    int c;
    uint8_t hval = 0;
    size_t bytes = 0;
    int left = 0;
    int lpos = 0;
    uint8_t ldata[255+7];	/* Record data including metadata */
    uint8_t *p = NULL;
    uint8_t csum = 0;

    rewind(file);

    while (bytes < len) {
	if (ferror(file))
	    break;

	c = fgetc(file);

	/* End of line/end of file? */
	if (is_eoln(c) || is_eof(c)) {
	    if (lpos > 0) {
		if (lpos < 4)
		    goto invalid;
		
		static const int addrlen[10] =
		    { 2, 2, 3, 4, 2, 2, 3, 4, 3, 4 };
		unsigned int ltype = p[0];
		unsigned int rlen = p[1];
		unsigned int alen = addrlen[p[0]];
		size_t addr = 0;
		const uint8_t *dp;

		if (left)
		    goto invalid;	/* Truncated record */

		rlen -= alen + 1;	/* Count data bytes only */
		if (rlen < alen + 1)
		    /* Record too short: need minimum address + checksum */
		    goto invalid;

		rlen -= alen + 1;

		/* Note: type is not included in the checksum */
		if (csum - ltype != 0xff)
		    goto invalid; /* Invalid checksum */

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
		    if (rlen > len - bytes)
			rlen = len - bytes;
		    load_data(as, addr + offset, dp, rlen);
		    bytes += rlen;
		    break;
		case 7:
		case 8:
		case 9:
		    /* Start of program, also end of file */
		    len = bytes;
		    break;
		}
	    }

	    if (is_eof(c))
		len = bytes;	/* End of file */

	    lpos = 0;
	} else if (is_white(c)) {
	    /* Whitespace or similar - do nothing */
	} else if (c < ' ') {
	    /* All other control characters invalid, even in comment */
	    goto invalid;
	} else if (lpos < 0) {
	    /* Skipping a comment */
	} else if (c == ';' || c == '#') {
	    /* Start of comment */
	    lpos = -1;
	} else if (!lpos++) {
	    if (c != 'S')
		goto invalid;
	    hval = 0;		/* S acts like a leading zero */
	    left = 2;		/* Type and byte count */
	    csum = 0;
	    p = ldata;
	} else {
	    int hdig = hexval(c);

	    /* Not a hex digit, or beyond the end of the record */
	    if (hdig < 0 || !left)
		goto invalid;

	    /*
	     * Note that lpos here is the number of bytes including this one,
	     * and including the leading S, which effectively functions as
	     * a leading zero for the type.
	     */
	    if (lpos & 1) {
		hval = hdig << 4;
	    } else {
		/* Byte complete */

		hval += hdig;
		*p++ = hval;
		csum += hval;
		left--;
		if (lpos == 4)      /* After Stcc where t = type, cc = count */
		    left += hval;   /* Add byte count for rest of record */
	    }
	}
    }

    return bytes;

 invalid:
    return LOAD_ERR;
}

static size_t load_bin(FILE *file, size_t len, struct as *as, size_t addr)
{
    int c;
    size_t bytes = 0;

    rewind(file);

    while (bytes < len) {
	c = fgetc(file);
	if (ferror(file))
	    goto invalid;
	if (c < 0)		/* EOF */
	    break;
	do_as_load(as, addr++, c);
	bytes++;
    }
    return bytes;

 invalid:
    return LOAD_ERR;
}

typedef size_t (*load_func)(FILE *file, size_t len, struct as *as, size_t addr);

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
static size_t load_any(FILE *file, size_t len, struct as *as, size_t addr)
{
    const struct file_format *fmt;
    size_t bytes = LOAD_ERR;

    for (fmt = file_formats; fmt->loader; fmt++) {
	/* Validate the file contents by loading to null */
	bytes = fmt->loader(file, len, null_as, addr);
	if (bytes != LOAD_ERR)
	    bytes = fmt->loader(file, len, as, addr);

	if (bytes && bytes != LOAD_ERR)
	    break;
    }
    return bytes;
}

int load_sysfile(const char *filespec)
{
    FILE *f;
    const char *p = filespec;
    struct as *as = NULL;
    load_func loader = NULL;
    size_t addr = 0;
    size_t spn;
    size_t len = SIZE_MAX;
    const char *filename = NULL;
    char c;
    int commas = 0;

    while ((c = *p++)) {
	spn = strcspn(p, "+@,=");
	char *ep = (char *)p + spn;

	switch (c) {
	case '@':
	    addr = strtoul(p, &ep, 0);
	    break;
	case '+':
	    len = strtoul(p, &ep, 0);
	    break;
	case ',':
	{
	    if (commas++) {
		/* Already seen a comma, must be filename [double comma] */
		filename = p;
		break;
	    }
	    if (!*p)		/* Empty format: autodetect */
		break;

	    const struct file_format *fmt;
	    for (fmt = file_formats; fmt->loader; fmt++) {
		if (!strncmp(fmt->name, p, spn) && !fmt->name[spn]) {
		    loader = fmt->loader;
		    break;
		}
	    }

	    if (!loader) {
		/* Not a valid format name, may be the filename */
		if (strchr(p, '='))
		    goto parse_err;
		filename = p;
	    }
	    break;
	}
	case '=':
	    filename = p;
	    break;
	default:
	    p--;
	    spn++;
	    if (as)
		goto parse_err;
	    as = get_addrspace(p, spn);
	    if (!as) {
		/* Not a valid address space name, maybe the filename? */
		if (strchr(p, '='))
		    goto parse_err;
		filename = p;
	    }
	    break;
	}

	if (filename)
	    break;
	
	if (ep != p + spn)
	    goto parse_err;
	p = ep;
    }

    if (!as) {
	as = get_addrspace("cpu", 3);
	if (!as)
	    goto parse_err;	/* No memspace, and "cpu" undefined */
    }

    f = fopen(filename, "rb");
    if (!f) {
	fprintf(stderr, "Can't open file: %s: %s\n",
		filename, strerror(errno));
	return -1;
    }

    if (!loader)
	loader = load_any;

    len = loader(f, len, as, addr);

    int rv = 0;
    if (len == LOAD_ERR) {
	fprintf(stderr, "%s: Invalid file format\n", filename);
	rv = -1;
    }

    fclose(f);

    /* After using the load functions, need to sync */
    as_sync();

    return rv;

 parse_err:
    fprintf(stderr, "Invalid load file spec: %s\n", filespec);
    return -1;
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

    if (!as->len)
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

    if (!(as->flags & AS_NODUMP))
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

    for (as = addrspaces; as; as = as->next) {
	if (!(as->flags & (AS_NODUMP|AS_NODUMP_ALL)))
	    dump_as(as, dirpath);
    }

    trigger_screen_refresh();
    abc_screenshot(dirpath);
    dump_txt_screen(dirpath, NULL);

    hf = dump_file(HF_TEXT, dirpath, NULL, "regs", ".txt");
    if (hf) {
	z80_dumpregs(hf->f, NULL);
	if (!ferror(hf->f))
	    keep_file(hf);

	close_file(&hf);
    }

    free(dirpath);
}
