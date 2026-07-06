#include "core/rng.h"

static uint64_t splitmix64(uint64_t* state) {
    uint64_t z = (state[0] += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void rng_init(RNG* rng, uint64_t seed) {
    /* Avoid degenerate zero-state */
    rng->state = (seed == 0) ? 0xDEADBEEFCAFEBABEULL : seed;
}

uint64_t rng_next_u64(RNG* rng) {
    return splitmix64(&rng->state);
}

uint32_t rng_next_u32(RNG* rng) {
    return (uint32_t)(rng_next_u64(rng) >> 32);
}

int rng_range(RNG* rng, int lo, int hi) {
    if (hi <= lo) return lo;
    uint32_t range = (uint32_t)(hi - lo);
    return lo + (int)(rng_next_u32(rng) % range);
}

float rng_float(RNG* rng) {
    return (float)(rng_next_u64(rng) >> 40) * (1.0f / 16777216.0f);
}

int rng_bool(RNG* rng) {
    return (int)(rng_next_u64(rng) & 1ULL);
}
