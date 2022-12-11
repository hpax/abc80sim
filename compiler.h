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

#define PLAIN_WIN32 defined(_WIN32) && !defined(__CYGWIN__)

#ifdef _WIN32

#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0600	/* Windows Vista (needed for WSAPoll) */
#endif

#ifdef HAVE_WINSOCK2_H
/* Must be included before windows.h */
# include <winsock2.h>
#endif
#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif

#endif /* _WIN32 */

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
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
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
#ifdef HAVE_SHARE_H
#include <share.h>
#endif
#ifdef HAVE_PROCESS_H
#include <process.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#ifdef HAVE_INTRIN_H
#include <intrin.h>
#endif
#ifdef HAVE_IMMINTRIN_H
#include <immintrin.h>
#endif
#ifdef HAVE_CPUID_H
#include <cpuid.h>
#endif

#ifndef NOT_USING_SDL
#include <SDL.h>                /* This includes endian definitions */

#define WORDS_LITTLEENDIAN	(SDL_BYTEORDER == SDL_LIL_ENDIAN)
#define WORDS_BIGENDIAN		(SDL_BYTEORDER == SDL_BIG_ENDIAN)
#endif /* NOT_USING_SDL */

#ifndef __cplusplus             /* C++ has false, true, bool as keywords */
# ifdef HAVE_STDBOOL_H
#  include <stdbool.h>
# elif defined(HAVE__BOOL)
#  define bool _Bool
#  define false 0
#  define true 1
# else
/* This is sort of dangerous, since casts will behave different than
   casting to the standard boolean type.  Always use !!, not (bool). */
typedef enum bool { false, true } bool;
# endif
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
 * localtime_r
 */
#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *, struct tm *);
#endif

/*
 * mode_t, speed_t, socklen_t
 */
#ifndef HAVE_MODE_T
typedef int mode_t;
#endif
#ifndef HAVE_SSIZE_T
# ifdef HAVE_PTRDIFF_T
typedef ptrdiff_t ssize_t;
# else
typedef int ssize_t;
# endif
#endif
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/*
 * O_CLOEXEC (if available)
 */
#ifndef O_CLOEXEC
# ifdef _O_NOINHERIT
#  define O_CLOEXEC _O_NOINHERIT
# else
#  define O_CLOEXEC 0
# endif
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

/*
 * This is a printf()-type function
 */
#ifdef HAVE_FUNC_ATTRIBUTE3_FORMAT
#define printf_func(fi,ai) __attribute__((__format__ (__printf__,fi,ai)))
#define vprintf_func(fi)   __attribute__((__format__ (__printf__,fi,0)))
#else
#define printf_func(fi,ai)
#define vprintf_func(fi)
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

/* Handy macro for comparing a partial string against a string constant */
#define isstr(str,ptr,len) \
  ((size_t)(len) == sizeof(str)-1 && !memcmp(str, (ptr), sizeof(str)-1))

#endif /* COMPILER_H */
