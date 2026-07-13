/* KL-110 hygiene: exercises select_replay_indices() (src/training/native/replay_sampling.h),
   the pure Torch-free extraction of ReplayBuffer::batch()'s index-selection logic
   (src/training/native/trainer.cpp). Deliberately links neither LibTorch/CUDA nor bomber_core -
   the header has no such dependency, and this test's build wiring (tests/CMakeLists.txt) proves
   that by construction. Plain assert()-based executable, the same convention as the C tests in
   this directory (tests/CMakeLists.txt keeps NDEBUG off directory-wide so assert() stays live
   even under a Release config). */

#include "training/native/replay_sampling.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <utility>
#include <vector>

namespace {

using bomber::az::select_replay_indices;

/* cause_balance_cap<=0.0 (empty pool_a passed, matching how ReplayBuffer::batch() never builds
   pool_a at all when the cap is <=0) must be byte-identical to constructing ONE
   std::uniform_int_distribution<size_t>(0, buffer_size-1) and drawing `count` times in a row
   from a freshly-seeded rng - exactly what the pre-extraction inline code did, and exactly what
   the pre-KL-105 sampler did before pool-A boosting existed at all.

   Proof this is the right test for "byte-compatible": std::mt19937_64/std::uniform_int_
   distribution are pure functions of engine state within one toolchain/build - identical seeds
   feeding an identically-constructed distribution, drawn from in the same order, MUST reproduce
   identical output sequences unless something else consumes or perturbs the rng in between.
   Both the reference loop below and select_replay_indices()'s uniform branch execute that exact
   construct-once/draw-per-row pattern and nothing else; this test compares their ACTUAL output
   index-for-index rather than relying on that argument alone, so it would catch a real
   discrepancy (e.g. an extra/missing draw, a reordered construction) that the argument alone
   would not. */
void test_cap_zero_identity_matches_reference_uniform_loop() {
    constexpr size_t kBufferSize = 1000;
    constexpr int kCount = 200;
    constexpr double kBoostMax = 16.0;
    constexpr uint64_t kSeed = 12345ULL;

    std::mt19937_64 reference_rng(kSeed);
    std::uniform_int_distribution<size_t> choose(0, kBufferSize - 1);
    std::vector<size_t> reference_indices(static_cast<size_t>(kCount));
    for (int row = 0; row < kCount; ++row)
        reference_indices[static_cast<size_t>(row)] = choose(reference_rng);

    std::mt19937_64 rng(kSeed);
    int forced_pool_a_draws = -999;
    const std::vector<size_t> indices = select_replay_indices(
        kBufferSize, /*pool_a=*/{}, /*cause_balance_cap=*/0.0, kBoostMax, kCount, rng,
        &forced_pool_a_draws);

    assert(forced_pool_a_draws == 0);
    assert(indices.size() == reference_indices.size());
    assert(indices == reference_indices);
    std::cout << "  cap_zero_identity: " << kCount << " indices match reference uniform loop\n";
}

/* cause_balance_cap > 0 but pool_a genuinely empty (n_A == 0) must fall back to the exact same
   uniform path as cap<=0 - a DIFFERENT reason for pool_a.empty() than the test above (here the
   cap is meaningfully nonzero; there simply are no pool-A samples yet, e.g. early in training
   before any bomb-decisive game has been collected). Also confirmed RNG-identical to a
   reference loop for the same reason as above - this exercises the same "pool_a.empty()"
   uniform branch through a different precondition, so it is not redundant with the cap=0 case. */
void test_empty_pool_a_falls_back_to_uniform() {
    constexpr size_t kBufferSize = 500;
    constexpr int kCount = 50;
    constexpr double kBoostMax = 16.0;
    constexpr uint64_t kSeed = 999ULL;

    std::mt19937_64 reference_rng(kSeed);
    std::uniform_int_distribution<size_t> choose(0, kBufferSize - 1);
    std::vector<size_t> reference_indices(static_cast<size_t>(kCount));
    for (int row = 0; row < kCount; ++row)
        reference_indices[static_cast<size_t>(row)] = choose(reference_rng);

    std::mt19937_64 rng(kSeed);
    int forced_pool_a_draws = -999;
    const std::vector<size_t> indices = select_replay_indices(
        kBufferSize, /*pool_a=*/{}, /*cause_balance_cap=*/0.9, kBoostMax, kCount, rng,
        &forced_pool_a_draws);

    assert(forced_pool_a_draws == 0);
    assert(indices == reference_indices);
    for (size_t index : indices) assert(index < kBufferSize);
    std::cout << "  empty_pool_a_fallback: n_A=0 with cap=0.9 still takes the uniform path\n";
}

/* Tiny pool A (n_A=5 of N=10000) with a large cap (0.5): the boost_max*n_A/N term
   (16*5/10000 = 0.008) binds, not the cap. forced_pool_a_draws must equal
   lround(boost_max*n_A/N*count) exactly - chosen (8.0) to land exactly on an integer so there
   is no rounding-boundary ambiguity to reason about. Also checks every forced row is drawn from
   pool_a, and that forced_pool_a_draws_out is genuinely WRITTEN (seeded with a sentinel that is
   not the expected answer, so a no-op write would be caught). */
void test_boost_limited_forced_count() {
    constexpr size_t kBufferSize = 10000;
    constexpr int kCount = 1000;
    constexpr double kBoostMax = 16.0;
    constexpr double kCap = 0.5;
    constexpr uint64_t kSeed = 42ULL;
    const std::vector<size_t> pool_a = {0, 1, 2, 3, 4};
    constexpr int kExpectedForced = 8;  // lround(16 * 5 / 10000.0 * 1000) == lround(8.0) == 8

    std::mt19937_64 rng(kSeed);
    int forced_pool_a_draws = -999;
    const std::vector<size_t> indices = select_replay_indices(
        kBufferSize, pool_a, kCap, kBoostMax, kCount, rng, &forced_pool_a_draws);

    assert(forced_pool_a_draws == kExpectedForced);
    assert(static_cast<int>(indices.size()) == kCount);
    for (int row = 0; row < forced_pool_a_draws; ++row) {
        const size_t index = indices[static_cast<size_t>(row)];
        assert(index <= 4);  // every forced row drawn from pool_a == {0,1,2,3,4}
    }
    std::cout << "  boost_limited_forced_count: forced=" << forced_pool_a_draws
              << " (expected " << kExpectedForced << ")\n";
}

/* Large pool A (n_A=6000 of N=10000, a contiguous prefix [0,6000) so membership is a trivial
   index<6000 check) with a binding cap (0.3): boost_max*n_A/N (16*6000/10000=9.6) is far larger
   than the cap, so the cap term wins. forced_pool_a_draws must equal lround(cap*count) exactly
   (0.3*1000=300.0, again an exact integer). Also checks every forced row is drawn from pool_a
   and that the out-param is genuinely written. Returns the indices/forced count so the
   total-vs-forced test below can reuse this same large-pool scenario. */
std::pair<std::vector<size_t>, int> test_cap_limited_forced_count() {
    constexpr size_t kBufferSize = 10000;
    constexpr size_t kPoolASize = 6000;
    constexpr int kCount = 1000;
    constexpr double kBoostMax = 16.0;
    constexpr double kCap = 0.3;
    constexpr uint64_t kSeed = 2026ULL;
    std::vector<size_t> pool_a(kPoolASize);
    for (size_t i = 0; i < kPoolASize; ++i) pool_a[i] = i;
    constexpr int kExpectedForced = 300;  // lround(0.3 * 1000) == lround(300.0) == 300

    std::mt19937_64 rng(kSeed);
    int forced_pool_a_draws = -999;
    std::vector<size_t> indices = select_replay_indices(
        kBufferSize, pool_a, kCap, kBoostMax, kCount, rng, &forced_pool_a_draws);

    assert(forced_pool_a_draws == kExpectedForced);
    for (int row = 0; row < forced_pool_a_draws; ++row)
        assert(indices[static_cast<size_t>(row)] < kPoolASize);
    std::cout << "  cap_limited_forced_count: forced=" << forced_pool_a_draws
              << " (expected " << kExpectedForced << ")\n";
    return {std::move(indices), forced_pool_a_draws};
}

/* Total-vs-forced counting: count EVERY selected row (forced or drawn from the uniform
   remainder) that lands in pool A, using the large-pool scenario above (pool A = 60% of the
   buffer). With 1000-300=700 uniform-over-the-whole-buffer remainder draws each independently
   having a 60% chance of landing in pool A, the remainder alone is expected to contribute
   hundreds more pool-A hits, so total must be >= forced - the reviewer's exact point: uniform
   draws can also select pool A, so the forced count alone understates true pool-A
   representation in the batch. (kCount/kPoolASize/kBufferSize duplicated from
   test_cap_limited_forced_count() rather than threaded through as parameters, to keep that
   function's signature focused on what it is actually asserting.) */
void test_total_vs_forced_when_pool_a_large(const std::vector<size_t>& indices,
                                             int forced_pool_a_draws) {
    constexpr size_t kPoolASize = 6000;
    constexpr int kCount = 1000;

    int total_in_pool_a = 0;
    for (int row = 0; row < kCount; ++row)
        if (indices[static_cast<size_t>(row)] < kPoolASize) ++total_in_pool_a;

    assert(total_in_pool_a >= forced_pool_a_draws);
    std::cout << "  total_vs_forced: total=" << total_in_pool_a
              << " forced=" << forced_pool_a_draws << " (total must be >= forced)\n";
}

}  // namespace

int main() {
    test_cap_zero_identity_matches_reference_uniform_loop();
    test_empty_pool_a_falls_back_to_uniform();
    test_boost_limited_forced_count();
    auto [indices, forced] = test_cap_limited_forced_count();
    test_total_vs_forced_when_pool_a_large(indices, forced);
    std::cout << "test_replay_sampling: ALL PASSED\n";
    return 0;
}
