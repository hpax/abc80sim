#ifndef RANDOM_H
#define RANDOM_H

#include "compiler.h"

/* Get random bytes from the system */
extern int get_random_bytes(void *, int);
extern void randomize(void);	/* Initializes PRNG */

/* Pseudo-random number generator (Mersenne Twister) */
extern double genrand_res53(void); /* Floating-point 0 <= x < 1 */
extern uint32_t genrand_int32(void);
extern void genrand_data(void *, size_t);
extern void genrand_init_by_array(const uint32_t *, uint32_t);

#endif /* RANDOM_H */
