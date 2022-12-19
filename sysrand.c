#include "compiler.h"
#include "nstime.h"
#include "random.h"

#pragma GCC optimize("no-unroll-loops")

static size_t cpu_init_get_random_bytes(void *, size_t);
static size_t os_init_get_random_bytes(void *, size_t);

static size_t (*cpu_get_random_bytes)(void *, size_t) = cpu_init_get_random_bytes;
static size_t (*os_get_random_bytes)(void *, size_t) = os_init_get_random_bytes;

static size_t fail_get_random_bytes(void *ptr, size_t len)
{
    (void)ptr; (void)len;
    return 0;
}

#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>

/* read() with retry and timeout */
#define READ_TIMEOUT 50	/* unit: ms */

static inline int ns_to_ms(int64_t ns)
{
    if (ns <= 0)
	return 0;		/* Watch for wraparound */
    else
	return (ns + 999999) / 1000000;
}

static size_t toread(int fd, void *buf, size_t count)
{
    char *b = (char *)buf;
    int n = 0;
    int rv;
    struct pollfd pfd[1];
    const uint64_t timeout_ns = nstime() + ((uint64_t)READ_TIMEOUT*1000000);
    uint64_t now;

    while (count) {
	pfd[0].fd = fd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	now = nstime();

	rv = poll(pfd, 1, ns_to_ms(timeout_ns - now));
	if (rv == -1 && errno == EINTR)
	    continue;
	else if (rv <= 0 || !(pfd[0].revents & POLLIN))
	    break;

	rv = read(fd, b, count);
	if (rv == -1 && (errno == EINTR || errno == EAGAIN))
	    continue;
	else if (rv <= 0)
	    break;

	count -= rv;
	n += rv;
	b += rv;
    }
    return n;
}

static int randfd = -1;

static void close_randfd(void)
{
    if (randfd >= 0)
	close(randfd);
}

static size_t unix_get_random_bytes(void *ptr, size_t len)
{
    return toread(randfd, ptr, len);
}

static size_t os_init_get_random_bytes(void *ptr, size_t len)
{
    /* Look for /dev/urandom or /dev/random */

    randfd = open("/dev/urandom", O_RDONLY);
    if (randfd < 0) {
	randfd = open("/dev/random", O_RDONLY);
	if (randfd < 0) {
	    os_get_random_bytes = fail_get_random_bytes;
	    return 0;
	}
    }

    atexit(close_randfd);
    os_get_random_bytes = unix_get_random_bytes;
    return unix_get_random_bytes(ptr, len);
}

#elif defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>

static HCRYPTPROV hProv;

static void close_provider(void)
{
    CryptReleaseContext(hProv, 0);
}

static size_t win_get_random_bytes(void *ptr, size_t len)
{
    return CryptGenRandom(hProv, len, ptr) ? len : 0;
}

static size_t os_init_get_random_bytes(void *ptr, size_t len)
{
    static const DWORD providers[] = {
	PROV_RNG, PROV_INTEL_SEC, PROV_RSA_SIG, PROV_RSA_FULL,
	PROV_DSS, PROV_SSL, 0
    };
    const DWORD *pp = providers;

    os_get_random_bytes = fail_get_random_bytes;

    while (*pp) {
	if (CryptAcquireContext(&hProv, NULL, NULL, *pp,
				CRYPT_SILENT | CRYPT_VERIFYCONTEXT)) {
	    atexit(close_provider);

	    os_get_random_bytes = win_get_random_bytes;
	    break;
	}
	pp++;
    }

    return os_get_random_bytes(ptr, len);
}

#else

/* Need system-specific code here */

static size_t os_init_get_random_bytes(void *ptr, size_t len)
{
    return fail_get_random_bytes(ptr, len);
}

#endif

/* Need at least these basic intrinsics */
#if defined(HAVE___CPUIDEX) && defined(HAVE__RDRAND32_STEP)

#if (defined(__i386__) && !defined(__i586__)) || \
    (defined(_M_IX86) && _M_IX86 < 500)

