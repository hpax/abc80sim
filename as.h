/*
 * Unified address space definition
 */

#ifndef AS_H
#define AS_H

#include "compiler.h"

struct as;			/* Defined below */

/* CPU read operation from this memory space */
typedef uint8_t (*as_read_op)(struct as *as, size_t offs);

/* CPU write operation to this memory space */
typedef void (*as_write_op)(struct as *as, size_t offs, uint8_t val);

/*
 * Bulk memory space preload operation (if not just memcpy).  This
 * will typically differ from as_write_op in that it doesn't need to
 * trigger side effects until as_sync_op is called, nor should it
 * typically enforce e.g. ROM write protect (we probably want to be
 * able to load ROM contents.) Furthermore, if appropriate for the
 * address space it might be desirable to cross all possible maps,
 * e.g. for a paged ROM.
 */
typedef void (*as_load_op)(struct as *as, size_t offs, uint8_t val);

/* Execute after bulk preload to this memory space */
typedef void (*as_sync_op)(struct as *as);

/*
 * Memory space data dump (if not just fwrite). Returns a pointer
 * available data and its length.
 */
struct as_data {
    const void *data;
    size_t len;
};
typedef struct as_data (*as_dump_op)(struct as *as, size_t offs);

struct as_ops {
    as_read_op read;
    as_write_op write;
    as_dump_op dump;
    as_write_op load;
    as_sync_op sync;
};

/* (address space, offset) pair */
struct asoffs {
    struct as *as;
    size_t offs;
};
typedef struct asoffs (*as_translate_op)(struct asoffs aso);

#define MAX_GRAIN ((sizeof(size_t))*CHAR_BIT - 1)

static inline size_t grain_size(unsigned int grain)
{
    return (size_t)1 << grain;
}
static inline size_t grain_mask(unsigned int grain)
{
    return grain_size(grain) - 1;
}

/* Definition of an address space */
struct as {
    /* The hottest items... */
    as_translate_op translate;
    const struct as_ops *ops;
    union {
	uint8_t *data;
	struct asoffs *page;
	struct as *parent_as;
    } p;
    size_t mask;		/* Address mask */
    size_t base;		/* Base offset within this address space */
    size_t len;		        /* Size of the namespace, per map */

    unsigned int flags;	        /* Flags for the drivers */
    unsigned int grain;		/* Granularity of address translation */

    unsigned int map;		/* Current map number */
    unsigned int nmaps;		/* Total maps */

    const char *name;		/* Address space name */
    const char *dump_name;	/* name when dumping to a file */

    struct as *next;	        /* Linked list of known address spaces */
};

#define AS_NOLOAD	1	/* Do not load data into this namespace */
#define AS_NODUMP	2	/* Do not dump this namespace by itself */
#define AS_ONE_MAP	4	/* Load or dump only one (current) map */
#define AS_ALIAS	8	/* It is an alias map */

/*
 * Wrapper functions for methods
 */

static inline struct asoffs do_translate_addr(struct as *as, size_t offs)
{
    struct asoffs aso;

    aso.as   = as;
    aso.offs = offs;

    while (1) {
	aso.offs = (aso.offs & aso.as->mask) + aso.as->base;
	if (aso.as->translate)
	    aso = aso.as->translate(aso);
	else if (!aso.as->ops)
	    aso.as = aso.as->p.parent_as;
	else
	    return aso;		/* Found a "real" address space */
    }
}

static inline uint8_t do_as_read(struct as *as, size_t offs)
{
    struct asoffs aso = do_translate_addr(as, offs);
    as_read_op read_op = aso.as->ops->read;

    if (!read_op) {
	return aso.as->p.data[aso.offs];
    } else {
	return read_op(aso.as, aso.offs);
    }
}

static inline void do_as_write(struct as *as, size_t offs, uint8_t v)
{
    struct asoffs aso = do_translate_addr(as, offs);
    as_write_op write_op = aso.as->ops->write;

    if (!write_op) {
	aso.as->p.data[aso.offs] = v;
    } else {
	write_op(aso.as, aso.offs, v);
    }
}

static inline struct as_data do_as_dump(struct as *as, size_t offs)
{
    struct asoffs aso = do_translate_addr(as, offs);
    as_dump_op dump_op = aso.as->ops->dump;

    if (!dump_op) {
	struct as_data asd;

	asd.data = NULL;
	asd.len  = 0;
	return asd;
    } else {
	return dump_op(as, offs);
    }
}

static inline void do_as_load(struct as *as, size_t offs, uint8_t v)
{
    struct asoffs aso = do_translate_addr(as, offs);
    as_write_op load_op = aso.as->ops->load;

    if (!load_op) {
	aso.as->p.data[aso.offs] = v;
    } else {
	load_op(aso.as, aso.offs, v);
    }
}

static inline void do_as_sync(struct as *as)
{
    as_sync_op sync_op = as->ops->sync;

    if (sync_op)
	return sync_op(as);
}

/*
 * Dummy address space
 */
extern struct as *null_as;

/*
 * The top-level address space for the CPU
 */
extern struct as *cpu_as;

/*
 * Canned as_ops and as_ops members
 */
struct as_data mem_as_dump(struct as *as, size_t offs);
void rom_as_write(struct as *as, size_t offs, uint8_t v);

extern const struct as_ops rom_as_ops;
extern const struct as_ops ram_as_ops;
extern const struct as_ops null_as_ops;

/*
 * Find an address space by name
 */
struct as *get_addrspace(const char *name, size_t len);

/*
 * Choose a specific map/bank in an address space
 */
static inline void as_set_map(struct as *as, unsigned int map)
{
    assert(map < as->nmaps);
    as->map  = map;
    as->base = as->len * map;
}

/*
 * Iterator for memory translations
 */
struct xlt_addr {
    struct asoffs ao;		/* Address space:offset */
    size_t len;			/* Length of contiguous translation */
};
bool as_translate_iter(struct xlt_addr *va, struct xlt_addr *pa);

/*
 * Memory spaces
 */
struct as *as_new_space(const char *name, const struct as_ops *ops,
			size_t len, unsigned int nmaps);
struct as *new_mem(const char *name, size_t len, unsigned int nmaps,
		   void *buf, const struct as_ops *ops);
struct as *new_ram(const char *name, size_t len, unsigned int nmaps, void *buf);
struct as *new_rom(const char *name, size_t len, unsigned int nmaps, void *buf);

/*
 * Page tables
 */
struct as *as_new_pagespace(const char *name, size_t len,
			    unsigned int nmaps, unsigned int grain);
void as_set_pages(struct as *vas, size_t voffs, unsigned int map,
		  struct as *pas, size_t poffs, size_t len);

/*
 *  Alias address space
 */
struct as *as_new_aliasspace(const char *name, size_t len);
static inline void
as_point_alias(struct as *alias_as, struct as *parent_as, size_t offs)
{
    alias_as->p.parent_as = parent_as;
    alias_as->base = offs;
}

/*
 * Initialization
 */
void as_init(void);

#endif /* AS_H */
