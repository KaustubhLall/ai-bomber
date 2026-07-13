"""KL-110 Phase B: unit coverage for tools/analyze_h3a_verdict.py's verdict logic, using
synthetic mini gates-evidence dicts built in-test (no native binary, no GPU - pure Python,
mirrors the tests/test_native_run_analysis.py pattern of importing the tool's functions
directly rather than shelling out, so failures point at the exact broken predicate instead of a
diffed stdout blob). Covers the four cases the KL-110 spec names explicitly (rescue, no-rescue,
mixed-one-restoration, mixed-restoration-without-visit-lead) plus additional defense-in-depth
cases for the corridor-regression veto, the non-deciding descriptive checkpoint, and the
provenance guardrails - then one end-to-end pass through the real CLI (argparse + file I/O +
--output) to prove the plumbing around the pure logic also works, not just compute_verdict()
in isolation."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.analyze_h3a_verdict import (  # noqa: E402
    BOMB,
    ACTIONS,
    REQUIRED_CHECKPOINTS,
    argmax,
    compute_verdict,
    parse_label,
)

CONTROL03, CONTROL154, TREATMENT154 = REQUIRED_CHECKPOINTS
LEVER2 = "lever2-160"

# BOMB=4, WAIT=5 (see trainer.h's Action enum via kActions/ACTION_PLACE_BOMB). A "visit lead"
# step has BOMB as the visit_marginal argmax; a "no visit lead" step gives a movement action
# (RIGHT=3) more visits than BOMB despite BOMB still being chosen that step (PUCT can execute a
# low-visit action near the root - exactly the disagreement this tool exists to catch).
VISIT_LEAD = [0, 0, 0, 0, 6, 2]
NO_VISIT_LEAD = [0, 0, 0, 6, 2, 0]
assert argmax(VISIT_LEAD) == BOMB
assert argmax(NO_VISIT_LEAD) != BOMB


def bomb_step(step: int, visit_marginal: list[int], *, root_q: list[float] | None = None) -> dict:
    return {
        "step": step,
        "chosen": BOMB,
        "opponent_executed_action": 5,
        "prior_after_safety_mask_marginal": [1.0 / ACTIONS] * ACTIONS,
        "visit_marginal": visit_marginal,
        "root_q": root_q or [0.0] * ACTIONS,
        "opp_visit_marginal": [0] * ACTIONS,
    }


def make_evidence(*, checkpoint_sha="ckpt-x", executable_sha="exe-x",
                   trap_pass: bool, corridor_pass: bool = True,
                   trap_steps: list[dict] | None = None) -> dict:
    return {
        "gates_format_version": 2,
        "checkpoint_sha256": checkpoint_sha,
        "executable_sha256": executable_sha,
        "scenarios": [
            {"name": "trap", "search": {"passed": trap_pass, "steps": trap_steps or []}},
            {"name": "corridor-clear", "search": {"passed": corridor_pass, "steps": []}},
        ],
    }


def restoration_pair(*, checkpoint_sha="ckpt-x", executable_sha="exe-x",
                      visit_lead: bool = True, corridor_regression: bool = False) -> dict:
    """A checkpoint's {"self", "aligned"} pair where the trap gate is a restoration candidate
    (self FAIL, aligned PASS), with the bomb-placement step's visit lead controllable."""
    self_evidence = make_evidence(checkpoint_sha=checkpoint_sha, executable_sha=executable_sha,
                                  trap_pass=False,
                                  corridor_pass=True)
    aligned_evidence = make_evidence(
        checkpoint_sha=checkpoint_sha, executable_sha=executable_sha, trap_pass=True,
        corridor_pass=not corridor_regression,
        trap_steps=[bomb_step(3, VISIT_LEAD if visit_lead else NO_VISIT_LEAD)])
    return {"self": self_evidence, "aligned": aligned_evidence}


def non_candidate_pair(*, checkpoint_sha="ckpt-x", executable_sha="exe-x") -> dict:
    """A checkpoint that already passes trap in self mode - never a restoration candidate."""
    self_evidence = make_evidence(checkpoint_sha=checkpoint_sha, executable_sha=executable_sha,
                                  trap_pass=True)
    aligned_evidence = make_evidence(checkpoint_sha=checkpoint_sha, executable_sha=executable_sha,
                                     trap_pass=True)
    return {"self": self_evidence, "aligned": aligned_evidence}


# --- Case 1: rescue - all 3 required checkpoints restore with a genuine visit lead ---
rescue_input = {name: restoration_pair(checkpoint_sha=name, executable_sha=name)
                for name in REQUIRED_CHECKPOINTS}
