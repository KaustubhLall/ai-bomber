/* v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md): exercises
   prune_policy_target_visits() (src/training/native/policy_target_pruning.h), the pure Torch-free
   extraction of the KataGo policy-target-pruning arithmetic collect_self_play()/
   collect_league_play() (src/training/native/trainer.cpp) apply when config.forced_playouts_k>0.
   Deliberately links neither LibTorch/CUDA nor bomber_core - the header has no such dependency,
   and this test's build wiring (tests/CMakeLists.txt) proves that by construction. Plain
   assert()-based executable, the same convention as test_replay_sampling.cpp and the C tests in
   this directory (tests/CMakeLists.txt keeps NDEBUG off directory-wide so assert() stays live
   even under a Release config).

   Test cases mirror exactly the four named in the v7 Stage 0 design brief for this helper: no
   forcing -> identity; all-forced non-max action -> 0; partially-forced -> n-forced (floored at
   1 whenever genuinely selected once, which the arithmetic gives for free - see the header's own
   comment); max action untouched. A tie-break case and a raw-counts (non-normalized) case are
   added for the two behavioral contracts the header documents but the four named cases do not
   individually pin down. */

#include "training/native/policy_target_pruning.h"

#include <array>
#include <cassert>
#include <iostream>
#include <numeric>

namespace {

using bomber::az::prune_policy_target_visits;

/* No forcing at all (forced all zero) must return the visits array unchanged, action for
   action - the "config.forced_playouts_k<=0 never calls this at all, and even if it did, it
   would be a no-op" identity this feature's off-path relies on. */
void test_no_forcing_is_identity() {
    const std::array<int, 6> visits = {5, 3, 8, 1, 0, 2};
    const std::array<int, 6> forced = {0, 0, 0, 0, 0, 0};
    const auto pruned = prune_policy_target_visits(visits, forced);
    assert(pruned == visits);
    std::cout << "  no_forcing_is_identity: pruned == visits with all-zero forced\n";
}

/* A non-max action whose EVERY visit was forced (forced[a] == visits[a] > 0) must prune to
   exactly 0 - "the network should not be taught 'forced == good' merely from the forcing
   itself." A second non-max action with forced=0 is included as a control in the SAME call,
   proving the zeroing is per-action, not an accidental global effect. */
void test_all_forced_non_max_action_prunes_to_zero() {
    const std::array<int, 6> visits = {4, 6, 10, 3, 3, 0};
    const std::array<int, 6> forced = {4, 0, 5, 0, 3, 0};
    const auto pruned = prune_policy_target_visits(visits, forced);
    assert(pruned[0] == 0);   // all 4 of action 0's visits were forced
    assert(pruned[1] == 6);   // control: forced=0, action 1 untouched by pruning
    assert(pruned[4] == 0);   // all 3 of action 4's visits were forced
    std::cout << "  all_forced_non_max_action_prunes_to_zero: pruned=["
              << pruned[0] << "," << pruned[1] << "," << pruned[4] << "] (expected 0,6,0)\n";
}

/* Partially-forced non-max actions keep exactly their GENUINE (non-forced) visit count -
   "n-forced, floored at 1 when genuinely selected once" is the boundary of this (action 3:
   visits=5, forced=4, exactly 1 genuine visit -> pruned=1); action 4 (visits=7, forced=3, 4
   genuine visits -> pruned=4) confirms the general case is "keep the genuine count", not
   literally always 1. */
void test_partial_forcing_keeps_genuine_count() {
    const std::array<int, 6> visits = {10, 2, 2, 5, 7, 0};
    const std::array<int, 6> forced = {0, 0, 0, 4, 3, 0};
    const auto pruned = prune_policy_target_visits(visits, forced);
    assert(pruned[3] == 1);   // 5 visits, 4 forced, 1 genuine -> floored-at-1 boundary case
    assert(pruned[4] == 4);   // 7 visits, 3 forced, 4 genuine -> kept exactly, not clamped to 1
    std::cout << "  partial_forcing_keeps_genuine_count: pruned[3]=" << pruned[3]
              << " (expected 1), pruned[4]=" << pruned[4] << " (expected 4)\n";
}

/* The most-visited action a* is NEVER pruned, even if a large fraction (or, hypothetically, all)
   of ITS OWN visits were nominally forced - KataGo never prunes the action the policy target is
   built around; pruning only ever removes mass from non-maximal actions to shift the target
   toward a*, never away from it. */
void test_max_action_untouched_regardless_of_forced() {
    const std::array<int, 6> visits = {3, 1, 20, 0, 2, 4};
    const std::array<int, 6> forced = {0, 0, 20, 0, 0, 0};  // action 2 (the max) "all forced"
    const auto pruned = prune_policy_target_visits(visits, forced);
    assert(pruned[2] == 20);  // untouched: still the max action's full visit count
    std::cout << "  max_action_untouched_regardless_of_forced: pruned[2]=" << pruned[2]
              << " (expected 20, forced[2]=20 ignored)\n";
}

/* Tie-break: when two actions share the max visit count, only the LOWEST-index one is treated
   as a* (untouched); the other tied action is pruned like any ordinary non-max action - matches
   trainer.cpp's own >-strict tie-break convention (select_joint, select_joint_root_forced) of
   favoring the lower index on an exact tie. */
void test_tie_break_favors_lowest_index_as_max() {
    const std::array<int, 6> visits = {8, 8, 1, 0, 0, 0};
    const std::array<int, 6> forced = {8, 8, 0, 0, 0, 0};  // both tied actions "all forced"
    const auto pruned = prune_policy_target_visits(visits, forced);
    assert(pruned[0] == 8);  // lowest-index tie winner treated as a* -> untouched
    assert(pruned[1] == 0);  // the other tied action is NOT a* -> pruned like any all-forced one
    std::cout << "  tie_break_favors_lowest_index_as_max: pruned=[" << pruned[0] << ","
              << pruned[1] << "] (expected 8,0)\n";
}

/* Renormalization is explicitly the CALLER's job (trainer.cpp divides by the pruned total at
   both call sites) - this function returns raw counts, so the pruned array's sum need not equal
   the original visits' sum (and in general will be smaller, once anything was actually forced),
   and need not sum to any particular value at all. */
void test_returns_raw_counts_not_normalized() {
    const std::array<int, 6> visits = {10, 5, 5, 0, 0, 0};
    const std::array<int, 6> forced = {0, 5, 3, 0, 0, 0};
    const auto pruned = prune_policy_target_visits(visits, forced);
    const int visits_total = std::accumulate(visits.begin(), visits.end(), 0);
    const int pruned_total = std::accumulate(pruned.begin(), pruned.end(), 0);
    assert(pruned_total < visits_total);  // strictly smaller: real pruning occurred
    assert(pruned_total == 10 + 0 + 2);   // a*(10, untouched) + action1(0) + action2(5-3=2)
    std::cout << "  returns_raw_counts_not_normalized: visits_total=" << visits_total
              << " pruned_total=" << pruned_total << " (not renormalized by this function)\n";
}

}  // namespace

int main() {
    test_no_forcing_is_identity();
    test_all_forced_non_max_action_prunes_to_zero();
    test_partial_forcing_keeps_genuine_count();
    test_max_action_untouched_regardless_of_forced();
    test_tie_break_favors_lowest_index_as_max();
    test_returns_raw_counts_not_normalized();
    std::cout << "test_policy_target_pruning: ALL PASSED\n";
    return 0;
}
