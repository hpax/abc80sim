#include "compiler.h"
#include "z80.h"
#include "as.h"
#include "debug.h"
#include "ilog2.h"
#include "random.h"
#include "chartype.h"

/* -------------------------------------------------------------------------
 *  List of all registered address spaces
 * ------------------------------------------------------------------------- */
struct as *null_as;
struct as *addrspaces;

/* -------------------------------------------------------------------------
 *  Fill RAM area with (user-specified) random junk
 * ------------------------------------------------------------------------- */
static size_t ram_junk_len;
static void *ram_junk;

static void ram_init_junk(void *buf, size_t len)
{
    if (!ram_junk_len) {
	/* Default: initialize to zero */
	memset(buf, 0, len);
    } else if (!ram_junk) {
	/* Random content */
	genrand_data(buf, len);
    } else {
	/* Fixed content */
	uint8_t *p = buf;
	while (len) {
	    size_t bytes = min(len, ram_junk_len);
	    p = mempcpy(p, ram_junk, bytes);
	    len -= bytes;
	}
    }
}

/* Configure said junk */
void config_init_ram(const char *str)
{
    if (ram_junk) {
	free(ram_junk);
	ram_junk = NULL;
    }

    if (!str || !*str) {
	ram_junk_len = 0;
    } else if (str[0] == 'r') {
	ram_junk_len = 1;	/* Initialize to random */
    } else {
	unsigned int ndig = 0;
	const unsigned char *p = (const unsigned char *)str;
	uint8_t *q;

	while (*p) {
	    ndig += hexval((unsigned char)*p++) >= 0;
	}

	ndig &= ~1;
	if (!ndig)
	    return;

	ram_junk_len = ndig >> 1;
	ram_junk = q = malloc(ndig >> 1);
	if (!q)
	    return;

	p = (const unsigned char *)str;
	while (ndig) {
	    int v = hexval(*p++);
	    if (v < 0)
		continue;
	    if (--ndig & 1)
		*q = v << 4;
	    else
		*q++ |= v;
	}
    }
}

/* -------------------------------------------------------------------------
 *  Initialize a memory area according to the init value from ops
 * ------------------------------------------------------------------------- */
void mem_buf_init(void *buf, size_t bytes, int init)
{
    if (init < 0)
	ram_init_junk(buf, bytes);
    else
	memset(buf, init, bytes);
}

/* -------------------------------------------------------------------------
 *  Generic address space constructors and tools
 * ------------------------------------------------------------------------- */
struct as *as_new_space(const char *name, const struct as_ops *ops,
			 size_t len, unsigned int nmaps)
{
    struct as *as = calloc(1, sizeof *as);
    if (!as)
	return NULL;

    as->name = name;
    as->dump_name = !strcmp(name, "cpu") ? "mem" : name; /* Historic */
    as->ops = ops;
    as->nmaps = nmaps;
    as->len = len;
    as->mask = ~(size_t)0;
    as->grain = MAX_GRAIN;

    /* Add to linked list */
    as->next   = addrspaces;
    addrspaces = as;

    if (tracing(TRACE_MAP)) {
	fprintf(tracef, "MAP: new as %s len 0x%zx maps %u\n",
		as->name, as->len, as->nmaps);
    }

    return as;
}

/*
 * The name, len pair allows for a separator other than \0.
 */
pure_func struct as *get_addrspace(const char *name, size_t len)
{
    struct as *as;

    for (as = addrspaces; as; as = as->next) {
	if (!strncmp(as->name, name, len) && !as->name[len])
	    return as;
    }
    return NULL;
}

/* ------------------------------------------------------------------------
 * Address space backed by a memory buffer (which may be pre-allocated
 * or not); initialize the memory buffer according to the ops.
 * ------------------------------------------------------------------------- */
struct as *new_mem(const char *name, size_t len, unsigned int nmaps,
		   void *buf, const struct as_ops *ops)
{
    const size_t bytes = nmaps*len;
    struct as *as = as_new_space(name, ops, len, nmaps);

    if (!buf) {
	buf = malloc(bytes);
	if (!buf)
	    return NULL;
    }
    mem_buf_init(buf, bytes, ops->init);

    as->p.data = buf;
    if (is_power2(len)) {
	as->grain = ilog2_sz(len);
	as->mask = grain_mask(as->grain);
    }
    return as;
}

pure_func struct as_data mem_as_dump(struct as *as, size_t offs)
{
    struct as_data asd;

    asd.data = as->p.data + offs;
    asd.len  = as->len - offs;

    return asd;
}

/* -------------------------------------------------------------------------
 *  Specializations of the memory buffer address spaces: ROM and RAM
 * ------------------------------------------------------------------------- */
void rom_as_write(struct as *as, size_t offs, uint8_t v)
{
    (void)as;
    (void)offs;
    (void)v;
}

const struct as_ops ram_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = NULL,		/* Just write it */
    .dump  = mem_as_dump,
    .load  = NULL,		/* Just load it */
    .sync  = NULL,		/* No syncing */
    .init  = -1			/* Powers up to junk */
};

const struct as_ops rom_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .dump  = mem_as_dump,
    .load  = NULL,		/* Just load it */
    .sync  = NULL,		/* No syncing */
    .init  = 0xff		/* Powers up to FF */
};

struct as *new_ram(const char *name, size_t len, unsigned int nmaps, void *buf)
{
    return new_mem(name, len, nmaps, buf, &ram_as_ops);
}
struct as *new_rom(const char *name, size_t len, unsigned int nmaps, void *buf)
{
    return new_mem(name, len, nmaps, buf, &rom_as_ops);
}

/* -------------------------------------------------------------------------
 *  Paged memory space
 * ------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------
 *  Paged memory space
 * ------------------------------------------------------------------------- */

