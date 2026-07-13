"""KL-105 Phase 2b / KL-110 Phase B regression: the `gates` subcommand's write-once JSON evidence
must be well-formed, carry full provenance, cover all six scenarios in both search and raw mode,
and be BYTE-FOR-BYTE deterministic across repeated runs against the same checkpoint (fixed seeds,
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
failure mode this gate exists to probe. Crush/self-kill deaths still do not count.

KL-110 Phase B adds: gates_format_version 2, a top-level search_opponent_model field, per-step
search telemetry (search mode only, both "self" and "aligned"), a scenario-level
opponent_model_alignment label, and a fixed_opponent_internal_violations structural-proof count.
"self" (--gates-opponent-model self, or the flag omitted - same default) must reproduce the
pre-KL-110 behavior exactly: this file checks that against a THIRD run (gates-self-explicit.json)
independently of the pre-existing gates_run/gates_rerun determinism pair, since determinism
(same flags, same output) and backward compatibility (different flags spelling the same
behavior, same output) are different claims - see doc 13 amendment discipline. Note the scenario
count below: the KL-110 issue text says "the four NONE/CONSTANT scenarios" for action_exact
alignment, but the actual scenario table (doc 13 section 2) has FOUR NONE scenarios
(bomb-and-escape, corridor-clear, trap, flame-timing) PLUS ONE CONSTANT scenario (stall-break) =
FIVE action_exact scenarios, not four; verified empirically against the real binary before
pinning ALIGNED_ALIGNMENT_KIND below. Flagged for the planner, not silently "corrected" in the
spec text - see the KL-110 executor's closeout summary."""

from __future__ import annotations

import json
import sys
from pathlib import Path

SCENARIO_NAMES = [
    "bomb-and-escape", "corridor-clear", "trap", "chase", "flame-timing", "stall-break",
]

ACTIONS = 6
ACTION_UP = 0
ACTION_WAIT = 5

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

# KL-110 Phase B: aligned-mode opponent_model_alignment per scenario - "action_exact" for
# NONE/CONSTANT (the real opponent action is a single fixed value search can pin exactly),
# "type_aligned" for AGENT (search can only match the baseline TYPE, not the live scripted
# agent's own state/RNG trajectory) - see gate_opponent_alignment_kind() in trainer.cpp.
ALIGNED_ALIGNMENT_KIND = {
    "bomb-and-escape": "action_exact",
    "corridor-clear": "action_exact",
    "trap": "action_exact",
    "chase": "type_aligned",
    "flame-timing": "action_exact",
    "stall-break": "action_exact",
}

