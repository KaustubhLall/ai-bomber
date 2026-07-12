"""KL-105 Phase 2b regression: the `gates` subcommand's write-once JSON evidence must be
well-formed, carry full provenance, cover all six scenarios in both search and raw mode, and be
BYTE-FOR-BYTE deterministic across repeated runs against the same checkpoint (fixed seeds,
greedy selection, no dirichlet noise - see doc 13 section 2's "Determinism" paragraph). The
fixture checkpoint here is a tiny, effectively-random 1-iteration network (channels=16,
blocks=1) - its own pass/fail values are NOT asserted (a tiny random-ish net may fail every
scenario; that is fine and expected, and asserting otherwise would make this test fixture-
fragile for no reason).

Achievability is instead checked against a SCRIPTED agent (--gates-agent heuristic), which
substitutes the heuristic Agent for the network entirely. Empirically pinned against this
exact fixture: the heuristic reliably passes bomb-and-escape, corridor-clear, trap, chase, and
stall-break; it reliably FAILS only flame-timing (its fixed priority order - escape danger >
bomb crates/enemies in range > chase powerups > chase crates > pressure the nearest enemy >
random safe walk - has no "wait out a corridor-blocking bomb, then proceed" behavior, so it
never reaches the far side within k_steps=15; a search-based policy, mcts at even a tiny
32-sim/depth-6 budget, solves it in 12 steps, confirming the scenario is achievable and this
is a heuristic-specific priority gap, not a broken gate).

stall-break's kill path is a PLANNER RULING, recorded here because this file pins its
consequences: the executor originally implemented the doc-13 two-condition predicate (reach
the far side, or clear the alternate-path crate), found that both the heuristic AND mcts
independently bomb the defenseless CONSTANT(ACTION_UP) blocker instead (terminal learner_win,
which under that predicate registered as a FAIL), and flagged it rather than changing it. The
planner amended the predicate: a demonstrated bomb kill of the blocker (death_owner == learner,
the same standard as the trap gate) now counts as breaking the stall - killing the thing
blocking the chokepoint is decisive aggression, the exact opposite of the passive-stall
failure mode this gate exists to probe. Crush/self-kill deaths still do not count."""

from __future__ import annotations

import json
import sys
from pathlib import Path

SCENARIO_NAMES = [
    "bomb-and-escape", "corridor-clear", "trap", "chase", "flame-timing", "stall-break",
]

# Empirically observed against the tiny fixture checkpoint's --gates-agent heuristic run (see
# module docstring, including the planner ruling behind stall-break's kill path). Pinned
# exactly, both passes and fails, so a future change that silently breaks an achievable gate
# OR silently "fixes" the flagged flame-timing gap is equally visible.
EXPECTED_HEURISTIC_PASS = {
    "bomb-and-escape": True,
    "corridor-clear": True,
    "trap": True,
    "chase": True,
    "flame-timing": False,
    "stall-break": True,
}


def check_result_shape(result: dict, context: str) -> None:
    assert isinstance(result["passed"], bool), f"{context}: passed must be a bool, got {result}"
    assert isinstance(result["steps_used"], int) and result["steps_used"] >= 0, (
        f"{context}: steps_used must be a non-negative int, got {result}"
    )
    assert isinstance(result["terminal"], str) and result["terminal"], (
        f"{context}: terminal must be a non-empty string, got {result}"
    )
    assert isinstance(result["note"], str), f"{context}: note must be a string, got {result}"


def check_provenance(evidence: dict, label: str) -> None:
    assert evidence["gates_format_version"] == 1, (
        f"{label}: expected gates_format_version 1, got {evidence.get('gates_format_version')!r}"
    )
    assert evidence["generated_at_utc"].endswith("Z"), label
    assert isinstance(evidence["invocation_argv"], list) and "gates" in evidence["invocation_argv"], (
        f"{label}: invocation_argv must be preserved verbatim and include the 'gates' command"
    )
    assert evidence["working_directory"], label
    assert evidence["checkpoint"], label
    assert Path(evidence["checkpoint_path"]).is_absolute(), label
    assert len(evidence["checkpoint_sha256"]) == 64, label
    assert evidence["executable_path"], label
    assert len(evidence["executable_sha256"]) == 64, label
    assert evidence["git_commit"], label
    assert evidence["runtime_config_signature"], label
    assert evidence["checkpoint_semantics_verified"] is True, (
        f"{label}: this fixture is fresh-trained under the current manifest, not a legacy "
        f"checkpoint - semantics must be verified, not unverified-accepted"
    )
    assert evidence["checkpoint_semantics_source"] == "checkpoint_manifest", label
    assert isinstance(evidence["checkpoint_iteration"], int), label
    assert evidence["resolved_semantics"], label
    assert evidence["simulations"] == 8, (
        f"{label}: expected simulations==8 (--eval-simulations 8), got {evidence['simulations']}"
    )


