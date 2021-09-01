#include "compiler.h"
#include "z80.h"
#include "as.h"
#include "debug.h"

/* -------------------------------------------------------------------------
 *  List of all registered address spaces
 * ------------------------------------------------------------------------- */
static struct as *null_as;
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

    /* Add to linked list */
    as->next   = addrspaces;
    addrspaces = as;

    return as;
}

/* The name, len pair allows for a separator other than \0 */
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

    as->p = as->base = p;
    return as;
}

static void mem_as_map(struct as *as, unsigned int map)
{
    as->p = (uint8_t *)as->base + (map * as->len);
}

static struct as_data mem_as_dump(struct as *as, unsigned int map, size_t offs)
{
    struct as_data asd;

    assert(map < as->nmaps);

    offs &= as->mask;
    asd.data = (const uint8_t *)as->base + (map * as->len) + offs;
    asd.len  = as->len - (offs % as->len);

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
    .map   = mem_as_map,
    .dump  = mem_as_dump,
    .load  = NULL,		/* Just load it */
    .sync  = NULL		/* No syncing */
};

const struct as_ops rom_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .map   = mem_as_map,
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
    return new_mem(name, &rom_as_ops, len, nmaps);
}

/* -------------------------------------------------------------------------
 *  Paged memory space
 * ------------------------------------------------------------------------- */
struct page {
    struct as *as;
    size_t offs;
};

static inline struct page *page_mapbase(struct as *as, unsigned int map)
{
    return (struct page *)as->base + ((map * as->len) >> as->grain);
}
static inline size_t page_size(unsigned int grain)
{
    return (size_t)1 << grain;
}
static inline size_t page_mask(unsigned int grain)
{
    return page_size(grain)-1;
}

static uint8_t page_as_read(struct as *as, size_t offs)
{
    struct page *page = as->p;

    offs &= as->mask;
    page += offs >> as->grain;
    offs = page->offs + (offs & page_mask(as->grain));

    return do_as_read(page->as, offs);
}

static void page_as_write(struct as *as, size_t offs, uint8_t v)
{
    struct page *page = as->p;

    offs &= as->mask;
    page += offs >> as->grain;
    offs = page->offs + (offs & page_mask(as->grain));

    do_as_write(page->as, offs, v);
}

static void page_as_map(struct as *as, unsigned int map)
{
    as->p = (struct page *)as->base + ((map * as->len) >> as->grain);
}

static struct as_data page_as_dump(struct as *as, unsigned int map, size_t offs)
{
    struct as_data asd;
    struct page *page;
    size_t maxbytes;

    offs &= as->mask;
    page = page_mapbase(as, map) + (offs >> as->grain);
    offs = page->offs + (offs & page_mask(as->grain));

    maxbytes = page_size(as->grain) - offs;

    asd = page->as->ops->dump(page->as, page->as->map, offs);
    if (asd.len > maxbytes)
	asd.len = maxbytes;

    return asd;
}

static void page_as_load(struct as *as, unsigned int map, size_t offs, uint8_t v)
{
    struct page *page;

    offs &= as->mask;
    page = page_mapbase(as, map) + (offs >> as->grain);
    offs = page->offs + (offs & page_mask(as->grain));

    do_as_load(as, as->map, offs, v);
}

static const struct as_ops page_as_ops = {
    .read  = page_as_read,
    .write = page_as_write,
    .map   = page_as_map,
    .dump  = page_as_dump,
    .load  = page_as_load,
    .sync  = NULL
};

void as_set_pages(struct as *vas, size_t voffs, unsigned int map,
		  struct as *pas, size_t poffs, size_t len)
{
    unsigned int pages;
    const size_t psize = page_size(vas->grain);
    struct page *p = vas->p;

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
    const size_t psize = (size_t)1 << grain;
    struct page *p;
    unsigned int i;
    struct as *as = as_new_space(name, &page_as_ops, len, nmaps);

    if (!as)
	return NULL;

    /* Granularity of the page tables as a power of 2 */
    as->grain = grain;
    assert((len & (psize - 1)) == 0);

    p = calloc(nmaps * (len >> grain), sizeof *p);
    if (!p)
	return NULL;

    as->p = as->base = p;
    as->flags |= AS_DUMP_ONE;

    /* Initialize all pages to point to the null address space */
    for (i = 0; i < nmaps; i++) {
	as_set_pages(as, 0, i, null_as, 0, len);
    }

    return as;
}

/* -------------------------------------------------------------------------
 *  Null address space (empty bus)
 * ------------------------------------------------------------------------- */
static void null_as_load(struct as *as, unsigned int map, size_t offs, uint8_t v)
{
    (void)as;
    (void)map;
    (void)offs;
    (void)v;
}

const struct as_ops null_as_ops = {
    .read  = NULL,		/* Just read it */
    .write = rom_as_write,	/* Drop write on floor */
    .map   = mem_as_map,
    .dump  = mem_as_dump,
    .load  = null_as_load,	/* Drop attempts at loading on the floor */
    .sync  = NULL		/* No syncing */
};

#define NULL_BUF_SIZE	4096	/* Must be a power of 2 */

static void null_as_init(void)
{
    null_as = new_mem("null", &null_as_ops, NULL_BUF_SIZE, 1);
    memset(null_as->p, 0xff, NULL_BUF_SIZE);
    null_as->mask = NULL_BUF_SIZE-1;
    null_as->flags |= AS_NOLOAD | AS_NODUMP;
}

/* -------------------------------------------------------------------------
 *  Initialization
 * ------------------------------------------------------------------------- */

void as_init(void)
{
    null_as_init();
}