# KL-110 Phase B: the exact fixed action aligned mode pins for each NONE/CONSTANT scenario -
# mirrors gate_search_constraint()'s kNone (ACTION_WAIT) / kConstant (the scenario's own
# opponent_constant_action) mapping. chase (kAgent) is deliberately excluded - there is no
# single fixed action to one-hot against for the enforcement/one-hot checks below.
FIXED_OPPONENT_ACTION = {
    "bomb-and-escape": ACTION_WAIT,
    "corridor-clear": ACTION_WAIT,
    "trap": ACTION_WAIT,
    "flame-timing": ACTION_WAIT,
    "stall-break": ACTION_UP,
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


def check_step_records(search_result: dict, context: str) -> None:
    """KL-110 Phase B: search-mode-only per-step telemetry. One record per EXECUTED step
    (steps_used), present in BOTH opponent models (self and aligned) - this function is called
    on every "search" result regardless of --gates-opponent-model."""
    assert isinstance(search_result["fixed_opponent_internal_violations"], int), context
    assert search_result["fixed_opponent_internal_violations"] == 0, (
        f"{context}: fixed_opponent_internal_violations must be structurally 0 (expand_and_backup "
        f"one-hots the fixed seat at every node it expands, root or interior alike), got "
        f"{search_result['fixed_opponent_internal_violations']}"
    )
    steps = search_result["steps"]
    assert isinstance(steps, list) and len(steps) == search_result["steps_used"], (
        f"{context}: expected {search_result['steps_used']} step records (one per executed "
        f"step), got {len(steps)}"
    )
    for index, step in enumerate(steps):
        step_context = f"{context} step {index}"
        assert step["step"] == index, f"{step_context}: step field is {step['step']}, expected {index}"
        assert isinstance(step["chosen"], int) and 0 <= step["chosen"] < ACTIONS, step_context
        assert isinstance(step["opponent_executed_action"], int), step_context
        assert 0 <= step["opponent_executed_action"] < ACTIONS, step_context
        for key in ("prior_after_safety_mask_marginal", "visit_marginal", "root_q",
                    "opp_visit_marginal"):
            value = step.get(key)
            assert isinstance(value, list) and len(value) == ACTIONS, (
                f"{step_context}: {key} must be a length-{ACTIONS} array, got {value!r}"
            )


def check_alignment_kinds(evidence: dict, label: str, mode: str) -> None:
    for scenario in evidence["scenarios"]:
        expected = "self" if mode == "self" else ALIGNED_ALIGNMENT_KIND[scenario["name"]]
        actual = scenario["opponent_model_alignment"]
        assert actual == expected, (
            f"{label}/{scenario['name']}: expected opponent_model_alignment={expected!r}, "
            f"got {actual!r}"
        )


def check_one_hot_enforcement(evidence: dict, label: str) -> None:
    """KL-110 Phase B: on an ALIGNED run, every NONE/CONSTANT scenario's opp_visit_marginal
    (the OPPONENT seat's search-visit marginal) must put 100% of visits on the scenario's fixed
    action, at every recorded step - the one-hot enforcement expand_and_backup performs. Paired
    with the internal-node fixed_opponent_internal_violations==0 check (already asserted by
    check_step_records for every scenario) as the two halves of the same structural proof: one
    from the root's own SearchResult, the other from a full tree traversal."""
    scenarios = {s["name"]: s for s in evidence["scenarios"]}
    for name, fixed_action in FIXED_OPPONENT_ACTION.items():
        search = scenarios[name]["search"]
        for step in search["steps"]:
            marginal = step["opp_visit_marginal"]
            total = sum(marginal)
            step_context = f"{label}/{name} step {step['step']}"
            assert total > 0, f"{step_context}: opp_visit_marginal is all-zero ({marginal})"
            assert marginal[fixed_action] == total, (
                f"{step_context}: opp_visit_marginal is not 100% on the fixed action "
                f"{fixed_action} - got {marginal} (total visits {total})"
            )


def check_provenance(evidence: dict, label: str, *, expected_opponent_model: str) -> None:
    assert evidence["gates_format_version"] == 2, (
        f"{label}: expected gates_format_version 2, got {evidence.get('gates_format_version')!r}"
    )
    assert evidence["search_opponent_model"] == expected_opponent_model, (
        f"{label}: expected search_opponent_model={expected_opponent_model!r}, got "
        f"{evidence.get('search_opponent_model')!r}"
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
            assert "opponent_model_alignment" not in scenario, (
                f"{label}: agent-mode scenario should not carry opponent_model_alignment "
                f"(no BatchedMcts search runs in agent mode): {scenario}"
            )
            check_result_shape(scenario["result"], f"{label}/{scenario['name']}/result")
        else:
            assert "search" in scenario and "raw" in scenario, (
                f"{label}: network-mode scenario missing search/raw: {scenario}"
            )
            assert "result" not in scenario, (
                f"{label}: network-mode scenario should not carry a bare 'result': {scenario}"
            )
            assert "opponent_model_alignment" in scenario, (
                f"{label}: network-mode scenario missing opponent_model_alignment: {scenario}"
            )
            check_result_shape(scenario["search"], f"{label}/{scenario['name']}/search")
            check_result_shape(scenario["raw"], f"{label}/{scenario['name']}/raw")
            # KL-110 Phase B: per-step search telemetry is search-mode ONLY - raw never calls
            # BatchedMcts::search, so it must never carry steps/fixed_opponent_internal_violations.
            check_step_records(scenario["search"], f"{label}/{scenario['name']}/search")
            assert "steps" not in scenario["raw"], (
                f"{label}/{scenario['name']}/raw: raw mode must not carry per-step search "
                f"telemetry (it never calls BatchedMcts::search): {scenario['raw']}"
            )
            assert "fixed_opponent_internal_violations" not in scenario["raw"], (
                f"{label}/{scenario['name']}/raw: raw mode must not carry "
                f"fixed_opponent_internal_violations: {scenario['raw']}"
            )


run_dir = Path(sys.argv[1])
run_a = json.loads((run_dir / "gates-network-1.json").read_text())
run_b = json.loads((run_dir / "gates-network-2.json").read_text())
agent_run = json.loads((run_dir / "gates-heuristic.json").read_text())
self_explicit = json.loads((run_dir / "gates-self-explicit.json").read_text())
aligned_1 = json.loads((run_dir / "gates-aligned-1.json").read_text())
aligned_2 = json.loads((run_dir / "gates-aligned-2.json").read_text())

for evidence, label in ((run_a, "network run 1 (no flag)"), (run_b, "network run 2 (no flag)")):
    check_provenance(evidence, label, expected_opponent_model="self")
    assert evidence["mode"] == "network", f"{label}: expected mode=='network', got {evidence['mode']!r}"
    check_scenarios_present(evidence, label, agent_mode=False)
    check_alignment_kinds(evidence, label, mode="self")

check_provenance(agent_run, "heuristic agent run", expected_opponent_model="self")
assert agent_run["mode"] == "heuristic", (
    f"heuristic agent run: expected mode=='heuristic', got {agent_run['mode']!r}"
)
check_scenarios_present(agent_run, "heuristic agent run", agent_mode=True)

check_provenance(self_explicit, "self (explicit flag)", expected_opponent_model="self")
assert self_explicit["mode"] == "network", self_explicit["mode"]
check_scenarios_present(self_explicit, "self (explicit flag)", agent_mode=False)
check_alignment_kinds(self_explicit, "self (explicit flag)", mode="self")

for evidence, label in ((aligned_1, "aligned run 1"), (aligned_2, "aligned run 2")):
    check_provenance(evidence, label, expected_opponent_model="aligned")
    assert evidence["mode"] == "network", f"{label}: expected mode=='network', got {evidence['mode']!r}"
    check_scenarios_present(evidence, label, agent_mode=False)
    check_alignment_kinds(evidence, label, mode="aligned")
    check_one_hot_enforcement(evidence, label)

# Determinism (doc 13 section 2): identical checkpoint, fixed per-scenario seeds, greedy
# selection, no dirichlet noise - two independent `gates` invocations against the same
# checkpoint must produce IDENTICAL scenario outcomes. Compare the scenarios arrays directly
# (provenance fields like generated_at_utc/invocation_argv legitimately differ run-to-run and
# are checked separately above, not part of the determinism claim).
assert run_a["scenarios"] == run_b["scenarios"], (
    "gates is not deterministic in self mode: two runs against the identical checkpoint with "
    "identical flags produced different scenario outcomes:\n"
    f"  run 1: {run_a['scenarios']}\n"
    f"  run 2: {run_b['scenarios']}"
)
assert aligned_1["scenarios"] == aligned_2["scenarios"], (
    "gates is not deterministic in aligned mode: two --gates-opponent-model aligned runs "
    "against the identical checkpoint produced different scenario outcomes:\n"
    f"  aligned run 1: {aligned_1['scenarios']}\n"
    f"  aligned run 2: {aligned_2['scenarios']}"
)

# KL-110 Phase B backward compatibility: --gates-opponent-model self (explicit) must produce the
# exact same scenarios array as the no-flag default - NOT the same claim as rerun-determinism
# above (which reruns with IDENTICAL argv). This proves the default truly IS "self", not a
# separate code path that happens to usually agree with it.
assert run_a["scenarios"] == self_explicit["scenarios"], (
    "--gates-opponent-model self (explicit) does not byte-match the no-flag default - the "
    "default is supposed to BE self, not a separately-behaving path:\n"
    f"  no-flag default: {run_a['scenarios']}\n"
    f"  explicit self:   {self_explicit['scenarios']}"
)

assert (run_a["checkpoint_sha256"] == run_b["checkpoint_sha256"] == agent_run["checkpoint_sha256"]
        == self_explicit["checkpoint_sha256"] == aligned_1["checkpoint_sha256"]
        == aligned_2["checkpoint_sha256"])
assert (run_a["executable_sha256"] == run_b["executable_sha256"] == agent_run["executable_sha256"]
        == self_explicit["executable_sha256"] == aligned_1["executable_sha256"]
        == aligned_2["executable_sha256"])
assert run_a["resolved_semantics"] == run_b["resolved_semantics"] == self_explicit["resolved_semantics"]

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
    "Native AlphaZero KL-105/KL-110 tactical gates validated: "
    f"provenance + all 6 scenarios x both modes well-formed on 2 self-mode network runs, "
    f"1 explicit-self run, and 2 aligned runs; byte-identical determinism confirmed in both "
    f"self and aligned mode; self (explicit) byte-matches the no-flag default; one-hot "
    f"enforcement + zero internal violations confirmed on every NONE/CONSTANT scenario's "
    f"aligned run; alignment kinds correct; heuristic achievability reference passed "
    f"{passed_count}/6 scenarios matching the pinned pattern "
    f"({sorted(name for name, passed in actual_heuristic_pass.items() if passed)})"
)