static inline bool x86_has_cpuid(void)
{
    const uint32_t eflags_id = 1 << 21;
    uint32_t a, b;

    a = __readeflags();
    __writeeflags(a ^ eflags_id);
    b = __readeflags();
    __writeeflags(a);

    return !!((a ^ b) & eflags_id);
}

#else

static inline bool x86_has_cpuid(void)
{
    return true;
}

#endif

#ifdef HAVE___PAUSE
# define cpu_pause() __pause()
#elif defined(HAVE__MM_PAUSE)
# define cpu_pause() _mm_pause()
#elif defined(HAVE___BUILTIN_IA32_PAUSE)
# define cpu_pause() __builtin_ia32_pause()
#else
# define cpu_pause() ((void)0)
#endif

#define RDRAND_LOOPS 16

#define x86_make_random_func(instr,bits)				\
static size_t x86_ ## instr ## _get_random_bytes(void *ptr, size_t len)	\
{									\
    uint8_t *p = ptr;							\
    size_t left = len;							\
    rdrand_t r;								\
    int ctr;								\
    if (len <= 0)							\
	return 0;							\
    while (left) {							\
	ctr = RDRAND_LOOPS;						\
	NO_UNROLL							\
	while (unlikely(! _ ## instr ## bits ## _step(&r))) {		\
	    if (!--ctr)							\
		return len - left;					\
	    cpu_pause();						\
	}								\
	if (left < sizeof(rdrand_t)) {					\
	    rdrand_t rr = r;						\
	    memcpy(p, &rr, left);					\
	    break;							\
	}								\
	*(rdrand_t *)p = r;						\
	p += sizeof(rdrand_t);						\
	left -= sizeof(rdrand_t);					\
    }									\
    return len;								\
}

#ifdef HAVE__RDRAND64_STEP
typedef unsigned long long rdrand_t;
# ifdef HAVE__RDSEED64_STEP
x86_make_random_func(rdseed,64)
# endif
x86_make_random_func(rdrand,64)
#else
typedef unsigned int rdrand_t;
# ifdef HAVE__RDSEED32_STEP
x86_make_random_func(rdseed,32)
# endif
x86_make_random_func(rdrand,32)
#endif

static size_t cpu_init_get_random_bytes(void *ptr, size_t len)
{
    int id0[4];

    cpu_get_random_bytes = fail_get_random_bytes;
    if (!x86_has_cpuid())
	goto done;

    __cpuidex(id0, 0, 0);
#ifdef HAVE__RDSEED32_STEP
    if (id0[0] >= 7) {
	int id7[4];
	__cpuidex(id7, 7, 0);
	if (id7[1] & (1 << 18)) {
	    cpu_get_random_bytes = x86_rdseed_get_random_bytes;
	    goto done;
	}
    }
#endif
#ifdef HAVE__RDRAND32_STEP
    if (id0[0] >= 1) {
	int id1[4];
	__cpuidex(id1, 1, 0);
	if (id1[2] & (1 << 30)) {
	    cpu_get_random_bytes = x86_rdrand_get_random_bytes;
	    goto done;
	}
    }
#endif

done:
    return cpu_get_random_bytes(ptr, len);
}

#else

/* CPU-specific code needed here */
static int cpu_init_get_random_bytes(void *ptr, int len)
{
    return fail_get_random_bytes(ptr, len);
}

#endif

/* Fill with something that's supposedly better than nothing at all */
static size_t scribble_on_random_bytes(void *ptr, size_t len)
{
    uint64_t ns;

    if (len <= 0)
	return 0;

    ns = nstime() * UINT64_C(15669708845224982231); /* Prime */
    memcpy(ptr, &ns, len < 8 ? len : 8);

    return 0;			/* Not really random */
}

size_t get_random_bytes(void *ptr, size_t len)
{
    if (cpu_get_random_bytes(ptr, len) == len)
	return len;
    else if (os_get_random_bytes(ptr, len) == len)
	return len;
    else
	return scribble_on_random_bytes(ptr, len);
}

#define RANDOMIZE_WORDS 8
void randomize(void)
{
    uint32_t key_array[RANDOMIZE_WORDS];

    get_random_bytes(key_array, sizeof key_array);
    genrand_init_by_array(key_array, RANDOMIZE_WORDS);
}
