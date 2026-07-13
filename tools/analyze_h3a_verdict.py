"""KL-110 Phase B: machine-readable H3a verdict discriminator for the aligned-opponent gate
ablation. Reads gates evidence JSONs (gates_format_version 2 - native trainer's `gates`
subcommand with --gates-opponent-model self|aligned) for 4 checkpoints x 2 opponent models and
applies the FROZEN rule set below, exactly, to decide whether scenario-aligned constrained
search rescues the trap-gate raw-pass/search-fail inversion first observed in
docs/experiment-memory/12-post-audit-execution.md's "Load-bearing new observation" (control03-130
AND lever2-160: raw-argmax passes the trap gate in 5 steps while 96-sim "self"-opponent-model
search FAILS it).

Vocabulary (frozen, do not reinterpret):
- "restoration" on a checkpoint = the trap gate goes self-mode search FAIL -> aligned-mode
  search PASS (both evidence files must be from the SAME battery/binary - checked below; a
  checkpoint_sha256/executable_sha256 mismatch between a checkpoint's self and aligned evidence
  is a hard error, not a warning, since a difference could then be explained by a different
  checkpoint or binary rather than by the opponent-model change under test).
- A restoration only COUNTS toward the verdict if, at the LAST step with chosen==BOMB in the
  ALIGNED run's trap trace (the bomb placement responsible for the kill), visit_marginal's
  argmax is BOMB - i.e. search's own root-visit distribution actually leads toward the bomb,
  not just that the final outcome happens to pass (PUCT can execute a low-visit action near the
  root at low simulation counts; the visit lead is the actual "search chose this" signal).
  Root Q (root_q_at_bomb_step in the output) is supporting telemetry ONLY, MUST NOT be used to
  decide "counted", and MUST be ignored for actions with zero visits (Q defaults to 0.0 there,
  which is not a genuine evaluation - see GateStepRecord in trainer.cpp).
- A restoration CANDIDATE (self FAIL, aligned PASS) that fails the visit-lead check is a
  Q/visit-vs-behavior disagreement - passing without search's own visit distribution supporting
  the "bomb" story. This is exactly the kind of gap the trace-language rule exists to catch, so
  it forces the WHOLE verdict to "mixed", independent of the tally elsewhere - see
  restoration_candidates_without_visit_lead in the output.
- "corridor regression" = a REQUIRED checkpoint's corridor-clear scenario going self-mode PASS ->
  aligned-mode FAIL.
- Decision uses ONLY the three REQUIRED checkpoints: control03-130 (base), control-154,
  treatment-154 (docs/experiment-memory/13-kl105-experiment-design.md). Any other checkpoint
  (e.g. lever2-160) is recorded in the output as descriptive only and never affects the verdict.

Verdict (exactly one of):
- H3a_rescue    = counted restorations on >=2 of the 3 required checkpoints AND no corridor
                  regression.
- mixed         = exactly 1 counted restoration, OR any restoration-candidate/visit-lead
                  disagreement among the 3 required checkpoints (regardless of the tally), OR
                  corridor regression alongside >=2 counted restorations.
- H3a_no_rescue = 0 counted restorations and none of the "mixed" triggers above fired.

Scope note (always included in the output verbatim, regardless of which verdict fires): a rescue
supports scenario-aligned constrained search and also benefits from reduced opponent branching -
it does not by itself separate those two mechanisms. A no-rescue does NOT kill H3 (opponent-model
mismatch) globally: leaf values remain self-play-conditioned regardless of what search's
root-level opponent model assumes - this ablation only tests search-TIME opponent modeling, not
the value head's own training-time conditioning.

Usage:
    python tools/analyze_h3a_verdict.py \\
        control03-130:self=path.json     control03-130:aligned=path.json \\
        control-154:self=path.json       control-154:aligned=path.json \\
        treatment-154:self=path.json     treatment-154:aligned=path.json \\
        lever2-160:self=path.json        lever2-160:aligned=path.json \\
        [--output verdict.json]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

BOMB = 4
ACTIONS = 6

REQUIRED_CHECKPOINTS = ("control03-130", "control-154", "treatment-154")
MODES = ("self", "aligned")

SCOPE_NOTE = (
    "a rescue supports scenario-aligned constrained search and also benefits from reduced "
    "opponent branching - it does not by itself separate those two mechanisms; a no-rescue "
    "does NOT kill H3 (opponent-model mismatch) globally, since leaf values remain "
    "self-play-conditioned regardless of what search's root-level opponent model assumes - "
    "this ablation only tests search-time opponent modeling, not the value head's own "
    "training-time conditioning"
)


def argmax(values: list[float]) -> int:
    return max(range(len(values)), key=lambda i: values[i])


def _scenario(evidence: dict, name: str) -> dict:
    for scenario in evidence["scenarios"]:
        if scenario["name"] == name:
            return scenario
    raise ValueError(f"evidence is missing scenario {name!r}")


def check_same_battery(checkpoint: str, self_evidence: dict, aligned_evidence: dict) -> None:
    """Causal-control discipline (project standing rule): a restoration is only meaningful if
    the self and aligned evidence for one checkpoint came from the identical checkpoint file and
    binary - otherwise an observed difference could be explained by a different checkpoint or
    executable, not by the opponent-model change under test. Hard error, not a soft downgrade:
    this tool mechanically applies a FROZEN, pre-registered discriminator, so silently accepting
    mismatched provenance would produce a verdict that looks authoritative but isn't licensed by
    the protocol."""
    for key in ("checkpoint_sha256", "executable_sha256"):
        if self_evidence.get(key) != aligned_evidence.get(key):
            raise ValueError(
                f"{checkpoint}: self/aligned evidence {key} differ "
                f"({self_evidence.get(key)!r} vs {aligned_evidence.get(key)!r}) - not the same "
                f"battery/binary; refusing to treat this pair as a valid ablation"
            )


def trap_restoration(checkpoint: str, self_evidence: dict, aligned_evidence: dict) -> dict:
    self_trap = _scenario(self_evidence, "trap")["search"]
    aligned_trap = _scenario(aligned_evidence, "trap")["search"]
    candidate = (not self_trap["passed"]) and aligned_trap["passed"]

    record = {
        "checkpoint": checkpoint,
        "self_pass": self_trap["passed"],
        "aligned_pass": aligned_trap["passed"],
        "restoration_candidate": candidate,
        "bomb_step": None,
        "visit_lead": False,
        "root_q_at_bomb_step": None,
        "counted": False,
    }
    if not candidate:
        return record

    bomb_steps = [s for s in aligned_trap["steps"] if s["chosen"] == BOMB]
    if not bomb_steps:
        # Physically shouldn't happen for a genuine trap-gate pass (death_owner==learner
        # requires the learner to have placed a bomb at some point) - if it does anyway, there
        # is no bomb-placement step to check the visit lead against, so the restoration cannot
        # count; leave visit_lead/counted at their False defaults rather than guessing.
        return record

    bomb_step = max(bomb_steps, key=lambda s: s["step"])
    record["bomb_step"] = bomb_step["step"]
    record["root_q_at_bomb_step"] = bomb_step["root_q"]
    record["visit_lead"] = argmax(bomb_step["visit_marginal"]) == BOMB
    record["counted"] = record["visit_lead"]
    return record


def corridor_regression(checkpoint: str, self_evidence: dict, aligned_evidence: dict) -> dict:
    self_corridor = _scenario(self_evidence, "corridor-clear")["search"]
    aligned_corridor = _scenario(aligned_evidence, "corridor-clear")["search"]
    regressed = self_corridor["passed"] and not aligned_corridor["passed"]
    return {
        "checkpoint": checkpoint,
        "self_pass": self_corridor["passed"],
        "aligned_pass": aligned_corridor["passed"],
        "regression": regressed,
    }


def compute_verdict(evidence_by_checkpoint: dict[str, dict[str, dict]]) -> dict:
    """evidence_by_checkpoint: {checkpoint_label: {"self": evidence_dict, "aligned": evidence_dict}}.
    Every checkpoint present must carry both modes. A missing REQUIRED checkpoint is a hard
    error (the verdict is meaningless without all three); extra (descriptive) checkpoints are
    recorded in the output but never affect the verdict - see REQUIRED_CHECKPOINTS."""
    missing_required = [c for c in REQUIRED_CHECKPOINTS if c not in evidence_by_checkpoint]
    if missing_required:
        raise ValueError(f"missing required checkpoint(s): {missing_required}")

    checkpoints_out: dict[str, dict] = {}
    trap_records: dict[str, dict] = {}
    corridor_records: dict[str, dict] = {}

    for checkpoint, by_mode in evidence_by_checkpoint.items():
        missing_modes = [mode for mode in MODES if mode not in by_mode]
        if missing_modes:
            raise ValueError(f"{checkpoint}: missing mode(s) {missing_modes}")
        check_same_battery(checkpoint, by_mode["self"], by_mode["aligned"])
        trap_records[checkpoint] = trap_restoration(checkpoint, by_mode["self"], by_mode["aligned"])
        corridor_records[checkpoint] = corridor_regression(
            checkpoint, by_mode["self"], by_mode["aligned"])
        checkpoints_out[checkpoint] = {
            "required": checkpoint in REQUIRED_CHECKPOINTS,
            "trap": trap_records[checkpoint],
            "corridor-clear": corridor_records[checkpoint],
        }

    required_trap = [trap_records[c] for c in REQUIRED_CHECKPOINTS]
    required_corridor = [corridor_records[c] for c in REQUIRED_CHECKPOINTS]

    counted_restorations = [r["checkpoint"] for r in required_trap if r["counted"]]
    disagreements = [
        r["checkpoint"] for r in required_trap if r["restoration_candidate"] and not r["counted"]
    ]
    corridor_regressions = [r["checkpoint"] for r in required_corridor if r["regression"]]

    n_counted = len(counted_restorations)
    if disagreements:
        # Any Q/visit-vs-behavior disagreement forces "mixed" unconditionally - see module
        # docstring. Checked FIRST so it can never be silently absorbed into a clean tally-only
        # verdict, even if enough OTHER checkpoints would otherwise clear the rescue bar.
        verdict = "mixed"
    elif n_counted == 1:
        verdict = "mixed"
    elif n_counted >= 2:
        verdict = "mixed" if corridor_regressions else "H3a_rescue"
    else:
        verdict = "H3a_no_rescue"

    return {
        "verdict": verdict,
        "scope_note": SCOPE_NOTE,
        "required_checkpoints": list(REQUIRED_CHECKPOINTS),
        "counted_restorations": counted_restorations,
        "restoration_candidates_without_visit_lead": disagreements,
        "corridor_regressions": corridor_regressions,
        "checkpoints": checkpoints_out,
    }


def load_evidence(path: Path, checkpoint: str, mode: str) -> dict:
    evidence = json.loads(path.read_text())
    if evidence.get("gates_format_version") != 2:
        raise ValueError(
            f"{path}: gates_format_version {evidence.get('gates_format_version')!r} predates "
            f"the per-step search telemetry (v2) this verdict tool depends on"
        )
    actual_mode = evidence.get("search_opponent_model")
    if actual_mode != mode:
        raise ValueError(
            f"{path}: labeled '{checkpoint}:{mode}' but its own search_opponent_model field "
            f"says {actual_mode!r} - mislabeled input, refusing to guess which one is right"
        )
    return evidence


def parse_label(entry: str) -> tuple[str, str, Path]:
    label, sep, path_str = entry.partition("=")
    if not sep:
        raise ValueError(f"expected checkpoint:mode=path.json (got {entry!r})")
    checkpoint, sep2, mode = label.partition(":")
    if not sep2 or mode not in MODES:
        raise ValueError(
            f"expected checkpoint:mode=path.json with mode in {MODES} (got label {label!r})"
        )
    return checkpoint, mode, Path(path_str)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("evidence", nargs="+",
                         help="checkpoint:mode=path.json (mode is 'self' or 'aligned'); the "
                              "usual battery is 4 checkpoints x 2 modes = 8 args")
    parser.add_argument("--output", type=Path, default=None,
                         help="write the verdict JSON here instead of stdout")
    args = parser.parse_args()

    evidence_by_checkpoint: dict[str, dict[str, dict]] = {}
    for entry in args.evidence:
        checkpoint, mode, path = parse_label(entry)
        evidence_by_checkpoint.setdefault(checkpoint, {})[mode] = load_evidence(
            path, checkpoint, mode)

    result = compute_verdict(evidence_by_checkpoint)
    text = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(text + "\n", encoding="utf-8")
        print(f"wrote {args.output}")
    else:
        print(text)


if __name__ == "__main__":
    main()
