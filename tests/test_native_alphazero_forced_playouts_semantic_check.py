"""v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md): forced_playouts_k
must ride the FULL semantic manifest machinery exactly like temperature_final did - persisted,
inherited on a plain resume (silently, no fork-log entry), and explicit-override-logged as a
SEMANTIC FORK when a later resume's CLI value diverges from the checkpoint's stored manifest.

The fixture is a 3-invocation chain into ONE run-dir:
  1. fresh:    --forced-playouts-k 2                          (iteration 0->1)
  2. resume:   no forced-playouts flag at all - must inherit forced_playouts_k=2 silently
               (iteration 1->2)
  3. override: --forced-playouts-k 3 - must log a SEMANTIC FORK line for forced_playouts_k
               (iteration 2->3)

config-history.jsonl accumulates one row per PROCESS invocation (appended in the constructor,
before that process's own training loop advances `iteration` further), so its "runtime_config"
field (runtime_config_signature(config) - a semicolon key=value string) gives a direct snapshot
of what apply_semantic_manifest() resolved at the START of each of the three invocations above -
exactly the "inherited vs overridden" moment this test needs, without splitting into three
separate ctest steps. Structurally mirrors test_native_alphazero_temperature_anneal_check.py,
which established this exact pattern for the previous v7 Stage 0 semantic-field addition."""

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
    f"invocation (0 fresh, 1 resume, 2 override); got "
    f"{[row['iteration'] for row in rows]}"
)

fresh_cfg = parse_runtime_config(fresh_row["runtime_config"])
resume_cfg = parse_runtime_config(resume_row["runtime_config"])
override_cfg = parse_runtime_config(override_row["runtime_config"])

# (1) Fresh run: runtime_config_signature carries the field at its explicit CLI value.
assert abs(float(fresh_cfg["forced_playouts_k"]) - 2.0) < 1e-9, (
    f"fresh runtime_config forced_playouts_k={fresh_cfg.get('forced_playouts_k')!r}, expected 2"
)

# (2) Plain resume, no forced-playouts flag at all: inherited from the checkpoint manifest, not
# silently reset to the compiled default (forced_playouts_k=0.0) the way a pre-existing KL-101
# bug would have done.
assert abs(float(resume_cfg["forced_playouts_k"]) - 2.0) < 1e-9, (
    f"resume (no override) runtime_config forced_playouts_k="
    f"{resume_cfg.get('forced_playouts_k')!r}, expected inherited 2 - a plain resume must not "
    f"silently fall back to the compiled default 0"
)

# (3) Explicit override resume: forced_playouts_k follows the CLI (3), not the inherited 2.
assert abs(float(override_cfg["forced_playouts_k"]) - 3.0) < 1e-9, (
    f"override runtime_config forced_playouts_k={override_cfg.get('forced_playouts_k')!r}, "
    f"expected the explicit CLI value 3, not the inherited 2"
)

# (4) Final config.json (written by the override invocation, the last one) mirrors the same
# resolved state - the config JSON dump must carry the field like temperature_final does.
config = json.loads((run_dir / "config.json").read_text())
assert abs(config["forced_playouts_k"] - 3.0) < 1e-9, (
    f"config.json forced_playouts_k={config.get('forced_playouts_k')!r}, expected 3"
)

# (5) SEMANTIC FORK visibility: the override invocation is the ONLY one of the three that passes
# any explicit semantic flag (--forced-playouts-k 3, diverging from the checkpoint's stored 2) -
# exactly one semantic-fork-log.jsonl line should exist for the whole chain (proving BOTH that
# the fresh run's --fresh truncation left it empty beforehand, and that the plain resume in step
# 2 contributed nothing - "fork-log silent" on inherit).
fork_log_path = run_dir / "semantic-fork-log.jsonl"
assert fork_log_path.exists(), (
    "expected semantic-fork-log.jsonl to exist after the explicit --forced-playouts-k 3 "
    "override (checkpoint=2 -> explicit CLI=3 must be logged as a SEMANTIC FORK, exactly like "
    "any other semantic field's explicit override)"
)
fork_log_lines = fork_log_path.read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl line across this fresh+resume+override chain "
    f"(only the override invocation forks anything), got {len(fork_log_lines)} - either --fresh "
    f"stopped truncating this file, or the plain resume in step 2 unexpectedly forked something"
)
last_entry = json.loads(fork_log_lines[-1])
matching = [fork for fork in last_entry["forks"] if fork.startswith("forced_playouts_k:")]
assert matching, (
    f"no forced_playouts_k fork line in the semantic-fork-log.jsonl entry (saw: "
    f"{last_entry['forks']}) - the explicit --forced-playouts-k 3 override against a checkpoint "
    f"manifest value of 2 must be logged"
)
assert matching[0] == "forced_playouts_k: checkpoint=2 -> explicit CLI=3", (
    f"unexpected fork line format: {matching[0]!r}"
)

print("Native AlphaZero forced-playouts-k semantic inheritance + explicit-override fork "
      f"visibility validated (fresh={fresh_cfg['forced_playouts_k']}, "
      f"resume inherited={resume_cfg['forced_playouts_k']}, "
      f"override={override_cfg['forced_playouts_k']}, fork_line={matching[0]!r})")