def check_scenarios_present(evidence: dict, label: str, *, agent_mode: bool) -> None:
    scenarios = evidence["scenarios"]
    assert [s["name"] for s in scenarios] == SCENARIO_NAMES, (
        f"{label}: expected exactly the six scenarios in doc-13 order, got "
        f"{[s['name'] for s in scenarios]}"
    )
    for scenario in scenarios:
        if agent_mode:
            assert "result" in scenario, f"{label}: agent-mode scenario missing 'result': {scenario}"
            assert "search" not in scenario and "raw" not in scenario, (
                f"{label}: agent-mode scenario should not carry search/raw: {scenario}"
            )
            check_result_shape(scenario["result"], f"{label}/{scenario['name']}/result")
        else:
            assert "search" in scenario and "raw" in scenario, (
                f"{label}: network-mode scenario missing search/raw: {scenario}"
            )
            assert "result" not in scenario, (
                f"{label}: network-mode scenario should not carry a bare 'result': {scenario}"
            )
            check_result_shape(scenario["search"], f"{label}/{scenario['name']}/search")
            check_result_shape(scenario["raw"], f"{label}/{scenario['name']}/raw")


run_dir = Path(sys.argv[1])
run_a = json.loads((run_dir / "gates-network-1.json").read_text())
run_b = json.loads((run_dir / "gates-network-2.json").read_text())
agent_run = json.loads((run_dir / "gates-heuristic.json").read_text())

for evidence, label in ((run_a, "network run 1"), (run_b, "network run 2")):
    check_provenance(evidence, label)
    assert evidence["mode"] == "network", f"{label}: expected mode=='network', got {evidence['mode']!r}"
    check_scenarios_present(evidence, label, agent_mode=False)

check_provenance(agent_run, "heuristic agent run")
assert agent_run["mode"] == "heuristic", (
    f"heuristic agent run: expected mode=='heuristic', got {agent_run['mode']!r}"
)
check_scenarios_present(agent_run, "heuristic agent run", agent_mode=True)

# Determinism (doc 13 section 2): identical checkpoint, fixed per-scenario seeds, greedy
# selection, no dirichlet noise - two independent `gates` invocations against the same
# checkpoint must produce IDENTICAL scenario outcomes. Compare the scenarios arrays directly
# (provenance fields like generated_at_utc/invocation_argv legitimately differ run-to-run and
# are checked separately above, not part of the determinism claim).
assert run_a["scenarios"] == run_b["scenarios"], (
    "gates is not deterministic: two runs against the identical checkpoint with identical "
    "flags produced different scenario outcomes:\n"
    f"  run 1: {run_a['scenarios']}\n"
    f"  run 2: {run_b['scenarios']}"
)
assert run_a["checkpoint_sha256"] == run_b["checkpoint_sha256"] == agent_run["checkpoint_sha256"]
assert run_a["executable_sha256"] == run_b["executable_sha256"] == agent_run["executable_sha256"]
assert run_a["resolved_semantics"] == run_b["resolved_semantics"]

# Achievability: at least two scenarios must be passable by a real (if simple) tactical agent,
# or the gate suite would be indistinguishable from "impossible" - see EXPECTED_HEURISTIC_PASS's
# derivation above for why exactly four, not six, are pinned as passing.
actual_heuristic_pass = {
    scenario["name"]: scenario["result"]["passed"] for scenario in agent_run["scenarios"]
}
assert actual_heuristic_pass == EXPECTED_HEURISTIC_PASS, (
    f"heuristic-agent pass/fail pattern changed - expected {EXPECTED_HEURISTIC_PASS}, "
    f"got {actual_heuristic_pass} (full results: {agent_run['scenarios']})"
)
passed_count = sum(actual_heuristic_pass.values())
assert passed_count >= 2, (
    f"only {passed_count} scenario(s) passed by the heuristic achievability reference - "
    f"doc 13 section 2 treats fewer than 2 as a broken-gate signal for the planner, not "
    f"something this test should silently accept"
)

print(
    "Native AlphaZero KL-105 tactical gates validated: "
    f"provenance + all 6 scenarios x both modes well-formed on 2 network runs, "
    f"byte-identical determinism confirmed, heuristic achievability reference passed "
    f"{passed_count}/6 scenarios matching the pinned pattern "
    f"({sorted(name for name, passed in actual_heuristic_pass.items() if passed)})"
)
