#pragma once

/* v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md; KataGo forced
   playouts + policy-target pruning, Wu arXiv:1902.10565 S4.1-4.2, independently replicated by
   Trudeau & Bowling 2023): pure, Torch-free extraction of the pruning arithmetic that
   collect_self_play()/collect_league_play() (src/training/native/trainer.cpp) apply when
   building the POLICY TRAINING TARGET from root visit marginals - mirrors replay_sampling.h's
   precedent (kept out of trainer.cpp so the arithmetic itself is unit-testable without linking
   LibTorch/CUDA). Action SELECTION (sample_joint_action's input) always uses the UNPRUNED
   marginal visits from SearchResult - this function is called only when constructing the
   separate policy-target array, never for the actual move played.

   KataGo's full algorithm re-runs a UCB search to find, for each non-maximal action a, the
   largest visit count at which raw PUCT would still have chosen a over the max-visit action a*
   at some point in the actual search - that requires the search's Q/prior state at every
   intermediate visit count, which this trainer does not retain. This function instead applies
   the practical simplification the v7 design doc calls out: since every FORCED selection
   (BatchedMcts::select_joint_root_forced in trainer.cpp) is, by construction, also a real visit
   (forcing always selects AND descends AND backs up, so forced[a] <= visits[a] holds for every
   action a, always - a forced count can never exceed the visits it produced), subtracting
   forced[a] from visits[a] exactly separates each action's forced-only visits from its
   genuinely-PUCT-won visits:
     - visits[a] - forced[a] == 0 whenever EVERY visit action a received was forced (it was
       never independently favored by PUCT) - pruned to 0, matching "the network should not be
       taught 'forced == good' merely from the forcing itself."
     - visits[a] - forced[a] >= 1 whenever AT LEAST ONE visit was genuinely PUCT-selected - the
       genuine count is kept exactly. This is where "keep at least 1 visit if the action was
       genuinely selected at least once by plain PUCT" falls out of the arithmetic directly
       (an integer subtraction where forced[a] < visits[a] can never land below 1) rather than
       needing a separate floor/clamp step.
   Judgment call, flagged for review: the design brief's own prose formula for this
   simplification - "subtract min(forced_count(a), n(a)-1)" - does not actually match its own
   listed test cases (that formula gives 1, not 0, in the "all visits forced" case, since
   min(forced,n-1) = n-1 when forced==n, leaving n-(n-1)=1). The four listed test cases (no
   forcing -> identity; all-forced non-max action -> 0; partially-forced -> n-forced, floored at
   1 when genuinely selected once; max action untouched) are unambiguous and are what this
   function implements; they are consistent with each other and with plain "n(a) - forced(a),
   clamped at 0" without needing the min(...,n(a)-1) formula at all. See
   tests/test_policy_target_pruning.cpp for exactly these four cases.

   The most-visited action a* (ties broken toward the lowest action index, matching trainer.cpp's
   >-strict tie-break convention elsewhere) is always left untouched, regardless of its own
   forced count - KataGo never prunes the action the target is built around.

   Renormalization to a probability distribution is the CALLER's job (trainer.cpp already has a
   total-visits-safe-divide pattern at both call sites) - this returns raw pruned visit counts,
   not probabilities. */

#include <algorithm>
#include <array>
#include <cstddef>

namespace bomber::az {

template <size_t kActionCount>
std::array<int, kActionCount> prune_policy_target_visits(
        const std::array<int, kActionCount>& visits,
        const std::array<int, kActionCount>& forced) {
    const size_t best_action = static_cast<size_t>(
        std::distance(visits.begin(), std::max_element(visits.begin(), visits.end())));

    std::array<int, kActionCount> pruned{};
    for (size_t action = 0; action < kActionCount; ++action) {
        if (action == best_action) {
            pruned[action] = visits[action];
            continue;
        }
        const int genuine = visits[action] - forced[action];
        pruned[action] = genuine > 0 ? genuine : 0;
    }
    return pruned;
}

}  // namespace bomber::az
