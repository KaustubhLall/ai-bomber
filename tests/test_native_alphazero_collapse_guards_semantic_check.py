"""v7 Stage-1 IL Bombing-Collapse guards (docs/experiment-memory/14-v7-from-scratch-design.md
Stage 1): policy_entropy_bonus and value_only_iterations must ride the FULL semantic manifest
machinery exactly like forced_playouts_k / teacher_agents - persisted, inherited on a plain
resume (silently, no fork-log entry), and explicit-override-logged as a SEMANTIC FORK when a
later resume's CLI value diverges from the checkpoint's stored manifest.

The fixture is a 3-invocation chain into ONE run-dir:
  1. fresh:    --value-only-iterations 4 --policy-entropy-bonus 0.01       (iteration 0->1)
  2. resume:   NEITHER flag - must inherit both silently                   (iteration 1->2)
  3. override: --value-only-iterations 6 --policy-entropy-bonus 0.02 -
               both diverge from the checkpoint, both must log a fork line (iteration 2->3)

Structurally mirrors test_native_alphazero_forced_playouts_semantic_check.py, which established
this pattern for the previous v7 semantic-field additions."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def parse_runtime_config(raw: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for pair in raw.split(";"):
        if "=" not in pair:
            continue
        key, _, value = pair.partition("=")
        result[key] = value
    return result


run_dir = Path(sys.argv[1])

rows = [json.loads(line) for line in (run_dir / "config-history.jsonl").read_text().splitlines()]
assert len(rows) == 3, (
    f"expected 3 config-history.jsonl rows (fresh + resume + override, one per process "
    f"invocation), got {len(rows)}"
)
fresh_row, resume_row, override_row = rows
assert [fresh_row["iteration"], resume_row["iteration"], override_row["iteration"]] == [0, 1, 2], (
    "config-history.jsonl rows must reflect the iteration counter at the START of each "
    f"invocation (0 fresh, 1 resume, 2 override); got {[row['iteration'] for row in rows]}"
)

fresh_cfg = parse_runtime_config(fresh_row["runtime_config"])
resume_cfg = parse_runtime_config(resume_row["runtime_config"])
override_cfg = parse_runtime_config(override_row["runtime_config"])


def check(cfg: dict[str, str], voi: int, beta: float, label: str) -> None:
    assert cfg.get("value_only_iterations") == str(voi), (
        f"{label} runtime_config value_only_iterations="
        f"{cfg.get('value_only_iterations')!r}, expected {voi}"
    )
    assert abs(float(cfg["policy_entropy_bonus"]) - beta) < 1e-9, (
        f"{label} runtime_config policy_entropy_bonus="
        f"{cfg.get('policy_entropy_bonus')!r}, expected {beta}"
    )


# (1) Fresh run: both fields at their explicit CLI values.
check(fresh_cfg, 4, 0.01, "fresh")
# (2) Plain resume, no collapse-guard flags: inherited from the manifest, not silently reset to
# the compiled defaults (0 / 0.0).
check(resume_cfg, 4, 0.01, "resume (no override)")
# (3) Explicit override resume: both follow the CLI, not the inherited values.
check(override_cfg, 6, 0.02, "override")

# (4) Final config.json (written by the override invocation, the last one) mirrors the same
# resolved state.
config = json.loads((run_dir / "config.json").read_text())
assert config["value_only_iterations"] == 6, (
    f"config.json value_only_iterations={config.get('value_only_iterations')!r}, expected 6"
)
assert abs(config["policy_entropy_bonus"] - 0.02) < 1e-9, (
    f"config.json policy_entropy_bonus={config.get('policy_entropy_bonus')!r}, expected 0.02"
)

# (5) SEMANTIC FORK visibility: the override invocation is the ONLY one passing explicit semantic
# flags (both diverging from the checkpoint), so exactly one semantic-fork-log.jsonl line should
# exist for the whole chain, and that entry must carry a fork line for BOTH fields.
fork_log_path = run_dir / "semantic-fork-log.jsonl"
assert fork_log_path.exists(), (
    "expected semantic-fork-log.jsonl to exist after the explicit override (both fields "
    "diverging from the checkpoint must be logged as SEMANTIC FORKs)"
)
fork_log_lines = fork_log_path.read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl line across this fresh+resume+override chain "
    f"(only the override invocation forks anything), got {len(fork_log_lines)} - either --fresh "
    f"stopped truncating this file, or the plain resume unexpectedly forked something"
)
forks = json.loads(fork_log_lines[-1])["forks"]

voi_forks = [f for f in forks if f.startswith("value_only_iterations:")]
assert voi_forks == ["value_only_iterations: checkpoint=4 -> explicit CLI=6"], (
    f"expected a value_only_iterations fork line (checkpoint=4 -> explicit CLI=6), saw: {forks}"
)
beta_forks = [f for f in forks if f.startswith("policy_entropy_bonus:")]
assert beta_forks == ["policy_entropy_bonus: checkpoint=0.01 -> explicit CLI=0.02"], (
    f"expected a policy_entropy_bonus fork line (checkpoint=0.01 -> explicit CLI=0.02), saw: "
    f"{forks}"
)

print(
    "Native AlphaZero collapse-guards semantic inheritance + explicit-override fork visibility "
    f"validated (fresh voi=4/beta=0.01, resume inherited both, override voi=6/beta=0.02; fork "
    f"lines: {voi_forks[0]!r}, {beta_forks[0]!r})"
)
