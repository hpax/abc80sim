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

#include "autoconf/attribute.h"

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
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#ifdef HAVE_STDBIT_H

/* C23 */
#include <stdbit.h>
#define WORDS_LITTLEENDIAN	(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__)
#define WORDS_BIGENDIAN		(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__)

#elif defined(_WIN32)

/* Windows is always littleendian */
#define WORDS_LITTLEENDIAN	1
#define WORDS_BIGENDIAN		0

#else

#ifdef HAVE_ENDIAN_H
#include <endian.h>		/* POSIX */
#elif defined(HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>	/* Some BSDs */
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>		/* Some other BSDs */
#endif

#ifdef BYTE_ORDER
#define WORDS_LITTLEENDIAN	(BYTE_ORDER == LITTLE_ENDIAN)
#define WORDS_BIGENDIAN		(BYTE_ORDER == BIG_ENDIAN)
#elif defined(__BYTE_ORDER__)
/* gcc et al */
#define WORDS_LITTLEENDIAN	(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define WORDS_BIGENDIAN		(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif !defined(NOT_USING_SDL)

#include <SDL3/SDL.h>		/* Last resort... */

#define WORDS_LITTLEENDIAN	(SDL_BYTEORDER == SDL_LIL_ENDIAN)
#define WORDS_BIGENDIAN		(SDL_BYTEORDER == SDL_BIG_ENDIAN)

#endif
#endif

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
 * fnmatch()
 */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#else
# include "clib/fnmatch.h"
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
#define no_return void ATTRIBUTE(noreturn)
#elif defined(_MSC_VER)
#define no_return __declspec(noreturn) void
#else
#define no_return void
#endif

/*
 * A fatal function is both unlikely and no_return
 */
#define fatal_func     no_return unlikely_func
#define fatal_func_ptr no_return unlikely_func_ptr

/*
 * How to tell the compiler that a function is pure arithmetic
 */
#ifdef HAVE_FUNC_ATTRIBUTE_CONST
#define const_func ATTRIBUTE(const)
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
#define pure_func ATTRIBUTE(pure)
#else
#define pure_func
#endif

/*
 * This is a printf()-type function
 */
#ifdef HAVE_FUNC_ATTRIBUTE3_FORMAT
#define printf_func(fi,ai) ATTRIBUTE(__format__ (__printf__,fi,ai))
#define vprintf_func(fi)   ATTRIBUTE(__format__ (__printf__,fi,0))
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

/*
 * Useful array-handling constructs. Don't know how to do these without
 * typeof() or auto/__auto_type; fortunately it is supported by all major
 * compilers these days.
 */
#define ARRAY_SIZE(x)	((sizeof x)/(sizeof *(x)))
#define ARRAY_END(x)	(&(x)[ARRAY_SIZE(x)])
#define ARRAY_FOREACH(v,a) \
    for (typeof(*(a)) v = (a); v < ARRAY_END(a); v++)

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

/* Flagging that a loop should not be unrolled */
#define NO_UNROLL				\
  _Pragma("GCC unroll 0")			\
  _Pragma("clang loop unroll(disable)")

#endif /* COMPILER_H */
