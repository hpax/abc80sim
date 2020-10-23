/*
 * Common definitions that we need for everything
 */

#ifndef COMPILER_H
#define COMPILER_H

#ifdef HAVE_CONFIG_H
#include "config/config.h"
#else
#error "Need compiler-specific hacks here"
#endif

/* On Microsoft platforms we support multibyte character sets in filenames */
#define _MBCS 1


/* These header files should pretty much always be included... */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include <stdatomic.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_INTRIN_H
#include <intrin.h>
#endif

#ifndef NOT_USING_SDL
#include <SDL.h>                /* This includes endian definitions */

#define WORDS_LITTLEENDIAN	(SDL_BYTEORDER == SDL_LIL_ENDIAN)
#define WORDS_BIGENDIAN		(SDL_BYTEORDER == SDL_BIG_ENDIAN)
#endif /* NOT_USING_SDL */

#ifndef __cplusplus             /* C++ has false, true, bool as keywords */
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
/* This is sort of dangerous, since casts will behave different than
   casting to the standard boolean type.  Always use !!, not (bool). */
typedef enum bool { false, true } bool;
#endif
#endif

/*
 * mempcpy() replacement
 */
#ifndef HAVE_MEMPCPY
static inline void *mempcpy(void *dest, const void *src, size_t n)
{
    memcpy(dest, src, n);
    return (char *)dest + n;
}
#endif

/*
 * asprintf()
 */
#ifndef HAVE_ASPRINTF
extern int asprintf(char **, const char *, ...);
#endif

/*
 * mode_t
 */
#ifndef HAVE_MODE_T
typedef int mode_t;
#endif

/*
 * 128-bit integers
 */
#ifdef HAVE_UINT128_T
/* Already good */
#elif defined(HAVE___UINT128_T)
typedef __uint128_t uint128_t;
# define HAVE_UINT128_T
# elif defined(HAVE___UINT128)
typedef __uint128 uint128_t;
# define HAVE_UINT128_T
#elif defined(HAVE_UNSIGNED___INT128)
typedef unsigned __int128 uint128_t;
# define HAVE_UINT128_T
#endif

/*
 * Hack to support external-linkage inline functions
 */
#ifndef HAVE_STDC_INLINE
#ifdef __GNUC__
#ifdef __GNUC_STDC_INLINE__
#define HAVE_STDC_INLINE
#else
#define HAVE_GNU_INLINE
#endif
#elif defined(__GNUC_GNU_INLINE__)
/* Some other compiler implementing only GNU inline semantics? */
#define HAVE_GNU_INLINE
#elif defined(__STDC_VERSION__)
#if __STDC_VERSION__ >= 199901L
#define HAVE_STDC_INLINE
#endif
#endif
#endif

#ifdef HAVE_STDC_INLINE
#define extern_inline inline
#elif defined(HAVE_GNU_INLINE)
#define extern_inline extern inline
#define inline_prototypes
#else
#define inline_prototypes
#endif

/*
 * Hints to the compiler that a particular branch of code is more or
 * less likely to be taken.
 */
#if HAVE___BUILTIN_EXPECT
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#else
#define likely(x)	(!!(x))
#define unlikely(x)	(!!(x))
#endif

/*
 * How to tell the compiler that a function doesn't return
 */
#ifdef HAVE_STDNORETURN_H
#include <stdnoreturn.h>
#define no_return noreturn void
#elif defined(HAVE_FUNC_ATTRIBUTE_NORETURN)
#define no_return void __attribute__((noreturn))
#elif defined(_MSC_VER)
#define no_return __declspec(noreturn) void
#else
#define no_return void
#endif

/*
 * How to tell the compiler that a function is pure arithmetic
 */
#ifdef HAVE_FUNC_ATTRIBUTE_CONST
#define const_func __attribute__((const))
#else
#define const_func
#endif

/*
 * This function has no side effects, but depends on its arguments,
 * memory pointed to by its arguments, or global variables.
 * NOTE: functions that return a value by modifying memory pointed to
 * by a pointer argument are *NOT* considered pure.
 */
#ifdef HAVE_FUNC_ATTRIBUTE_PURE
#define pure_func __attribute__((pure))
#else
#define pure_func
#endif

/* Determine probabilistically if something is a compile-time constant */
#ifdef HAVE___BUILTIN_CONSTANT_P
#define is_constant(x) __builtin_constant_p(x)
#else
#define is_constant(x) false
#endif

/* Useful construct */
#define ARRAY_SIZE(x) ((sizeof x)/(sizeof *(x)))

/* min() and max(): useful, pre-defined on Windows */
#ifndef min
# define min(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef max
# define max(x,y) ((x)>(y)?(x):(y))
#endif

/* Create a NULL pointer of the same type as the address of
   the argument, without actually evaluating said argument. */
#define nullas(p) (0 ? &(p) : NULL)

/* Convert an offsetted NULL pointer dereference to a size_t offset.
   Technically non-portable as taking the offset from a NULL pointer
   is undefined behavior, but... */
#define null_offset(p) ((size_t)((const char *)&(p) - (const char *)NULL))

/* Provide a substitute for offsetof() if we don't have one.  This
   variant works on most (but not *all*) systems... */
#ifndef offsetof
# define offsetof(t,m) null_offset(((t *)NULL)->m)
#endif

/* If typeof is defined as a macro, assume we have typeof even if
   HAVE_TYPEOF is not declared (e.g. due to not using autoconf.) */
#ifdef typeof
# define HAVE_TYPEOF 1
#endif

/* This is like offsetof(), but takes an object rather than a type. */
#ifndef offsetin
# ifdef HAVE_TYPEOF
#  define offsetin(p,m) offsetof(typeof(p),m)
# else
#  define offsetin(p,m)	null_offset(nullas(p)->m)
# endif
#endif

/* The container_of construct: if p is a pointer to member m of
   container class c, then return a pointer to the container of which
   *p is a member. */
#ifndef container_of
# define container_of(p, c, m) ((c *)((char *)(p) - offsetof(c,m)))
#endif

/* Upper half of a 64-bit multiply */
#ifdef HAVE___UMULH
static inline uint64_t umulh(uint64_t a, uint64_t b)
{
    return __umulh(a,b);
}
#elif defined(HAVE_UINT128_T)
static inline uint64_t umulh(uint64_t a, uint64_t b)
{
    return (uint64_t)(((uint128_t)a * b) >> 64);
}
#else
static inline uint64_t umulh(uint64_t a, uint64_t b)
{
    uint32_t a0 = a;
    uint32_t a1 = a >> 32;
    uint32_t b0 = b;
    uint32_t b1 = b >> 32;

    uint64_t c0 = (uint64_t)a0 * b0;
    uint64_t c1 = (uint64_t)a1 * b0;
    uint64_t c2 = (uint64_t)a0 * b1;
    uint64_t c3 = (uint64_t)a1 * b1;

    uint64_t c12, c012;

    c12 = c1 + c2;
    c3 += (c12 < c1);		/* Handle carry */

    c012 = c12 + (c0 >> 32);	/* The low 32 bits can't carry */
    c3 += (c012 >> 32) + ((c012 < c12) << 32);

    return c3;
}
#endif

#endif /* COMPILER_H */
