#include "compiler.h"
#include "nstime.h"
#include "random.h"

static int cpu_init_get_random_bytes(void *, int);
static int os_init_get_random_bytes(void *, int);

static int (*cpu_get_random_bytes)(void *, int) = cpu_init_get_random_bytes;
static int (*os_get_random_bytes)(void *, int) = os_init_get_random_bytes;

static int fail_get_random_bytes(void *ptr, int len)
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

static int toread(int fd, void *buf, size_t count)
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

static int unix_get_random_bytes(void *ptr, int len)
{
    return toread(randfd, ptr, len);
}

static int os_init_get_random_bytes(void *ptr, int len)
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

static int win_get_random_bytes(void *ptr, int len)
{
    return CryptGenRandom(hProv, len, ptr) ? len : 0;
}

static int os_init_get_random_bytes(void *ptr, int len)
{
    static const DWORD providers[] = {
	PROV_RNG, PROV_INTEL_SEC, PROV_RSA_SIG, PROV_RSA_FULL,
	PROV_DSS, PROV_SSL, 0
    };
    const DWORD *pp = providers;

    while (*pp) {
	if (CryptAcquireContext(&hProv, NULL, NULL, *pp,
				CRYPT_SILENT | CRYPT_VERIFYCONTEXT)) {
	    atexit(close_provider);

	    os_get_random_bytes = win_get_random_bytes;
	    return win_get_random_bytes(ptr, len);
	}
	pp++;
    }

    os_get_random_bytes = fail_get_random_bytes;
    return 0;
}

#else

/* Need system-specific code here */

static int os_init_get_random_bytes(void *ptr, int len)
{
    return fail_get_random_bytes(ptr, len);
}

#endif

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

struct cpuid {
    uint32_t eax, ecx, edx, ebx;
};
static inline struct cpuid x86_cpuid(uint32_t leaf, uint32_t subleaf)
{
    struct cpuid id;
    asm("cpuid"
	: "=a" (id.eax), "=c" (id.ecx),
	  "=d" (id.edx), "=b" (id.ebx)
	: "a" (leaf), "c" (subleaf));
    return id;
}
static inline bool x86_has_cpuid(void)
{
#ifdef __x86_64__
    return true;
#else
    const uint32_t eflags_id = 1 << 21;
    uint32_t a, b;
    asm("pushf; "
	"pushf; "
	"pop %0; "
	"mov %0,%1; "
	"xor %2,%1; "
	"push %1; "
	"popf; "
	"pushf; "
	"pop %1; "
	"popf"
	: "=r" (a), "=r" (b)
	: "i" (eflags_id));
    return !!((a ^ b) & eflags_id);
#endif
}

#ifdef __x86_64__
typedef uint64_t rdrand_t;
#else
typedef uint32_t rdrand_t;
#endif

#define x86_make_random_func(instr)					\
    static int x86_ ## instr ## _get_random_bytes(void *ptr, int len)	\
    {									\
        uint8_t *p = ptr;						\
	size_t left = len;						\
	if (len <= 0)							\
	    return 0;							\
	while (left >= sizeof(rdrand_t)) {				\
	    asm volatile(#instr " %0" : "=r" (*(rdrand_t *)p));		\
	    p += sizeof(rdrand_t);					\
	    left -= sizeof(rdrand_t);					\
	}								\
	if (left) {							\
	    rdrand_t r;							\
	    asm volatile(#instr " %0" : "=r" (r));			\
	    memcpy(p, &r, left);					\
	}								\
        return len;							\
    }

x86_make_random_func(rdseed)
x86_make_random_func(rdrand)

static int cpu_init_get_random_bytes(void *ptr, int len)
{
    struct cpuid id0;

    cpu_get_random_bytes = fail_get_random_bytes;
    if (!x86_has_cpuid())
	goto done;

    id0 = x86_cpuid(0, 0);
    if (id0.eax >= 7) {
	struct cpuid id7 = x86_cpuid(7, 0);
	if (id7.ebx & (1 << 18)) {
	    cpu_get_random_bytes = x86_rdseed_get_random_bytes;
	    goto done;
	}
    }
    if (id0.eax >= 1) {
	struct cpuid id1 = x86_cpuid(1, 0);
	if (id1.ecx & (1 << 30)) {
	    cpu_get_random_bytes = x86_rdrand_get_random_bytes;
	    goto done;
	}
    }

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
static int scribble_on_random_bytes(void *ptr, int len)
{
    uint64_t ns;

    if (len <= 0)
	return 0;

    ns = nstime() * UINT64_C(15669708845224982231); /* Prime */
    memcpy(ptr, &ns, len < 8 ? len : 8);

    return 0;			/* Not really random */
}

int get_random_bytes(void *ptr, int len)
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
