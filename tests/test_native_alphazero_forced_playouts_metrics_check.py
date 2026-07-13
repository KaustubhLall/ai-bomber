"""v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md): forced-playouts
telemetry must be well-formed on every metrics.jsonl row, and must show the mechanism actually
DOING something across this fixture's three train invocations (all pass --forced-playouts-k>0,
--eval-interval 1, --simulations 32 - see tests/CMakeLists.txt's comment on why 32, not this
file's usual 2, is needed for the floor invariant to have room to converge).

Two properties from the v7 Stage 0 test plan:
  (3) Forcing floor property: mean forced playouts per move > 0 somewhere (forcing actually
      fired), AND forced_floor_violations == 0 on EVERY row (the always-on invariant telemetry -
      BatchedMcts::root_floor_violations() in trainer.cpp - expected to never fire when forcing
      is active).
  (4) Pruning property: mean_pruned_visit_fraction stays in [0,1] on every row, and is > 0 on at
      least one row (noise makes some forcing, and therefore some pruning, near-certain across
      three whole iterations of collection).

Reuses the forced-playouts semantic fixture's run-dir rather than a dedicated one - the same
reuse pattern test_native_alphazero_drift_canary_check.py established for the temperature-anneal
fixture."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 3, (
    f"expected 3 metrics.jsonl rows (one per completed iteration across the fresh/resume/"
    f"override chain), got {len(rows)}"
)

mean_forced_values: list[float] = []
mean_pruned_values: list[float] = []
total_floor_violations = 0
total_moves = 0

for row in rows:
    iteration = row.get("iteration")
    forced_playouts = row.get("forced_playouts")
    assert forced_playouts is not None, (
        f"iteration {iteration}: missing 'forced_playouts' - every row should carry this block "
        f"once config.forced_playouts_k>0 (this fixture's whole chain runs with k=2 or k=3)"
    )

    mean_forced = forced_playouts["mean_forced_per_move"]
    assert isinstance(mean_forced, (int, float)) and not isinstance(mean_forced, bool), (
        f"iteration {iteration}: forced_playouts.mean_forced_per_move={mean_forced!r} is not "
        f"numeric"
    )
    assert mean_forced >= 0.0, (
        f"iteration {iteration}: forced_playouts.mean_forced_per_move={mean_forced} is negative"
    )
    mean_forced_values.append(float(mean_forced))

    mean_pruned = forced_playouts["mean_pruned_visit_fraction"]
    assert isinstance(mean_pruned, (int, float)) and not isinstance(mean_pruned, bool), (
        f"iteration {iteration}: forced_playouts.mean_pruned_visit_fraction={mean_pruned!r} is "
        f"not numeric"
    )
    assert 0.0 <= float(mean_pruned) <= 1.0, (
        f"iteration {iteration}: forced_playouts.mean_pruned_visit_fraction={mean_pruned} "
        f"outside [0,1] - this is a fraction of visits removed by pruning, must be a valid "
        f"fraction"
    )
    mean_pruned_values.append(float(mean_pruned))

    violations = forced_playouts["forced_floor_violations"]
    assert isinstance(violations, int) and not isinstance(violations, bool), (
        f"iteration {iteration}: forced_playouts.forced_floor_violations={violations!r} is not "
        f"an int"
    )
    assert violations == 0, (
        f"iteration {iteration}: forced_playouts.forced_floor_violations={violations}, expected "
        f"0 - BatchedMcts::root_floor_violations() found a root action with non-negligible "
        f"post-noise prior whose final marginal visits fell (by more than the documented "
        f"one-simulation slack) below its forced-playouts floor; this is a real invariant "
        f"failure, not noise"
    )
    total_floor_violations += violations

    moves = forced_playouts["moves"]
    assert isinstance(moves, int) and not isinstance(moves, bool) and moves >= 0, (
        f"iteration {iteration}: forced_playouts.moves={moves!r} is not a non-negative int"
    )
    total_moves += moves

assert total_moves > 0, (
    "expected at least one seat-move recorded across the whole fixture chain (moves=0 on every "
    "row would mean the collection loop never ran, which would also make every other assertion "
    "here vacuous)"
)
assert total_floor_violations == 0, (
    f"expected 0 total forced_floor_violations across all {len(rows)} rows, got "
    f"{total_floor_violations}"
)
assert any(value > 0.0 for value in mean_forced_values), (
    f"expected mean_forced_per_move > 0 on at least one of the {len(rows)} rows (forcing never "
    f"fired at all across the whole fixture chain, despite forced_playouts_k in {{2, 3}} and "
    f"live Dirichlet noise): {mean_forced_values}"
)
assert any(value > 0.0 for value in mean_pruned_values), (
    f"expected mean_pruned_visit_fraction > 0 on at least one of the {len(rows)} rows (pruning "
    f"never removed any visits across the whole fixture chain): {mean_pruned_values}"
)

print("Native AlphaZero forced-playouts metrics validated across "
      f"{len(rows)} rows (total_moves={total_moves}, forced_floor_violations=0, "
      f"mean_forced_per_move={['%.3f' % v for v in mean_forced_values]}, "
      f"mean_pruned_visit_fraction={['%.3f' % v for v in mean_pruned_values]})")
