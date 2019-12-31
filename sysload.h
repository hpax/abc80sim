/*
 * Code to load data into a specific memory space
 */

#ifndef SYSLOAD_H
#define SYSLOAD_H

#include "compiler.h"

struct load_data;
typedef void (*load_op)(const struct load_data *ws, uint32_t addr,
			uint8_t val);

struct load_data {
    const char *name;		/* memspace name */
    const struct load_data *next;
    load_op write_op;
    void *buf;
    uint32_t mask;
    uint32_t limit;
};

void sysload_add_memspace(const char *name, load_op write_op, void *buf,
			  uint32_t mask, uint32_t limit);
int load_sysfile(const char *name);

#endif /* SYSLOAD_H */
