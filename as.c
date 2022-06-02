#include "compiler.h"
#include "z80.h"
#include "as.h"
#include "debug.h"
#include "ilog2.h"

/* -------------------------------------------------------------------------
 *  List of all registered address spaces
 * ------------------------------------------------------------------------- */
struct as *null_as;
struct as *addrspaces;

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

/* -------------------------------------------------------------------------
 *  Address space backed by a memory buffer
 * ------------------------------------------------------------------------- */
struct as *new_mem(const char *name, size_t len, unsigned int nmaps,
		   void *buf, const struct as_ops *ops)
{
    struct as *as = as_new_space(name, ops, len, nmaps);

    if (!buf) {
	buf = calloc(nmaps, len);
	if (!buf)
	    return NULL;
    }

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
    .sync  = NULL		/* No syncing */
};

const struct as_ops rom_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .dump  = mem_as_dump,
    .load  = NULL,		/* Just load it */
    .sync  = NULL		/* No syncing */
};

struct as *new_ram(const char *name, size_t len, unsigned int nmaps, void *buf)
{
    return new_mem(name, len, nmaps, buf, &ram_as_ops);
}
struct as *new_rom(const char *name, size_t len, unsigned int nmaps, void *buf)
{
    struct as *as = new_mem(name, len, nmaps, buf, &rom_as_ops);
    if (!buf)
	memset(as->p.data, 0xff, len * nmaps);
    return as;
}

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

/* -------------------------------------------------------------------------
 *  Null address space (empty bus)
 * ------------------------------------------------------------------------- */
const struct as_ops null_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .dump  = mem_as_dump,
    .load  = rom_as_write,	/* Drop attempts at loading on the floor */
    .sync  = NULL		/* No syncing */
};

#define NULL_BUF_SIZE	4096	/* Must be a power of 2 */

static void null_as_init(void)
{
    null_as = new_mem("null", NULL_BUF_SIZE, 1, NULL, &null_as_ops);
    memset(null_as->p.data, 0xff, NULL_BUF_SIZE);
    null_as->mask = NULL_BUF_SIZE-1;
    null_as->flags |= AS_NOLOAD | AS_NODUMP;
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