static struct asoffs page_as_translate(struct asoffs vso)
{
    struct asoffs pso;

    pso = vso.as->p.page[vso.offs >> vso.as->grain];
    pso.offs += vso.offs & grain_mask(vso.as->grain);

    return pso;
}

void as_set_pages(struct as *vas, size_t voffs, unsigned int map,
		  struct as *pas, size_t poffs, size_t len)
{
    unsigned int pages;
    const size_t psize = grain_size(vas->grain);
    struct asoffs *p = vas->p.page;

    assert(((voffs|poffs|len) & (psize-1)) == 0); /* Must be page aligned */
    assert(map < vas->nmaps);
    assert(vas != pas);

    pages = len >> vas->grain;
    p += ((map * vas->len) + voffs) >> vas->grain;

    if (tracing(TRACE_MAP)) {
	fprintf(tracef, "MAP: as %s map %u offs 0x%zx len 0x%zx -> "
		"as %s offs 0x%zx\n",
		vas->name, map, voffs, len, pas->name, poffs);
    }

    while (pages--) {
	p->as = pas;
	p->offs = poffs;

	p++;
	poffs += psize;
    }
}

struct as *as_new_pagespace(const char *name, size_t len,
			    unsigned int nmaps, unsigned int grain)
{
    const size_t psize = grain_size(grain);
    struct asoffs *p;
    unsigned int i;
    struct as *as = as_new_space(name, NULL, len, nmaps);
    if (!as)
	return NULL;

    /* Granularity of the page tables as a power of 2 */
    as->grain = grain;
    assert((len & (psize - 1)) == 0);

    p = calloc(nmaps * (len >> grain), sizeof *p);
    if (!p)
	return NULL;

    as->p.page = p;
    as->flags |= AS_ONE_MAP;

    as->translate = page_as_translate;

    /* Initialize all pages to point to the null address space */
    for (i = 0; i < nmaps; i++) {
	as_set_pages(as, 0, i, null_as, 0, len);
    }

    return as;
}

/* -------------------------------------------------------------------------
 *  Alias address space; basically an optimized single repointable page
 * ------------------------------------------------------------------------- */
struct as *as_new_aliasspace(const char *name, size_t len)
{
    struct as *as = as_new_space(name, NULL, len, 1);
    if (!as)
	return NULL;

    as->flags |= AS_ALIAS;

    return as;
}

void as_point_alias(struct as *alias_as, struct as *parent_as, size_t offs)
{
    alias_as->p.parent_as = parent_as;
    alias_as->ops = parent_as->ops;
    alias_as->base = offs;

    if (tracing(TRACE_MAP)) {
	fprintf(tracef, "MAP: as %s = as %s offs 0x%zx\n",
		alias_as->name, parent_as->name, offs);
    }
}

/* -------------------------------------------------------------------------
 *  Create and initialize a new alias space (convenience function).
 * ------------------------------------------------------------------------- */
struct as *as_alias(const char *name, size_t len,
		    struct as *parent_as, size_t offs, enum as_flags flags)
{
    struct as *as = as_new_aliasspace(name, len);
    if (!as)
	return NULL;

    as_point_alias(as, parent_as, offs);
    as->flags |= flags;

    return as;
}

/* -------------------------------------------------------------------------
 *  Null address space (empty bus)
 * ------------------------------------------------------------------------- */
const struct as_ops null_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .dump  = mem_as_dump,
    .load  = rom_as_write,	/* Drop attempts at loading on the floor */
    .sync  = NULL,		/* No syncing */
    .init  = 0xff
};

#define NULL_BUF_SIZE	4096	/* Must be a power of 2 */

static void null_as_init(void)
{
    null_as = new_mem("null", NULL_BUF_SIZE, 1, NULL, &null_as_ops);
    null_as->mask = NULL_BUF_SIZE-1;
    null_as->flags |= AS_NOLOAD | AS_NODUMP;
}

/* -------------------------------------------------------------------------
 *  Walk the list of address spaces and call ops->sync on the ones that
 *  need it.
 * ------------------------------------------------------------------------- */
void as_sync(void)
{
    struct as *as;
    for (as = addrspaces; as; as = as->next) {
	if (as->need_sync) {
	    as_sync_op sync_op = as->ops->sync;
	    as->need_sync = false;
	    if (sync_op)
		sync_op(as);
	}
    }
}

/* -------------------------------------------------------------------------
 *  Translation map iterator
 *
 *  This derives contiguous translation maps from the as_translate
 *  operations as long as the "grain" parameter is set correctly.
 *
 *  Returns the length of the translated chunk if available, or 0 on
 *  end of map.
 * ------------------------------------------------------------------------- */
size_t as_translate_iter(const struct xlt_addr *va, struct xlt_addr *pa)
{
    struct asoffs vo = va->ao;
    size_t tlen = va->len;

    vo.offs += vo.as->base;

    while (1) {
	size_t grainsize, grainmask;
	size_t bo = vo.offs - vo.as->base;

	if (bo >= vo.as->len)
	    tlen = 0;
	else
	    tlen = min(tlen, vo.as->len - bo);

	if (!tlen)
	    return 0;		/* End of the road */

	if (!as_translate_one_level(&vo)) {
	    /* Terminal translation */
	    pa->ao  = vo;
	    pa->len = tlen;
	    return tlen;
	}

	grainsize = grain_size(vo.as->grain);
	grainmask = grainsize - 1;
	tlen = min(tlen, grainsize - (vo.offs & grainmask));
    }
}

/* XXX: add coalescion function */

/* -------------------------------------------------------------------------
 *  Initialization
 * ------------------------------------------------------------------------- */

void as_init(void)
{
    null_as_init();
}
