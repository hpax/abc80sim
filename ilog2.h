/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1996-2022 The NASM Authors - All Rights Reserved
 *   See the file AUTHORS included with the NASM distribution for
 *   the specific copyright holders.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----------------------------------------------------------------------- */

#ifndef ILOG2_H
#define ILOG2_H

#include "compiler.h"

/* Intrinsics with many names... */
#ifdef clz
/* Already defined */
#elif defined(HAVE___LZCNT32)
# define clz(x) __lzcnt32(x)
#elif defined(HAVE___LZCNT)
# define clz(x) __lzcnt(x)
#elif defined(HAVE__LZCNT_U32)
# define clz(x) _lzcnt_u32(x)
#elif defined(HAVE___LZCNT_U32)
# define clz(x) __lzcnt_u32(x)
#elif defined(HAVE___BUILTIN_CLZ)
# define clz(x) __builtin_clz(x)
#endif

#ifdef clzll
/* Already defined */
#elif defined(HAVE___LZCNT64)
# define clzll(x) __lzcnt64(x)
#elif defined(HAVE__LZCNT_U64)
# define clzll(x) _lzcnt_u64(x)
#elif defined(HAVE___LZCNT_U64)
# define clzll(x) __lzcnt_u64(x)
#elif defined(HAVE___BUILTIN_CLZLL)
# define clzll(x) __builtin_clzll(x)
#endif

#ifdef ctz
/* Already defined */
#elif defined(HAVE___TZCNT32)
# define ctz(x) __tzcnt32(x)
#elif defined(HAVE___TZCNT)
# define ctz(x) __tzcnt(x)
#elif defined(HAVE__TZCNT_U32)
# define ctz(x) _tzcnt_u32(x)
#elif defined(HAVE___TZCNT_U32)
# define ctz(x) __tzcnt_u32(x)
#elif defined(HAVE___BUILTIN_CTZ)
# define ctz(x) __builtin_ctz(x)
#endif

#ifdef ctzll
/* Already defined */
#elif defined(HAVE___TZCNT64)
# define ctzll(x) __tzcnt64(x)
#elif defined(HAVE__TZCNT_U64)
# define ctzll(x) _tzcnt_u64(x)
#elif defined(HAVE___TZCNT_U64)
# define ctzll(x) __tzcnt_u64(x)
#elif defined(HAVE___BUILTIN_CTZLL)
# define ctzll(x) __builtin_ctzll(x)
#endif

/*
 * ilog2() - basically the opposite of count leading zeroes,
 * but expect the result to be 0 for a zero input
 */
#define ILROUND(v, a, w)                                \
    do {						\
	if (v >> w) {					\
	    v >>= w;					\
	    a += w;					\
	}						\
    } while (0)

#ifdef clz

static inline unsigned int const_func ilog2_32(uint32_t v)
{
    if (!v)
        return 0;

    return clz(v) ^ 31;
}

#elif defined(__GNUC__) && defined(__x86_64__)

static inline unsigned int const_func ilog2_32(uint32_t v)
{
    unsigned int n;

    __asm__("bsrl %1,%0"
            : "=r" (n)
            : "rm" (v), "0" (0));
    return n;
}

#elif defined(__GNUC__) && defined(__i386__)

static inline unsigned int const_func ilog2_32(uint32_t v)
{
    unsigned int n;

#ifdef __i686__
    __asm__("bsrl %1,%0 ; cmovz %2,%0\n"
            : "=&r" (n)
            : "rm" (v), "r" (0));
#else
    __asm__("bsrl %1,%0 ; jnz 1f ; xorl %0,%0\n"
            "1:"
            : "=&r" (n)
            : "rm" (v));
#endif
     return n;
}

#elif defined(HAVE__BITSCANREVERSE)

static inline unsigned int const_func ilog2_32(uint32_t v)
{
    unsigned long ix;
    return _BitScanReverse(&ix, v) ? ix : 0;
}

#else

static inline unsigned int const_func ilog2_32(uint32_t v)
{
    unsigned int p = 0;

    ILROUND(v, p, 16);
    ILROUND(v, p,  8);
    ILROUND(v, p,  4);
    ILROUND(v, p,  2);
    ILROUND(v, p,  1);
    return p;
}

#endif

#ifdef clzll

