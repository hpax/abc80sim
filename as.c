#include "compiler.h"
#include "z80.h"
#include "as.h"
#include "debug.h"

/* -------------------------------------------------------------------------
 *  List of all registered address spaces
 * ------------------------------------------------------------------------- */
struct as *null_as;
static struct as *addrspaces;

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
struct as *get_addrspace(const char *name, size_t len)
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
struct as *new_mem(const char *name, const struct as_ops *ops,
		   size_t len, unsigned int nmaps)
{
    struct as *as = as_new_space(name, ops, len, nmaps);
    void *p = calloc(nmaps, len);

    if (!p)
	return NULL;

    as->p.data = p;
    return as;
}

static struct as_data mem_as_dump(struct as *as, size_t offs)
{
    struct as_data asd;

    asd.data = as->p.data + offs;
    asd.len  = as->len - offs;

    return asd;
}

/* -------------------------------------------------------------------------
 *  Specializations of the memory buffer address spaces: ROM and RAM
 * ------------------------------------------------------------------------- */
static void rom_as_write(struct as *as, size_t offs, uint8_t v)
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

struct as *new_ram(const char *name, size_t len, unsigned int nmaps)
{
    return new_mem(name, &ram_as_ops, len, nmaps);
}
struct as *new_rom(const char *name, size_t len, unsigned int nmaps)
{
    struct as *as = new_mem(name, &rom_as_ops, len, nmaps);
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
 *  Null address space (empty bus)
 * ------------------------------------------------------------------------- */
static void null_as_load(struct as *as, size_t offs, uint8_t v)
{
    (void)as;
    (void)offs;
    (void)v;
}

const struct as_ops null_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .dump  = mem_as_dump,
    .load  = null_as_load,	/* Drop attempts at loading on the floor */
    .sync  = NULL		/* No syncing */
};

#define NULL_BUF_SIZE	4096	/* Must be a power of 2 */

static void null_as_init(void)
{
    null_as = new_mem("null", &null_as_ops, NULL_BUF_SIZE, 1);
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
 *  Returns true if a new translation is available, otherwise false;
 *  in the latter case va->len will contain a nonzero value if *va
 *  contains a final translation.
 * ------------------------------------------------------------------------- */
bool as_translate_iter(struct xlt_addr *va, struct xlt_addr *pa)
{
    struct asoffs pao;
    size_t grainsize, grainmask;

    if (va->ao.offs >= va->ao.as->len)
	va->len = 0;
    else if (va->ao.offs + va->len > va->ao.as->len)
	va->len = va->ao.as->len - va->ao.offs;

    if (!va->len || !va->ao.as->translate)
	return false;		/* All done */

    grainsize = grain_size(va->ao.as->grain);
    grainmask = grainsize - 1;

    pa->ao = pao = va->ao.as->translate(va->ao);
    pa->len = 0;

    do {
	size_t tlen = min(va->len, grainsize - (va->ao.offs & grainmask));

	pa->len += tlen;
	va->len += tlen;
	va->ao.offs += tlen;
	va->len -= tlen;

	if (!va->len)
	    break;

	pao = va->ao.as->translate(va->ao);
    } while (pao.as == pa->ao.as && pao.offs == pa->ao.offs + pa->len);

    return true;
}

/* -------------------------------------------------------------------------
 *  Initialization
 * ------------------------------------------------------------------------- */

void as_init(void)
{
    null_as_init();
}
