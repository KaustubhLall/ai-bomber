"""v7 Stage 0 item 0.5 (docs/experiment-memory/14-v7-from-scratch-design.md): the in-training
drift canary must fire at every evaluation_interval and record a well-formed "gates" object into
metrics.jsonl - passed_search (how many of the six tactical gates passed, in search mode), the
trap scenario's step-0 BOMB prior (the H3a starvation signal doc 14 names as the thing that "must
never be invisible again"), and a per-scenario pass/fail breakdown for all six scenarios.

Reuses the temperature-anneal fixture's own three-invocation chain (all three pass
--eval-interval 1, so every one of the resulting metrics.jsonl rows should carry a "gates"
object) rather than a dedicated fixture - this is a read of the SAME metrics.jsonl the
temperature-anneal check reads config-history.jsonl/config.json/semantic-fork-log.jsonl from, so
no extra GPU-time fixture is needed for this concern."""

from __future__ import annotations

import json
import sys
from pathlib import Path

EXPECTED_SCENARIOS = {
    "bomb-and-escape", "corridor-clear", "trap", "chase", "flame-timing", "stall-break",
}

run_dir = Path(sys.argv[1])
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert rows, "expected at least one metrics.jsonl row"

canary_seconds: list[float] = []
for row in rows:
    iteration = row.get("iteration")
    gates = row.get("gates")
    assert gates is not None, (
        f"iteration {iteration}: missing 'gates' - the canary should fire every eval interval "
        f"at this fixture's --eval-interval 1"
    )

    passed = gates["passed_search"]
    assert isinstance(passed, int) and not isinstance(passed, bool), (
        f"iteration {iteration}: gates.passed_search={passed!r} is not an int"
    )
    assert 0 <= passed <= 6, (
        f"iteration {iteration}: gates.passed_search={passed} outside [0,6] (six scenarios total)"
    )

    prior = gates["trap_bomb_prior_step0"]
    assert isinstance(prior, (int, float)) and not isinstance(prior, bool), (
        f"iteration {iteration}: gates.trap_bomb_prior_step0={prior!r} is not numeric"
    )
    assert 0.0 <= float(prior) <= 1.0, (
        f"iteration {iteration}: gates.trap_bomb_prior_step0={prior} outside [0,1] - this is a "
        f"safety-masked marginal probability, must be a valid probability"
    )

    per_scenario = gates["per_scenario"]
    assert set(per_scenario.keys()) == EXPECTED_SCENARIOS, (
        f"iteration {iteration}: gates.per_scenario keys {sorted(per_scenario)} != "
        f"{sorted(EXPECTED_SCENARIOS)} - build_gate_scenarios() defines exactly six scenarios"
    )
    for name, value in per_scenario.items():
        assert isinstance(value, bool), (
            f"iteration {iteration}: gates.per_scenario[{name!r}]={value!r} is not a bool"
        )
    actual_passed_count = sum(1 for value in per_scenario.values() if value)
    assert actual_passed_count == passed, (
        f"iteration {iteration}: gates.passed_search={passed} does not match the count of "
        f"True values in per_scenario ({actual_passed_count}) - {per_scenario}"
    )

    seconds = row["phase_timings"]["drift_canary_seconds"]
    assert isinstance(seconds, (int, float)) and seconds >= 0.0, (
        f"iteration {iteration}: phase_timings.drift_canary_seconds={seconds!r} must be a "
        f"non-negative number"
    )
    canary_seconds.append(float(seconds))

mean_seconds = sum(canary_seconds) / len(canary_seconds)
print("Native AlphaZero drift-canary metrics shape validated across "
      f"{len(rows)} eval-interval rows (passed_search/trap_bomb_prior_step0/per_scenario all "
      f"well-formed; mean drift_canary_seconds={mean_seconds:.3f}, "
      f"per-row={['%.3f' % s for s in canary_seconds]})")
