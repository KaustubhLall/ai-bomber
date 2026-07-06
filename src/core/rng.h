#ifndef BOMBER_RNG_H
#define BOMBER_RNG_H

#include <stdint.h>

/* Deterministic splitmix64-based PRNG. Same seed -> same sequence everywhere. */

typedef struct {
    uint64_t state;
} RNG;

void rng_init(RNG* rng, uint64_t seed);
uint64_t rng_next_u64(RNG* rng);
uint32_t rng_next_u32(RNG* rng);
int rng_range(RNG* rng, int lo, int hi); /* [lo, hi) */
float rng_float(RNG* rng);               /* [0.0, 1.0) */
int rng_bool(RNG* rng);                  /* 0 or 1 */

#endif /* BOMBER_RNG_H */
