"""v7 Stage 0 item 0.4 (docs/experiment-memory/14-v7-from-scratch-design.md; SEARCH-CONTEMPT
PROTOTYPE, Joshi 2025 arXiv:2504.07757): validates the `gates` subcommand's --gates-search-
contempt / --search-contempt-nscl wiring end to end, against the same tiny fixture checkpoint
test_native_alphazero_gates_check.py already uses (native-alphazero-semantic-smoke).

Three claims, each a distinct test-plan item from the v7 Stage 0 item 0.4 unit brief:
  (1) Determinism: two independent --gates-search-contempt --search-contempt-nscl 1 runs against
      the identical checkpoint, with identical seeds/flags, must produce byte-identical scenarios
      arrays - BatchedMcts::select_joint_contempt() samples from the frozen snapshot using the
      search's own rng, which is deterministic per fixed seed/order exactly like every other
      gates code path (see test_native_alphazero_gates_check.py's own determinism claims for
      self/aligned mode).
  (2) Live path: at least one scenario's search.steps differs between the contempt-on run and the
      contempt-off baseline (gates-network-1.json, produced earlier in this same ctest chain by
      test_native_alphazero_gates_run - identical checkpoint/flags apart from --gates-search-
      contempt/--search-contempt-nscl). This proves BatchedMcts::select_joint_contempt() is
      actually reached and actually changes what gets selected somewhere in the tree - not a
      behavioral/quality claim about search-contempt itself (untested here; this prototype is
      gates-only and not used by any training path - see doc 14 item 0.4).
  (3) Evidence shape: gates_format_version 2's new top-level "search_contempt_nscl" field is
      present (and equal to the --search-contempt-nscl value) exactly when --gates-search-
      contempt was passed, and ABSENT on a run that never passed the flag - confirming the field
      is conditional metadata about THIS run, not a blanket dump of whatever
      config.search_contempt_nscl happened to CLI-parse to."""

from __future__ import annotations

import json
import sys
from pathlib import Path

EXPECTED_NSCL = 1

run_dir = Path(sys.argv[1])
network_1 = json.loads((run_dir / "gates-network-1.json").read_text())
contempt_1 = json.loads((run_dir / "gates-contempt-1.json").read_text())
contempt_2 = json.loads((run_dir / "gates-contempt-2.json").read_text())

# --- (3a) Evidence shape: contempt-off baseline never carries the field. ---
assert "search_contempt_nscl" not in network_1, (
    "gates-network-1.json (no --gates-search-contempt) must not carry a top-level "
    f"search_contempt_nscl field, got {network_1.get('search_contempt_nscl')!r}"
)

# --- (3b) Evidence shape: contempt-on runs carry the field, equal to --search-contempt-nscl. ---
for evidence, label in ((contempt_1, "contempt run 1"), (contempt_2, "contempt run 2")):
    assert evidence["gates_format_version"] == 2, (
        f"{label}: expected gates_format_version 2, got {evidence.get('gates_format_version')!r}"
    )
    assert evidence["mode"] == "network", f"{label}: expected mode=='network', got {evidence['mode']!r}"
    assert evidence.get("search_contempt_nscl") == EXPECTED_NSCL, (
        f"{label}: expected top-level search_contempt_nscl=={EXPECTED_NSCL} (--gates-search-"
        f"contempt was passed), got {evidence.get('search_contempt_nscl')!r}"
    )
    # search_opponent_model defaults to "self" (--gates-opponent-model was not passed) - contempt
    # composes with self, the interesting case per doc 14 item 0.4's wiring section.
    assert evidence["search_opponent_model"] == "self", (
        f"{label}: expected search_opponent_model=='self', got "
        f"{evidence['search_opponent_model']!r}"
    )

# --- (1) Determinism: byte-identical scenarios arrays across the two contempt runs. ---
assert contempt_1["scenarios"] == contempt_2["scenarios"], (
    "gates --gates-search-contempt is not deterministic: two runs against the identical "
    "checkpoint with identical flags/seeds produced different scenario outcomes:\n"
    f"  contempt run 1: {contempt_1['scenarios']}\n"
    f"  contempt run 2: {contempt_2['scenarios']}"
)

# --- (2) Live path: at least one scenario's search.steps differs from the contempt-off baseline.
# Compare by name (not list order/index) so a future reordering of build_gate_scenarios() cannot
# silently break this into a false pass or a false failure for the wrong reason.
network_scenarios = {s["name"]: s for s in network_1["scenarios"]}
contempt_scenarios = {s["name"]: s for s in contempt_1["scenarios"]}
assert set(network_scenarios) == set(contempt_scenarios), (
    "contempt-on and contempt-off runs cover different scenario sets - "
    f"off: {sorted(network_scenarios)}, on: {sorted(contempt_scenarios)}"
)
differing_steps = [
    name for name in network_scenarios
    if network_scenarios[name]["search"]["steps"] != contempt_scenarios[name]["search"]["steps"]
]
assert differing_steps, (
    "search-contempt (--gates-search-contempt --search-contempt-nscl "
    f"{EXPECTED_NSCL}) produced IDENTICAL search.steps to the contempt-off baseline on every "
    "one of all 6 scenarios - BatchedMcts::select_joint_contempt() is either never reached or "
    "never changes the outcome at this fixture's tiny --eval-simulations 8 budget; this is the "
    "'code path is live' guard, not a behavioral claim, so a pass here requires at least one "
    "scenario to differ somewhere, however small - see doc 14 item 0.4's test plan item 3 "
    "(lower nscl / raise simulations if this ever regresses to zero differing scenarios)"
)

print(
    "Native AlphaZero SEARCH-CONTEMPT PROTOTYPE (gates --gates-search-contempt "
    f"--search-contempt-nscl {EXPECTED_NSCL}) validated: evidence shape correct (field present "
    "only when the flag is set, absent otherwise), byte-identical determinism across 2 runs, "
    f"and search.steps differs from the contempt-off baseline on {len(differing_steps)}/6 "
    f"scenario(s) ({sorted(differing_steps)}) - proving the code path is live."
)
