/*
 * Code to load data into a specific memory space
 */

#ifndef SYSLOAD_H
#define SYSLOAD_H

#include "compiler.h"
#include "as.h"

int load_sysfile(const char *name);
void dump_memory(const char *name);

#endif /* SYSLOAD_H */
