#include "core/rng.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    RNG r1, r2;
    rng_init(&r1, 42);
    rng_init(&r2, 42);

    /* Same seed -> same sequence */
    for (int i = 0; i < 1000; i++) {
        uint64_t v1 = rng_next_u64(&r1);
        uint64_t v2 = rng_next_u64(&r2);
        assert(v1 == v2);
    }

    /* Different seeds -> different first value */
    RNG r3;
    rng_init(&r3, 99);
    assert(rng_next_u64(&r3) != rng_next_u64(&r1));

    /* Range is within bounds */
    rng_init(&r1, 1);
    for (int i = 0; i < 10000; i++) {
        int v = rng_range(&r1, 5, 10);
        assert(v >= 5 && v < 10);
    }

    /* Float is in [0, 1) */
    rng_init(&r1, 1);
    for (int i = 0; i < 10000; i++) {
        float f = rng_float(&r1);
        assert(f >= 0.0f && f < 1.0f);
    }

    /* Zero seed doesn't produce degenerate state */
    rng_init(&r1, 0);
    assert(rng_next_u64(&r1) != 0);

    printf("test_rng: ALL PASSED\n");
    return 0;
}