rescue = compute_verdict(rescue_input)
assert rescue["verdict"] == "H3a_rescue", rescue
assert sorted(rescue["counted_restorations"]) == sorted(REQUIRED_CHECKPOINTS), rescue
assert rescue["restoration_candidates_without_visit_lead"] == [], rescue
assert rescue["corridor_regressions"] == [], rescue

# --- Case 2: no-rescue - trap already passes in self mode everywhere, nothing to restore ---
no_rescue_input = {name: non_candidate_pair(checkpoint_sha=name, executable_sha=name)
                    for name in REQUIRED_CHECKPOINTS}
no_rescue = compute_verdict(no_rescue_input)
assert no_rescue["verdict"] == "H3a_no_rescue", no_rescue
assert no_rescue["counted_restorations"] == [], no_rescue
assert no_rescue["restoration_candidates_without_visit_lead"] == [], no_rescue

# --- Case 3: mixed (exactly one counted restoration) ---
one_restoration_input = {
    CONTROL03: restoration_pair(checkpoint_sha=CONTROL03, executable_sha=CONTROL03),
    CONTROL154: non_candidate_pair(checkpoint_sha=CONTROL154, executable_sha=CONTROL154),
    TREATMENT154: non_candidate_pair(checkpoint_sha=TREATMENT154, executable_sha=TREATMENT154),
}
mixed_one = compute_verdict(one_restoration_input)
assert mixed_one["verdict"] == "mixed", mixed_one
assert mixed_one["counted_restorations"] == [CONTROL03], mixed_one
assert mixed_one["restoration_candidates_without_visit_lead"] == [], mixed_one

# --- Case 4: mixed (restoration without the visit lead) - the override must fire even though
# the OTHER two required checkpoints are clean counted restorations, which would otherwise tally
# to >=2 with no corridor regression and read as H3a_rescue. This is the case that specifically
# proves the disagreement check is an unconditional override, not just a low-count fallthrough. */
no_lead_input = {
    CONTROL03: restoration_pair(checkpoint_sha=CONTROL03, executable_sha=CONTROL03,
                                visit_lead=False),
    CONTROL154: restoration_pair(checkpoint_sha=CONTROL154, executable_sha=CONTROL154),
    TREATMENT154: restoration_pair(checkpoint_sha=TREATMENT154, executable_sha=TREATMENT154),
}
mixed_no_lead = compute_verdict(no_lead_input)
assert mixed_no_lead["verdict"] == "mixed", mixed_no_lead
assert mixed_no_lead["restoration_candidates_without_visit_lead"] == [CONTROL03], mixed_no_lead
assert sorted(mixed_no_lead["counted_restorations"]) == sorted([CONTROL154, TREATMENT154]), (
    mixed_no_lead
)  # 2 clean counted restorations - proves this isn't just "mixed because count < 2"

# --- Bonus: corridor regression alongside >=2 counted restorations forces mixed too ---
corridor_input = {
    CONTROL03: restoration_pair(checkpoint_sha=CONTROL03, executable_sha=CONTROL03,
                                corridor_regression=True),
    CONTROL154: restoration_pair(checkpoint_sha=CONTROL154, executable_sha=CONTROL154),
    TREATMENT154: restoration_pair(checkpoint_sha=TREATMENT154, executable_sha=TREATMENT154),
}
mixed_corridor = compute_verdict(corridor_input)
assert mixed_corridor["verdict"] == "mixed", mixed_corridor
assert len(mixed_corridor["counted_restorations"]) == 3, mixed_corridor
assert mixed_corridor["corridor_regressions"] == [CONTROL03], mixed_corridor

# --- Bonus: a descriptive (non-required) checkpoint is recorded but never decides. Give
# lever2-160 data that would, on its own, force "mixed" (a corridor regression) alongside an
# otherwise-clean rescue on the 3 required checkpoints - the verdict must stay H3a_rescue. ---
descriptive_input = dict(rescue_input)
descriptive_input[LEVER2] = restoration_pair(
    checkpoint_sha=LEVER2, executable_sha=LEVER2, corridor_regression=True)
descriptive_result = compute_verdict(descriptive_input)
assert descriptive_result["verdict"] == "H3a_rescue", descriptive_result
assert descriptive_result["checkpoints"][LEVER2]["required"] is False, descriptive_result
assert descriptive_result["checkpoints"][LEVER2]["corridor-clear"]["regression"] is True, (
    descriptive_result
)  # recorded...
assert LEVER2 not in descriptive_result["corridor_regressions"], descriptive_result  # ...but not decisive

