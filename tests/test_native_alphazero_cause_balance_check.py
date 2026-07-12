"""KL-105 Phase 3 (docs/experiment-memory/13-kl105-experiment-design.md section 3): capped
cause-balanced replay sampling. Covers the fresh run (--replay-cause-balance-cap 0.25) plus a
1-iteration resume with no override (inherited from the manifest) - both write into the same
metrics.jsonl, so this single check validates the machinery (fields present, well-formed, cap
respected) and round-trip persistence (the resumed run's pool did not reset to zero) together.

CAUSE_KEYS below must match the six OutcomeCause values 0-5 emitted by
ReplayBuffer::cause_pool_snapshot() via append_metrics() in trainer.cpp - "unknown" is cause 0
(never classified: collect_teacher's imitation-learning bootstrap, or a legacy-loaded sample),
the rest are the same taxonomy used at both tagging sites (mirror/league finalization)."""

from __future__ import annotations

import json
import sys
from pathlib import Path

CAUSE_KEYS = ["unknown", "bomb", "selfkill", "arena_crush", "mutual_death", "timeout_draw"]
CAP = 0.25
FRACTION_TOLERANCE = 1e-6

run_dir = Path(sys.argv[1])
config = json.loads((run_dir / "config.json").read_text())

resolved_cap = config["replay_cause_balance_cap"]
assert abs(resolved_cap - CAP) < 1e-9, (
    f"config.json replay_cause_balance_cap={resolved_cap}, expected {CAP} - the config JSON "
    f"dump must carry the resolved semantic field like arena_crush_win_value does"
)

rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert rows, "expected at least one metrics.jsonl row"

by_iteration: dict[int, dict] = {}
for row in rows:
    pools = row.get("replay_cause_pools")
    assert pools is not None, f"iteration {row.get('iteration')}: missing replay_cause_pools"
    for key in CAUSE_KEYS + ["bomb_win_side_pool"]:
        assert key in pools, f"iteration {row['iteration']}: replay_cause_pools missing '{key}'"

    total = sum(pools[key] for key in CAUSE_KEYS)
    assert total == row["replay_size"], (
        f"iteration {row['iteration']}: cause bucket counts sum to {total}, expected "
        f"replay_size {row['replay_size']} - every sample must land in exactly one of the six "
        f"outcome_cause buckets"
    )
    assert pools["bomb_win_side_pool"] <= pools["bomb"], (
        f"iteration {row['iteration']}: bomb_win_side_pool={pools['bomb_win_side_pool']} "
        f"exceeds the bomb cause count {pools['bomb']} - pool A (bomb_win_side==1) must be a "
        f"subset of bomb-caused samples (the losing seat of a bomb game is cause=bomb but "
        f"bomb_win_side=0)"
    )

    fraction = row["optimization"]["realized_pool_a_batch_fraction"]
    assert -FRACTION_TOLERANCE <= fraction <= CAP + FRACTION_TOLERANCE, (
        f"iteration {row['iteration']}: realized_pool_a_batch_fraction={fraction} outside "
        f"[0, {CAP}+eps] - the sampler must never draw more than the configured cap"
    )
    by_iteration[row["iteration"]] = row

# Liveness: proves tagging flows end-to-end even if bomb kills specifically never occur at
# this tiny fixture scale (2 games, 8 max-steps) - games always end SOMEHOW (timeout/crush/
# selfkill/mutual all count), so at least one non-"unknown" cause must appear somewhere once
# any self-play game has completed.
last_row = rows[-1]
last_pools = last_row["replay_cause_pools"]
classified_total = sum(last_pools[key] for key in CAUSE_KEYS if key != "unknown")
assert classified_total > 0, (
    f"final row (iteration {last_row['iteration']}) has zero classified samples across all "
    f"five non-unknown causes ({last_pools}) - tagging does not appear to be running at all"
)

# Round-trip persistence across the resume: iteration 2 is the fresh run's last row, iteration
# 3 is the resumed run's (only) new row. append_metrics() snapshots cause pools AFTER
# replay.add() for that iteration (see the comment at its call site in trainer.cpp), so the
# resumed row's pool is the fresh run's final pool PLUS whatever the resumed iteration's own
# new samples contributed - never a reset to zero - as long as replay_capacity (128 here) has
# not evicted anything between the two rows, which it has not at this fixture's scale (a
# handful of samples per iteration).
assert 2 in by_iteration and 3 in by_iteration, (
    f"expected metrics rows for iterations 2 (end of fresh run) and 3 (resume), got "
    f"{sorted(by_iteration)}"
)
fresh_final_pool = by_iteration[2]["replay_cause_pools"]["bomb_win_side_pool"]
resumed_pool = by_iteration[3]["replay_cause_pools"]["bomb_win_side_pool"]
assert resumed_pool >= fresh_final_pool, (
    f"resumed run's bomb_win_side_pool ({resumed_pool}) is less than the fresh run's final "
    f"value ({fresh_final_pool}) - persistence means the tag round-trips through save/load, "
    f"so the pool must never shrink across a resume at this scale (no eviction expected)"
)
if fresh_final_pool > 0:
    note = (
        f"fresh_final_pool={fresh_final_pool} was already nonzero - persistence of a real "
        f"pool-A count across save/load is directly demonstrated (resumed_pool={resumed_pool})"
    )
else:
    note = (
        "fresh_final_pool was 0 at this tiny fixture scale (no bomb kills occurred) - only "
        "the machinery (fields present, bounds respected, monotonic non-decrease) is "
        "demonstrated here; end-to-end tagging itself is proven by the liveness check above"
    )

print("Native AlphaZero cause-balance sampling machinery + persistence validated "
      f"(cap={resolved_cap}, rows={len(rows)}, classified_total_final={classified_total}); "
      + note)
