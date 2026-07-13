"""v7 Stage 0 item 0.1+0.3 (docs/experiment-memory/14-v7-from-scratch-design.md): temperature_final/
temperature_anneal must ride the FULL semantic manifest machinery exactly like
replay_cause_balance_cap did - persisted, inherited on a plain resume (silently, no fork-log
entry), and explicit-override-logged as a SEMANTIC FORK when a later resume's CLI value diverges
from the checkpoint's stored manifest.

The fixture is a 3-invocation chain into ONE run-dir:
  1. fresh:    --temperature-anneal --temperature-final 0.25 --temperature-steps 4  (iteration 0->1)
  2. resume:   no temperature flags at all - must inherit anneal=true/final=0.25/steps=4 silently
               (iteration 1->2)
  3. override: --temperature-final 0.1 only - must log a SEMANTIC FORK line for temperature_final
               specifically, while temperature_anneal/temperature_steps continue to inherit
               silently (never re-passed) (iteration 2->3)

config-history.jsonl accumulates one row per PROCESS invocation (appended in the constructor,
before that process's own training loop advances `iteration` further), so its "runtime_config"
field (runtime_config_signature(config) - a semicolon key=value string) gives a direct snapshot
of what apply_semantic_manifest() resolved at the START of each of the three invocations above -
exactly the "inherited vs overridden" moment this test needs, without splitting into three
separate ctest steps."""

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

# (1) Fresh run: runtime_config_signature carries both new fields at their explicit CLI values.
assert fresh_cfg["temperature_anneal"] == "true", (
    f"fresh runtime_config temperature_anneal={fresh_cfg.get('temperature_anneal')!r}, "
    f"expected 'true' - --temperature-anneal was passed explicitly"
)
assert abs(float(fresh_cfg["temperature_final"]) - 0.25) < 1e-9, (
    f"fresh runtime_config temperature_final={fresh_cfg.get('temperature_final')!r}, expected 0.25"
)
assert fresh_cfg["temperature_steps"] == "4", (
    f"fresh runtime_config temperature_steps={fresh_cfg.get('temperature_steps')!r}, expected 4"
)

# (2) Plain resume, no temperature flags at all: both fields inherited from the checkpoint
# manifest, not silently reset to the compiled defaults (temperature_final=0.0,
# temperature_anneal=false) the way a pre-existing KL-101 bug would have done.
assert resume_cfg["temperature_anneal"] == "true", (
    f"resume (no override) runtime_config temperature_anneal="
    f"{resume_cfg.get('temperature_anneal')!r}, expected inherited 'true' - a plain resume "
    f"must not silently fall back to the compiled default 'false'"
)
assert abs(float(resume_cfg["temperature_final"]) - 0.25) < 1e-9, (
    f"resume (no override) runtime_config temperature_final="
    f"{resume_cfg.get('temperature_final')!r}, expected inherited 0.25"
)

# (3) Explicit override resume: temperature_final follows the CLI (0.1), but temperature_anneal
# (never re-passed) keeps inheriting silently.
assert override_cfg["temperature_anneal"] == "true", (
    f"override runtime_config temperature_anneal={override_cfg.get('temperature_anneal')!r}, "
    f"expected inherited 'true' (temperature-anneal was never re-passed on this invocation)"
)
assert abs(float(override_cfg["temperature_final"]) - 0.1) < 1e-9, (
    f"override runtime_config temperature_final={override_cfg.get('temperature_final')!r}, "
    f"expected the explicit CLI value 0.1, not the inherited 0.25"
)

# (4) Final config.json (written by the override invocation, the last one) mirrors the same
# resolved state - the config JSON dump must carry both fields like arena_crush_win_value does.
config = json.loads((run_dir / "config.json").read_text())
assert config["temperature_anneal"] is True, (
    f"config.json temperature_anneal={config.get('temperature_anneal')!r}, expected true"
)
assert abs(config["temperature_final"] - 0.1) < 1e-9, (
    f"config.json temperature_final={config.get('temperature_final')!r}, expected 0.1"
)
assert config["temperature_steps"] == 4, (
    f"config.json temperature_steps={config.get('temperature_steps')!r}, expected 4 (inherited "
    f"across all three invocations, never overridden)"
)

# (5) SEMANTIC FORK visibility: the override invocation is the ONLY one of the three that passes
# any explicit semantic flag (--temperature-final 0.1, diverging from the checkpoint's stored
# 0.25) - exactly one semantic-fork-log.jsonl line should exist for the whole chain (proving BOTH
# that the fresh run's --fresh truncation left it empty beforehand, and that the plain resume in
# step 2 contributed nothing - "fork-log silent" on inherit). Defensive [-1] indexing (not a bare
# single-element unpack) matches this project's other semantic-fork checks' own convention for
# tolerating repeat ctest invocations of the same fixture.
fork_log_path = run_dir / "semantic-fork-log.jsonl"
assert fork_log_path.exists(), (
    "expected semantic-fork-log.jsonl to exist after the explicit --temperature-final 0.1 "
    "override (checkpoint=0.25 -> explicit CLI=0.1 must be logged as a SEMANTIC FORK, exactly "
    "like any other semantic field's explicit override)"
)
fork_log_lines = fork_log_path.read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl line across this fresh+resume+override chain "
    f"(only the override invocation forks anything), got {len(fork_log_lines)} - either --fresh "
    f"stopped truncating this file, or the plain resume in step 2 unexpectedly forked something"
)
last_entry = json.loads(fork_log_lines[-1])
matching = [fork for fork in last_entry["forks"] if fork.startswith("temperature_final:")]
assert matching, (
    f"no temperature_final fork line in the semantic-fork-log.jsonl entry (saw: "
    f"{last_entry['forks']}) - the explicit --temperature-final 0.1 override against a "
    f"checkpoint manifest value of 0.25 must be logged"
)
assert matching[0] == "temperature_final: checkpoint=0.25 -> explicit CLI=0.1", (
    f"unexpected fork line format: {matching[0]!r}"
)
anneal_forks = [fork for fork in last_entry["forks"] if fork.startswith("temperature_anneal:")]
assert not anneal_forks, (
    f"temperature_anneal must NOT appear in the fork log (never explicitly re-passed on the "
    f"override invocation, so it must inherit silently like temperature_steps does): "
    f"{anneal_forks}"
)

print("Native AlphaZero temperature-anneal semantic inheritance + explicit-override fork "
      f"visibility validated (fresh={fresh_cfg['temperature_final']}/"
      f"{fresh_cfg['temperature_anneal']}, resume inherited="
      f"{resume_cfg['temperature_final']}/{resume_cfg['temperature_anneal']}, override="
      f"{override_cfg['temperature_final']}/{override_cfg['temperature_anneal']}, "
      f"fork_line={matching[0]!r})")
