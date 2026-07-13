#pragma once

/* KL-110 hygiene: pure, Torch-free extraction of the index-selection logic that lives inside
   ReplayBuffer::batch() (src/training/native/trainer.cpp). Deliberately kept in its own header
   with no dependency on Sample/at::Half/torch - Sample embeds an at::Half state array, so
   extracting the whole ReplayBuffer would pull LibTorch into the test surface just to exercise
   an RNG/index-selection algorithm that never touches a tensor. ReplayBuffer::batch() builds
   pool_a (and, in the same pass, a membership vector for counting total pool-A representation)
   from its own samples_, calls select_replay_indices() below, and only then fills tensor rows
   from the returned indices - this function IS the sampling behavior, not a parallel
   reimplementation kept in sync by hand. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace bomber::az {

/* Selects `count` replay-buffer row indices, each in [0, buffer_size).

   - cause_balance_cap <= 0.0, OR pool_a empty (for any reason - cap<=0 never even builds it,
     or cap>0 with a genuinely empty pool A): plain uniform sampling over the whole buffer -
     exactly one std::uniform_int_distribution<size_t>(0, buffer_size-1) draw per row from a
     single distribution instance, byte-identical RNG consumption to the pre-KL-105 sampler
     (the "cap=0 is exactly current behavior" contract KL-105 Phase 3 was gated on).
   - otherwise: f = min(cause_balance_cap, boost_max * pool_a.size() / buffer_size),
     forced_pool_a_draws = clamp(lround(f * count), 0, count); the first forced_pool_a_draws
     returned rows are drawn uniformly-with-replacement from pool_a, the remaining rows are
     drawn uniformly from the whole buffer (which may also land in pool_a by chance - see the
     forced-vs-total distinction below).

   Writes the number of FORCED pool-A draws to *forced_pool_a_draws_out (if non-null): the
   count the sampler was compelled to take from pool_a, as opposed to how many SELECTED rows
   (forced or drawn from the uniform remainder) actually land in pool A - the caller counts the
   latter itself against the same pool_a it passed in, since only the caller has a cheap
   membership test available (it already scans samples_ once to build pool_a). */
inline std::vector<size_t> select_replay_indices(
        size_t buffer_size, const std::vector<size_t>& pool_a, double cause_balance_cap,
        double boost_max, int count, std::mt19937_64& rng, int* forced_pool_a_draws_out) {
    int forced_pool_a_draws = 0;
    if (!pool_a.empty()) {
        const double n_a = static_cast<double>(pool_a.size());
        const double n = static_cast<double>(buffer_size);
        const double fraction = std::min(cause_balance_cap, boost_max * n_a / n);
        forced_pool_a_draws =
            std::clamp(static_cast<int>(std::lround(fraction * count)), 0, count);
    }
    if (forced_pool_a_draws_out) *forced_pool_a_draws_out = forced_pool_a_draws;

    std::vector<size_t> indices(static_cast<size_t>(count));
    if (forced_pool_a_draws > 0) {
        std::uniform_int_distribution<size_t> choose_pool_a(0, pool_a.size() - 1);
        std::uniform_int_distribution<size_t> choose_any(0, buffer_size - 1);
        for (int row = 0; row < count; ++row) {
            indices[static_cast<size_t>(row)] = row < forced_pool_a_draws
                ? pool_a[choose_pool_a(rng)]
                : choose_any(rng);
        }
    } else {
        std::uniform_int_distribution<size_t> choose(0, buffer_size - 1);
        for (int row = 0; row < count; ++row) indices[static_cast<size_t>(row)] = choose(rng);
    }
    return indices;
}

}  // namespace bomber::az
