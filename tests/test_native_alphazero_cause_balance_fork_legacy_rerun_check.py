"""KL-110 hygiene regression: --fresh must truncate semantic-fork-log.jsonl exactly like it
truncates metrics.jsonl/config-history.jsonl - not just for the LAST entry a reader happens to
look at (test_native_alphazero_cause_balance_fork_legacy_check.py's pre-fix workaround), but for
real: repeated --fresh (--fork-from) runs into the same run-dir must not accumulate every past
run's fork entries into one file.

test_native_alphazero_cause_balance_fork_legacy already ran --fresh --fork-from .../semantic-
smoke/latest.pt --replay-cause-balance-cap 0.25 ONCE into this run-dir, and
test_native_alphazero_cause_balance_fork_legacy_check.py validated that first run's output.
test_native_alphazero_cause_balance_fork_legacy_rerun then re-ran the IDENTICAL command into the
SAME run-dir a second time. On the pre-fix code, semantic-fork-log.jsonl would now hold TWO
entries (one appended by each run); on fixed code, --fresh deletes the file at the start of the
second run before load_checkpoint() appends its own, leaving exactly one."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])

# The core regression assertion: exactly one entry survives a second --fresh --fork-from run
# into the same directory, not an accumulation of this run's entry plus the earlier run's.
fork_log_lines = (run_dir / "semantic-fork-log.jsonl").read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl entry after a second --fresh --fork-from run "
    f"into the same run-dir, got {len(fork_log_lines)} - --fresh must truncate "
    f"semantic-fork-log.jsonl exactly like it truncates metrics.jsonl/config-history.jsonl, or "
    f"entries accumulate across repeated fresh runs into the same directory "
    f"(lines={fork_log_lines!r})"
)

# Not just the right COUNT - the right CONTENT: this sole entry must be a well-formed fork
# record for the current run's own explicit --replay-cause-balance-cap 0.25 override (same
# format test_native_alphazero_cause_balance_fork_legacy_check.py checks for the first run).
entry = json.loads(fork_log_lines[0])
matching = [fork for fork in entry["forks"] if fork.startswith("replay_cause_balance_cap:")]
assert matching == ["replay_cause_balance_cap: checkpoint=0 -> explicit CLI=0.25"], (
    f"unexpected forks in the sole post-rerun semantic-fork-log.jsonl entry: {entry['forks']}"
)

# Sanity re-check of the pre-existing half of --fresh's truncation block (metrics.jsonl/
# config-history.jsonl) against this same rerun, so a regression there would surface here too
# rather than only in tests that happen to run a fresh command exactly once into a directory.
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 1, (
    f"expected exactly one metrics.jsonl row after the second --fresh run, got {len(rows)}"
)
config_history = (run_dir / "config-history.jsonl").read_text().splitlines()
assert len(config_history) == 1, (
    f"expected exactly one config-history.jsonl row after the second --fresh run, got "
    f"{len(config_history)}"
)

print("Native AlphaZero --fresh semantic-fork-log.jsonl truncation validated "
      f"(exactly 1 entry survives a second --fresh --fork-from run into the same directory, "
      f"forks={entry['forks']})")
