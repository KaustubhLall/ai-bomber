"""KL-107 v3 regression: trace-output's policy_head_raw_recomputed must be genuinely raw
(pre-safety-mask) and safe_action_mask must be the real mask applied to derive
policy_prior_after_safety_mask - not another copy of the search's already-masked prior. This
guards against the exact class of bug already found once in this project: v1/v2 called the
masked prior raw_policy and reported "the policy itself is passive" from it (see the correction
in Linear KL-98/KL-100 and docs/experiment-memory/10-autonomous-brick-execution.md). A test that
only checks well-formedness (distributions sum to 1, arrays are the right length) would have
passed on the old mislabeled data too - every assertion below instead checks a property that is
specifically false if raw_policy is secretly the masked prior again."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ACTIONS = 6
WAIT = 5
EPSILON = 1e-6

path = Path(sys.argv[1])
lines = [line for line in path.read_text().splitlines() if line.strip()]
assert len(lines) >= 2, f"expected a header line plus at least one traced step, got {len(lines)}"

header = json.loads(lines[0])
assert header["trace_format_version"] == 3, (
    f"expected trace_format_version 3, got {header.get('trace_format_version')!r} - "
    f"this test's assertions are specific to the v3 raw/mask/Q fields"
)
assert header["checkpoint_sha256"] and header["executable_sha256"] and header["git_commit"]

rows = [json.loads(line) for line in lines[1:]]

for row in rows:
    for key in ("policy_head_raw_recomputed", "safe_action_mask", "search_root_q_values",
                "policy_prior_after_safety_mask", "mcts_policy"):
        assert len(row[key]) == ACTIONS, f"{key} must have {ACTIONS} entries, row={row}"
    assert isinstance(row["wait_forced"], bool)
    assert row["safe_action_count"] == sum(row["safe_action_mask"]), (
        f"safe_action_count must equal the number of 1s in safe_action_mask, row={row}"
    )
    # Invariant that must hold regardless of what a masked policy IS: wherever the mask is 0,
    # the search's post-mask prior must be exactly 0 there too.
    for action in range(ACTIONS):
        if row["safe_action_mask"][action] == 0:
            assert row["policy_prior_after_safety_mask"][action] == 0.0, (
                f"masked action {action} has nonzero policy_prior_after_safety_mask, "
                f"row={row}"
            )
    raw_total = sum(row["policy_head_raw_recomputed"])
    assert abs(raw_total - 1.0) < 1e-3, (
        f"policy_head_raw_recomputed should be a probability distribution (sums to ~1), "
        f"got sum={raw_total}, row={row}"
    )
    if row["wait_forced"]:
        assert row["safe_action_count"] == 1 and row["safe_action_mask"][WAIT] == 1, (
            f"wait_forced set but not exactly one safe action with WAIT as that action, row={row}"
        )

# wait_forced must never be true when more than one action is safe - this is the property that
# actually distinguishes "forced idling" from "chosen idling"; get it backwards and every WAIT
# looks forced (or none ever do), silently breaking the diagnostic this field exists for.
multi_safe_forced = [r for r in rows if r["safe_action_count"] > 1 and r["wait_forced"]]
assert not multi_safe_forced, f"wait_forced=true with >1 safe action: {multi_safe_forced[:1]}"

# The decisive property: find rows where an action is masked, and confirm the RAW policy still
# carries real probability mass there. If policy_head_raw_recomputed were secretly derived from
# (or equal to) the masked prior - the exact historical bug - this would be zero everywhere the
# mask is zero, and this loop would find nothing.
masked_rows = [r for r in rows if r["safe_action_count"] < ACTIONS]
assert masked_rows, (
    "no traced step had any masked action - cannot verify raw vs masked divergence; "
    "widen the fixture (more games/steps) rather than weaken this check"
)
divergent = [
    r for r in masked_rows
    if any(r["safe_action_mask"][a] == 0 and r["policy_head_raw_recomputed"][a] > EPSILON
           for a in range(ACTIONS))
]
assert divergent, (
    "every masked row had ~zero raw probability on its masked action(s) too - "
    "policy_head_raw_recomputed looks like it was derived from the masked prior, "
    "not recomputed from the unmasked policy head (the v1/v2 raw_policy mislabeling bug)"
)
# Most, not necessarily all: softmax output can occasionally fall below EPSILON on a
# masked action without being exactly zero (a converged, confident checkpoint can push a
# disfavored action's probability well under 1e-6), so requiring every masked row to clear
# an arbitrary threshold would eventually false-fail on a checkpoint this fixture doesn't
# represent. The property that actually disproves the mislabeling bug is "this happens", not
# "this happens on literally every row" - a strong majority is enough evidence either way.
divergent_fraction = len(divergent) / len(masked_rows)
assert divergent_fraction >= 0.5, (
    f"only {len(divergent)}/{len(masked_rows)} ({100*divergent_fraction:.0f}%) masked rows "
    f"show raw/masked divergence above {EPSILON} - expected most of them"
)

# Cross-check: renormalizing the raw policy by the same safe mask should closely reproduce the
# search's own masked prior. This is the empirical faithfulness test for recomputing outside the
# search rather than capturing from inside it - large disagreement would mean the recomputed
# forward pass is evaluating something other than the position the search actually searched.
max_cross_check_diff = 0.0
for row in rows:
    raw = row["policy_head_raw_recomputed"]
    mask = row["safe_action_mask"]
    total = sum(v for v, m in zip(raw, mask) if m)
    if total <= 0:
        continue
    derived = [(v / total if m else 0.0) for v, m in zip(raw, mask)]
    diff = max(abs(a - b) for a, b in zip(derived, row["policy_prior_after_safety_mask"]))
    max_cross_check_diff = max(max_cross_check_diff, diff)
assert max_cross_check_diff < 0.05, (
    f"recomputed policy, masked and renormalized, diverges from the search's own "
    f"policy_prior_after_safety_mask by up to {max_cross_check_diff:.4f} - expected agreement "
    f"within BF16/CUDA-batching tolerance if the recomputed forward pass is faithfully "
    f"reproducing the root position the search evaluated"
)

# search_root_q_values must be finite and zero exactly where never-safe (visits are always 0 for
# a masked-out action, so the marginal division-guard should have left it at the 0.0 default).
for row in rows:
    for action in range(ACTIONS):
        if row["safe_action_mask"][action] == 0:
            assert row["search_root_q_values"][action] == 0.0, (
                f"masked action {action} has a nonzero search_root_q_values entry, row={row}"
            )

forced_count = sum(1 for r in rows if r["wait_forced"])
print(f"Native AlphaZero KL-107 v3 raw-trace validated ({len(rows)} rows, "
      f"{len(masked_rows)} with a masked action, all {len(divergent)} showing real raw-vs-masked "
      f"divergence, max cross-check diff={max_cross_check_diff:.4f}, "
      f"{forced_count} forced-WAIT rows)")
