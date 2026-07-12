"""KL-105 Phase 3: forking from the shared semantic-smoke fixture (cap explicitly 0 in its
manifest, since it was built by the current binary earlier in this same suite run) with an
explicit --replay-cause-balance-cap 0.25 override must (1) go through the normal semantic-fork
path - present key, value differs, logged exactly like sudden_death_start/arena_crush_win_value
overrides are - and (2) load/run cleanly with a tiny inherited replay buffer where pool A starts
at/near zero."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])

# (1) Semantic-fork visibility: the same machinery that prints "sudden_death_start: checkpoint=
# 160 -> explicit CLI=120" for other semantic fields (apply_semantic_manifest's reconcile_double
# lambda in trainer.cpp) must fire for replay_cause_balance_cap too, since the parent's manifest
# has the key present (=0) and this fork explicitly requests 0.25. Use only the LAST entry:
# --fresh truncates metrics.jsonl/config-history.jsonl but NOT semantic-fork-log.jsonl (verified
# empirically against the existing semantic-smoke fixture, which had 22 accumulated identical
# entries from past suite reruns), so this file can carry lines from earlier ctest invocations
# of this same run-dir - only the most recently appended entry is guaranteed to be from the
# load_checkpoint() call this specific test run just made.
fork_log_lines = (run_dir / "semantic-fork-log.jsonl").read_text().splitlines()
assert fork_log_lines, "expected at least one semantic-fork-log.jsonl entry"
last_entry = json.loads(fork_log_lines[-1])
matching = [fork for fork in last_entry["forks"] if fork.startswith("replay_cause_balance_cap:")]
assert matching, (
    f"no replay_cause_balance_cap fork line in the latest semantic-fork-log.jsonl entry (saw: "
    f"{last_entry['forks']}) - the explicit --replay-cause-balance-cap 0.25 override against a "
    f"checkpoint manifest value of 0 must be logged exactly like any other semantic field's "
    f"explicit override"
)
assert matching[0] == "replay_cause_balance_cap: checkpoint=0 -> explicit CLI=0.25", (
    f"unexpected fork line format: {matching[0]!r}"
)

# (2) Resolved config carries the override.
config = json.loads((run_dir / "config.json").read_text())
assert abs(config["replay_cause_balance_cap"] - 0.25) < 1e-9, (
    f"config.json replay_cause_balance_cap={config['replay_cause_balance_cap']}, expected 0.25"
)

# (3) The run completed and produced a well-formed metrics row with a small/bounded pool A -
# the parent's own replay buffer was collected at tiny fixture scale (2 games, 8 max-steps),
# so bomb kills are unlikely; this fork's own one new iteration is the only possible source of
# a nonzero pool, bounding it by new_samples for that iteration.
assert (run_dir / "latest.pt").is_file(), "forked run must produce a checkpoint"
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 1, f"expected exactly one new metrics row (one new iteration), got {len(rows)}"
row = rows[0]
pools = row["replay_cause_pools"]
assert pools["bomb_win_side_pool"] <= row["new_samples"], (
    f"bomb_win_side_pool={pools['bomb_win_side_pool']} exceeds this iteration's own "
    f"new_samples={row['new_samples']} - the inherited buffer's pool-A contribution must be "
    f"zero (or negligible) at this fixture scale, so any pool-A samples must come from the "
    f"one new iteration collected here"
)
fraction = row["optimization"]["realized_pool_a_batch_fraction"]
assert -1e-6 <= fraction <= 0.25 + 1e-6, (
    f"realized_pool_a_batch_fraction={fraction} outside [0, 0.25+eps]"
)

print("Native AlphaZero cause-balance semantic-fork visibility + tiny-buffer fork validated "
      f"(fork line={matching[0]!r}, bomb_win_side_pool={pools['bomb_win_side_pool']}, "
      f"realized_pool_a_batch_fraction={fraction})")