static inline unsigned int const_func ilog2_64(uint64_t v)
{
    if (!v)
        return 0;

    return clzll(v) ^ 63;
}

#elif defined(__GNUC__) && defined(__x86_64__)

static inline unsigned int const_func ilog2_64(uint64_t v)
{
    uint64_t n;

    __asm__("bsrq %1,%0"
            : "=r" (n)
            : "rm" (v), "0" (UINT64_C(0)));
    return n;
}

#elif defined(HAVE__BITSCANREVERSE64)

static inline unsigned int const_func ilog2_64(uint64_t v)
{
    unsigned long ix;
    return _BitScanReverse64(&ix, v) ? ix : 0;
}

#else

static inline unsigned int const_func ilog2_64(uint64_t v)
{
    unsigned int p = 0;

    ILROUND(v, p, 32);
    return p + ilog2_32(v);
}

#endif
#undef ILROUND

/*
 * ilog2, but rounded up
 */
static inline unsigned int const_func ilog2c_32(uint32_t v)
{
    return (v < 2) ? 0 : ilog2_32(v - 1) + 1;
}
static inline unsigned int const_func ilog2c_64(uint64_t v)
{
    return (v < 2) ? 0 : ilog2_64(v - 1) + 1;
}

/* Warning: unsafe macro */
#define is_power2(v) ((v) && ((v) & ((v) - 1)) == 0)


#if SIZE_MAX == UINT32_MAX
# define ilog2_sz(v)  ilog2_32(v)
# define ilog2c_sz(v) ilog2c_32(v)
#else
# define ilog2_sz(v)  ilog2_64(v)
# define ilog2c_sz(v) ilog2c_64(v)
#endif

/*
 * Count trailing zeroes
 */

/* ((v&0)|1) produces a 1 promoted to the type of v */
#define TZROUND(v, a, w)                                \
    do {                                                \
	if (!(v & ((((v&0)|1) << w)-1))) {		\
            a  += w;                                    \
            v >>= w;                                    \
        }                                               \
    } while (0)

#ifdef ctz

static inline unsigned int const_func tzcount_32(uint32_t v)
{
    /* Not all ctz() implementations handle this correctly */
    return v ? ctz(v) : 32;
}

#elif defined(__GNUC__) && defined(__x86_64__)

static inline unsigned int const_func tzcount_32(uint32_t v)
{
    unsigned int r;

    __asm__("rep; bsfl %1,%0"
	    : "=r" (r) : "r" (v), "0" (32));
    return r;
}

#elif defined(__GNUC__) && defined(__i386__)

static inline unsigned int const_func tzcount_32(uint32_t v)
{
    unsigned int r;

    __asm__("rep; bsfl %1,%0; jnz 1f; movl $32,%0; 1:"
	    : "=r" (r) : "r" (v));
    return r;
}

#elif defined(HAVE__BITSCANFORWARD)

static inline unsigned int const_func tzcount_32(uint32_t v)
{
    unsigned long ix;
    return _BitScanForward(&ix, v) ? ix : 32;
}

#else

static inline unsigned int const_func tzcount_32(uint32_t v)
{
    unsigned int p = 1;

    TZROUND(v, p, 16);
    TZROUND(v, p,  8);
    TZROUND(v, p,  4);
    TZROUND(v, p,  2);
    TZROUND(v, p,  1);

    /* At this point v == 0 if the input was 0, otherwise 1 */

    return p - v;
}

#endif

#ifdef ctzll

static inline unsigned int const_func tzcount_64(uint64_t v)
{
    /* Not all ctz() implementations handle this correctly */
    return v ? ctzll(v) : 64;
}

#elif defined(__GNUC__) && defined(__x86_64__)

static inline unsigned int const_func tzcount_64(uint64_t v)
{
    unsigned int r;

    __asm__("rep; bsfq %1,%0"
	    : "=r" (r) : "r" (v), "0" (64));
    return r;
}

#elif defined(HAVE__BITSCANFORWARD64)

static inline unsigned int const_func tzcount_64(uint64_t v)
{
    unsigned long ix;
    return _BitScanForward64(&ix, v) ? ix : 64;
}

#else

static inline unsigned int const_func tzcount_64(uint64_t v)
{
    unsigned int p = 0;

    TZROUND(v, p, 32);
    return p + tzcount_32(v);
}

#endif
#undef TZROUND

#endif /* ILOG2_H */
