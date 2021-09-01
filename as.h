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

/* Select one of possibly several maps */
typedef void (*as_map_op)(struct as *as, unsigned int map);

/*
 * Bulk memory space preload operation (if not just memcpy).  This
 * will typically differ from as_write_op in that it doesn't need to
 * trigger side effects until as_sync_op is called, nor should it
 * typically enforce e.g. ROM write protect (we probably want to be
 * able to load ROM contents.) Furthermore, if appropriate for the
 * address space it might be desirable to cross all possible maps,
 * e.g. for a paged ROM.
 */
typedef void (*as_load_op)(struct as *as, unsigned int map,
			   size_t offs, uint8_t val);

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
typedef struct as_data (*as_dump_op)(struct as *as, unsigned int map, size_t offs);

struct as_ops {
    as_read_op read;
    as_write_op write;
    as_map_op map;
    as_dump_op dump;
    as_load_op load;
    as_sync_op sync;
};

/* Definition of an address space */
struct as {
    /* The hottest items... */
    const struct as_ops *ops;
    void *p;			/* Data buffer *for the current map* */
    size_t mask;		/* Address mask (optional) */

    unsigned int flags;	        /* Flags for the drivers */
    unsigned int grain;		/* Granularity of page tables if applicable */

    unsigned int map;		/* Current map number */
    unsigned int nmaps;		/* Total maps */

    void *base;			/* Base buffer */
    size_t len;		        /* Size of the namespace per map */

    const char *name;		/* Address space name */
    const char *dump_name;	/* name when dumping to a file */

    struct as *next;	        /* Linked list of known address spaces */
};

#define AS_NOLOAD	1	/* Do not load data into this namespace */
#define AS_NODUMP	2	/* Do not dump this namespace by itself */
#define AS_DUMP_ONE	4	/* Only dump one (current) map */

/*
 * Inline functions for methods with fast-out defaults
 */
static inline uint8_t do_as_read(struct as *as, size_t offs)
{
    as_read_op read_op = as->ops->read;

    offs &= as->mask;

    if (!read_op) {
	const uint8_t *p = as->p;
	return p[offs];
    } else {
	return read_op(as, offs);
    }
}

static inline void do_as_write(struct as *as, size_t offs, uint8_t v)
{
    as_write_op write_op = as->ops->write;

    offs &= as->mask;

    if (!write_op) {
	uint8_t *p = as->p;
	p[offs & as->mask] = v;
    } else {
	write_op(as, offs, v);
    }
}

static inline void do_as_map(struct as *as, unsigned int map)
{
    as_map_op map_op = as->ops->map;

    assert(map < as->nmaps);
    if (map_op)
	map_op(as, map);
    as->map = map;
}

static inline struct as_data
do_as_dump(struct as *as, unsigned int map, size_t offs)
{
    as_dump_op dump_op = as->ops->dump;

    offs &= as->mask;

    if (!dump_op) {
	struct as_data asd;
	asd.data = NULL;
	asd.len  = 0;
	return asd;
    } else {
	return dump_op(as, map, offs);
    }
}

static inline void do_as_load(struct as *as, unsigned int map,
			      size_t offs, uint8_t v)
{
    as_load_op load_op = as->ops->load;

    offs &= as->mask;

    if (!load_op) {
	uint8_t *p = as->base;
	p += map * as->len;
	p[offs] = v;
    } else {
	return load_op(as, map, offs, v);
    }
}


static inline void do_as_sync(struct as *as)
{
    as_sync_op sync_op = as->ops->sync;

    if (sync_op)
	return sync_op(as);
}

/*
 * The top-level address space for the CPU
 */
extern struct as *cpu_as;

/*
 * Find an address space by name
 */
struct as *get_addrspace(const char *name, size_t len);

/*
 * Constructors etc.
 */
struct as *as_new_space(const char *name, const struct as_ops *ops,
			size_t len, unsigned int nmaps);
struct as *new_mem(const char *name, const struct as_ops *ops,
		   size_t len, unsigned int nmaps);
struct as *new_ram(const char *name, size_t len, unsigned int nmaps);
struct as *new_rom(const char *name, size_t len, unsigned int nmaps);

struct as *as_new_pagespace(const char *name, size_t len,
			    unsigned int nmaps, unsigned int grain);
void as_set_pages(struct as *vas, size_t voffs, unsigned int map,
		  struct as *pas, size_t poffs, size_t len);

void as_init(void);

#endif /* AS_H */
