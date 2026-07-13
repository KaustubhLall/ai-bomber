"""KL-105 Phase 3: forking from tests/fixtures/native-alphazero-pre-cause-tag-checkpoint.pt - a
checkpoint snapshotted at git commit 58f688bd4f9b, before this feature's own work started - is
the genuine "missing archive key AND missing manifest key" case, unlike the shared semantic-
smoke fixture (rebuilt fresh by the current binary every suite run, so it always has both).
Proves ReplayBuffer::load()'s try_read fallback: no archive key -> every inherited sample
defaults to outcome_cause=0/bomb_win_side=0 (not a crash, not garbage), and
apply_semantic_manifest's missing-key branch inherits the compiled default (0.0) silently
(no fork line - there is nothing stored to diverge from), rather than the present-but-0 case
the sibling fork_legacy test exercises."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])

# The run must load and complete without error (ctest's exit-code check on the native train
# test already enforces this via DEPENDS; re-confirmed here by the presence of well-formed
# output files).
assert (run_dir / "latest.pt").is_file(), "forked run must produce a checkpoint"

config = json.loads((run_dir / "config.json").read_text())
assert abs(config["replay_cause_balance_cap"] - 0.25) < 1e-9, (
    f"config.json replay_cause_balance_cap={config['replay_cause_balance_cap']}, expected 0.25 "
    f"- with the key absent from the parent's manifest, the explicit CLI value must still win "
    f"(nothing stored to overwrite it with)"
)

# No fork line for this field: reconcile_double's absent-key branch returns before ever
# comparing against explicit_semantic_flags, so there is nothing to log as diverging - a fork
# line would indicate the missing-key path incorrectly fabricated a "stored" value.
fork_log_path = run_dir / "semantic-fork-log.jsonl"
if fork_log_path.exists():
    entries = [json.loads(line) for line in fork_log_path.read_text().splitlines()]
    all_forks = [fork for entry in entries for fork in entry["forks"]]
    bogus = [fork for fork in all_forks if fork.startswith("replay_cause_balance_cap:")]
    assert not bogus, (
        f"unexpected replay_cause_balance_cap fork line(s) against a checkpoint whose manifest "
        f"never had the key: {bogus}"
    )

rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 1, f"expected exactly one new metrics row (one new iteration), got {len(rows)}"
row = rows[0]
pools = row["replay_cause_pools"]
cause_total = sum(pools[key] for key in
                   ["unknown", "bomb", "selfkill", "arena_crush", "mutual_death", "timeout_draw"])
assert cause_total == row["replay_size"], (
    f"cause bucket counts sum to {cause_total}, expected replay_size {row['replay_size']}"
)

inherited_count = row["replay_size"] - row["new_samples"]
assert inherited_count >= 0, (
    f"replay_size ({row['replay_size']}) < new_samples ({row['new_samples']}) - impossible"
)
# Every inherited (pre-existing) sample must default to unknown=0 (no archive key to read tags
# from); this iteration's own new_samples are the only possible source of a classified sample,
# so unknown's count is bounded below by the inherited portion.
assert pools["unknown"] >= inherited_count, (
    f"unknown={pools['unknown']} is less than the inherited sample count {inherited_count} "
    f"({row['replay_size']} total - {row['new_samples']} new) - every sample loaded from a "
    f"checkpoint with no replay_tags archive key must default to outcome_cause=unknown"
)
assert pools["bomb_win_side_pool"] <= row["new_samples"], (
    f"bomb_win_side_pool={pools['bomb_win_side_pool']} exceeds this iteration's own "
    f"new_samples={row['new_samples']} - the inherited (untagged) portion of the buffer must "
    f"contribute exactly 0 to pool A"
)
fraction = row["optimization"]["realized_pool_a_batch_fraction"]
assert -1e-6 <= fraction <= 0.25 + 1e-6, (
    f"realized_pool_a_batch_fraction={fraction} outside [0, 0.25+eps]"
)

print("Native AlphaZero true-legacy (missing archive key) fork validated "
      f"(inherited={inherited_count}, unknown={pools['unknown']}, "
      f"bomb_win_side_pool={pools['bomb_win_side_pool']}, no spurious fork line)")
