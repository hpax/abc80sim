#include "sysload.h"

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
 * sys   - system memory as seen by CPU at system reset (default)
 *
 * Not yet implemented:
 *    sram  - auxiliary SRAM for ABC80 128K+
 *    fg/hr - high res graphics memory
 */

/* write_op will never be called when limit == 0 */
static struct load_data memspace_null = { "null", NULL, NULL, NULL, 0, 0 };
static const struct load_data *memspaces = &memspace_null;

static inline uint32_t write_byte(const struct load_data *ws, uint32_t addr,
				  uint8_t val)
{
    if (likely(addr < ws->limit)) {
	if (!ws->write_op) {
	    uint8_t *p = ws->buf;
	    p[addr & ws->mask] = val;
	} else {
	    ws->write_op(ws, addr & ws->mask, val);
	}
    }
    return addr + 1;
}

static uint32_t load_data(const struct load_data *ws, uint32_t addr,
			   const uint8_t *data, unsigned int len)
{
    while (len--)
	addr = write_byte(ws, addr, *data++);
    return addr;
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

static int load_ihex(FILE *file, const struct load_data *ws, uint32_t offset)
{

    int c;
    int hval = 0;
    int bytes = 0;
    int left = 0;
    int lpos = 0;
    uint8_t ldata[255+5];	/* Record data including metadata */
    uint8_t *p = NULL;
    uint8_t csum = 0;
    uint32_t baseaddr = 0;

    rewind(file);

    while (1) {
	if (ferror(file))
	    return -1;

	c = fgetc(file);

	/* End of line/end of file? */
	if (is_eoln(c) || is_eof(c)) {
	    if (lpos > 0) {
		unsigned int len = ldata[0];
		uint32_t laddr = (ldata[1] << 8) + ldata[2];
		uint8_t ltype = ldata[3];

		if (csum)
		    return -1;	/* Invalid checksum */

		if (left)
		    return -1;	/* Truncated record */

		switch (ltype) {
		case 0:		/* Data */
		{
		    uint32_t addr = laddr + baseaddr;
		    load_data(ws, addr + offset, ldata+4, len);
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

static int load_srec(FILE *file, const struct load_data *ws, uint32_t offset)
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
		uint32_t addr = 0;
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
		    load_data(ws, addr + offset, dp, len);
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

static int load_bin(FILE *file, const struct load_data *ws, uint32_t addr)
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
	addr = write_byte(ws, addr, c);
	bytes++;
    }
}

void sysload_add_memspace(const char *name, load_op write_op, void *buf,
			  uint32_t mask, uint32_t limit)
{
    struct load_data *ws;

    ws = calloc(1, sizeof *ws);
    if (!ws)
	return;

    ws->name = name;
    ws->write_op = write_op;
    ws->buf = buf;
    ws->mask = mask;
    ws->limit = limit;
    ws->next = memspaces;
    memspaces = ws;
}

static const struct load_data *get_memspace(const char *name, size_t len)
{
    const struct load_data *ws;

    for (ws = memspaces; ws; ws = ws->next) {
	if (!strncmp(ws->name, name, len) && !ws->name[len])
	    return ws;
    }
    return NULL;
}

typedef int (*load_func)(FILE *file, const struct load_data *ws, uint32_t addr);

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
static int load_any(FILE *file, const struct load_data *ws, uint32_t addr)
{
    const struct file_format *fmt;
    int bytes = -1;

    for (fmt = file_formats; fmt->loader; fmt++) {
	bytes = fmt->loader(file, ws, addr);
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
    const struct load_data *ws;
    load_func loader = NULL;
    uint32_t addr = 0;
    int rv;

    ws = get_memspace("cpu", 3);

    while ((comma = strchr(p, ','))) {
	const struct load_data *wms;
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

	wms = get_memspace(p, len);
	if (wms) {
	    ws = wms;
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

    if (!ws)
	return -1;		/* No memspace, and "cpu" undefined */

    f = fopen(p, "rb");
    if (!f) {
	fprintf(stderr, "Can't open file: %s: %s\n",
		p, strerror(errno));
	return -1;
    }

    if (!loader)
	rv = load_any(f, ws, addr);
    else
	rv = loader(f, ws, addr);

    fclose(f);

    return rv;
}
