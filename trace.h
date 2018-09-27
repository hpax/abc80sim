#ifndef TRACE_H
#define TRACE_H

#include "compiler.h"

extern FILE *tracef;

enum tracing {
    TRACE_NONE  = 0x00,
    TRACE_CPU	= 0x01,
    TRACE_IO	= 0x02,
    TRACE_DISK	= 0x04,
    TRACE_CAS	= 0x08,
    TRACE_PR	= 0x10,
    TRACE_ALL   = 0x1f
};

extern enum tracing tracing;

#endif /* TRACE_H */
