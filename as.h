/*
 * Unified address space definition
 */

#ifndef AS_H
#define AS_H

#include "compiler.h"
#include "trace.h"

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
    int init;
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

enum as_flags {
    AS_NOLOAD	  =  1,	/* Do not load data into this namespace */
    AS_NODUMP	  =  2,	/* Do not dump this namespace by itself */
    AS_ONE_MAP	  =  4,	/* Load or dump only one (current) map */
    AS_NODUMP_ALL =  8,	/* Don't dump as part of an "all" dump */
    AS_ALIAS	  = 16	/* It is an alias map */
};

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
    size_t len;			/* Size of the namespace, per map */

    enum as_flags flags;        /* Flags for the drivers */
    unsigned int grain;		/* Granularity of address translation */

    unsigned int map;		/* Current map number */
    unsigned int nmaps;		/* Total maps */

    bool need_sync;		/* do_as_load() executed on this ns */

    const char *name;		/* Address space name */
    const char *dump_name;	/* name when dumping to a file */

    struct as *next;		/* Linked list of known address spaces */
};

/*
 * Wrapper functions for methods
 */
static inline bool as_translate_one_level(struct asoffs *aso)
{
    aso->offs = (aso->offs & aso->as->mask) + aso->as->base;

    if (aso->as->translate) {
	/* It has a translation function */
	*aso = aso->as->translate(*aso);
	return true;
    } else if (!aso->as->ops) {
	/* It is an alias (note: offset already applied) */
	aso->as = aso->as->p.parent_as;
	return true;
    } else {
	return false;		/* Terminal translation */
    }
}

static inline struct asoffs do_translate_addr(struct as *as, size_t offs)
{
    struct asoffs aso;

    aso.as   = as;
    aso.offs = offs;

    /* Descent translations until complete */
    while (as_translate_one_level(&aso))
	;			/* Keep iterating */

    return aso;
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

/* do_as_dump() takes an ALREADY TRANSLATED ADDRESS */
static inline struct as_data do_as_dump(struct as *as, size_t offs)
{
    as_dump_op dump_op = as->ops->dump;

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

    aso.as->need_sync = true;
    if (!load_op) {
	aso.as->p.data[aso.offs] = v;
    } else {
	load_op(aso.as, aso.offs, v);
    }
}

/* Synchronize side effects after do_as_load() */
void as_sync(void);

/*
 * Dummy address space
 */
extern struct as *null_as;

/*
 * The top-level address space for the CPU
 */
extern struct as *cpu_as;

/*
 * List of all address spaces
 */
extern struct as *addrspaces;

/*
 * Canned as_ops and as_ops members
 */
pure_func struct as_data mem_as_dump(struct as *as, size_t offs);
void rom_as_write(struct as *as, size_t offs, uint8_t v);

extern const struct as_ops rom_as_ops;
extern const struct as_ops ram_as_ops;
extern const struct as_ops null_as_ops;

/*
 * Find an address space by name
 */
pure_func struct as *get_addrspace(const char *name, size_t len);

/*
 * Choose a specific map/bank in an address space
 */
static inline void as_set_map(struct as *as, unsigned int map)
{
    if (map != as->map) {
	assert(map < as->nmaps);
	if (tracing(TRACE_MAP))
	    fprintf(tracef, "MAP: as %s map %u (was %u)\n",
		    as->name, map, as->map);
	as->map  = map;
	as->base = as->len * map;
    }
}

/*
 * Iterator for memory translations
 */
struct xlt_addr {
    struct asoffs ao;		/* Address space:offset */
    size_t len;			/* Length of contiguous translation */
};
size_t as_translate_iter(const struct xlt_addr *va, struct xlt_addr *pa);

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
 * Buffer initialization (without attaching to an as)
 */
void mem_buf_init(void *buf, size_t bytes, int init);

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
void as_point_alias(struct as *alias_as, struct as *parent_as, size_t offs);
struct as *as_alias(const char *name, size_t len, struct as *parent_as,
		    size_t offs, enum as_flags flags);

/*
 * Initialization
 */
void as_init(void);
void config_init_ram(const char *arg);

#endif /* AS_H */
