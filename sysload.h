/*
 * Code to load data into a specific memory space
 */

#ifndef SYSLOAD_H
#define SYSLOAD_H

#include "compiler.h"

struct load_data;

/*
 * Note: if a load_op and a dump_op are provided, then "buf" is just
 * a private pointer for the operations to use.
 */

/* Memory space write operation (if not just memcpy) */
typedef void
(*load_op)(void *buf, uint32_t addr, uint8_t val);

/* Memory space read operation (if not just memcpy) */
struct dump_data {
    const void *data;
    size_t len;
};

typedef struct dump_data
(*dump_op)(void *buf, uint32_t addr);

void sysload_add_memspace(const char *name, load_op write_op, dump_op read_op,
			  void *buf, uint32_t mask, uint32_t limit);
int load_sysfile(const char *name);
void dump_memory(const char *name);

#endif /* SYSLOAD_H */