# --- Bonus: a restoration candidate with NO recorded bomb step at all (malformed/impossible
# data) must not count and must surface as a disagreement, not silently pass. ---
no_bomb_step_input = dict(rescue_input)
no_bomb_step_input[CONTROL03] = {
    "self": make_evidence(checkpoint_sha=CONTROL03, executable_sha=CONTROL03, trap_pass=False),
    "aligned": make_evidence(checkpoint_sha=CONTROL03, executable_sha=CONTROL03, trap_pass=True,
                             trap_steps=[]),
}
no_bomb_result = compute_verdict(no_bomb_step_input)
assert no_bomb_result["verdict"] == "mixed", no_bomb_result
assert CONTROL03 in no_bomb_result["restoration_candidates_without_visit_lead"], no_bomb_result

# --- Guardrail: missing a required checkpoint entirely is a hard error ---
try:
    compute_verdict({CONTROL03: non_candidate_pair(), CONTROL154: non_candidate_pair()})
    raise AssertionError("expected ValueError for a missing required checkpoint")
except ValueError:
    pass

# --- Guardrail: mismatched checkpoint/executable SHA between a checkpoint's self and aligned
# evidence must fail closed (not the same battery/binary - not a valid ablation pair) ---
try:
    mismatched = {
        CONTROL03: {
            "self": make_evidence(checkpoint_sha="a", executable_sha="x", trap_pass=False),
            "aligned": make_evidence(checkpoint_sha="b", executable_sha="x", trap_pass=True),
        },
        CONTROL154: non_candidate_pair(checkpoint_sha=CONTROL154, executable_sha=CONTROL154),
        TREATMENT154: non_candidate_pair(checkpoint_sha=TREATMENT154, executable_sha=TREATMENT154),
    }
    compute_verdict(mismatched)
    raise AssertionError("expected ValueError for mismatched checkpoint_sha256 across modes")
except ValueError:
    pass

# --- parse_label: the checkpoint:mode=path label grammar ---
assert parse_label("control03-130:self=/tmp/a.json") == (
    "control03-130", "self", Path("/tmp/a.json"))
for bad in ("no-equals-sign", "control03-130=/no/mode.json", "control03-130:bogus=/x.json"):
    try:
        parse_label(bad)
        raise AssertionError(f"expected ValueError for malformed label {bad!r}")
    except ValueError:
        pass

# --- End-to-end: exercise the real CLI (argparse + file I/O + --output), not just the pure
# compute_verdict() logic above - proves load_evidence's provenance checks and main()'s label
# parsing/output-writing plumbing actually work together. ---
with tempfile.TemporaryDirectory() as directory:
    run_dir = Path(directory)
    args = []
    for name, pair in rescue_input.items():
        for mode in ("self", "aligned"):
            path = run_dir / f"{name}-{mode}.json"
            path.write_text(json.dumps({**pair[mode], "search_opponent_model": mode}))
            args.append(f"{name}:{mode}={path}")
    output_path = run_dir / "verdict.json"
    completed = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "analyze_h3a_verdict.py"),
         *args, "--output", str(output_path)],
        capture_output=True, text=True, check=False)
    assert completed.returncode == 0, (
        f"analyze_h3a_verdict.py CLI failed: stdout={completed.stdout!r} "
        f"stderr={completed.stderr!r}"
    )
    cli_result = json.loads(output_path.read_text())
    assert cli_result["verdict"] == "H3a_rescue", cli_result

    # A mode label that doesn't match the evidence file's own search_opponent_model must fail
    # closed (KL-101 mislabeled-provenance discipline), not silently trust the CLI label.
    mislabeled_path = run_dir / "mislabeled.json"
    mislabeled_path.write_text(json.dumps(
        {**non_candidate_pair()["self"], "search_opponent_model": "aligned"}))
    mislabeled = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "analyze_h3a_verdict.py"),
         f"{CONTROL03}:self={mislabeled_path}"],
        capture_output=True, text=True, check=False)
    assert mislabeled.returncode != 0, "expected a mislabeled search_opponent_model to fail closed"

print(
    "H3a verdict tool tests passed: rescue, no-rescue, mixed(one-restoration), "
    "mixed(restoration-without-visit-lead), corridor-regression override, descriptive-checkpoint "
    "non-decision, missing-bomb-step disagreement, missing-required-checkpoint guardrail, "
    "mismatched-battery guardrail, parse_label grammar, and one end-to-end CLI pass all verified"
)
