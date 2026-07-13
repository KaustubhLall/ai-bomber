"""v7 Stage-1 IL (docs/experiment-memory/14-v7-from-scratch-design.md Stage 1): teacher_agents
must ride the FULL semantic manifest machinery exactly like forced_playouts_k / temperature_final
- persisted, inherited on a plain resume (silently, no fork-log entry), and explicit-override-
logged as a SEMANTIC FORK when a later resume's CLI value diverges from the checkpoint's stored
manifest. teacher_agents is a std::string semantic field (the first one), reconciled by
reconcile_string in apply_semantic_manifest().

The fixture is a 3-invocation chain into ONE run-dir:
  1. fresh:    --teacher-agents heuristic,random             (iteration 0->1)
  2. resume:   no teacher-agents flag at all - must inherit heuristic,random silently
               (iteration 1->2)
  3. override: --teacher-agents random,heuristic - must log a SEMANTIC FORK line
               (iteration 2->3)

Structurally mirrors test_native_alphazero_forced_playouts_semantic_check.py, which established
this pattern for the previous v7 semantic-field addition; the only difference is the field is a
categorical roster string, compared exactly rather than with float slack.
"""

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

# (1) Fresh run: runtime_config_signature carries the field at its explicit CLI value.
assert fresh_cfg.get("teacher_agents") == "heuristic,random", (
    f"fresh runtime_config teacher_agents={fresh_cfg.get('teacher_agents')!r}, expected "
    f"'heuristic,random'"
)

# (2) Plain resume, no teacher-agents flag at all: inherited from the checkpoint manifest, not
# silently reset to the compiled default ("heuristic").
assert resume_cfg.get("teacher_agents") == "heuristic,random", (
    f"resume (no override) runtime_config teacher_agents={resume_cfg.get('teacher_agents')!r}, "
    f"expected inherited 'heuristic,random' - a plain resume must not silently fall back to the "
    f"compiled default 'heuristic'"
)

# (3) Explicit override resume: teacher_agents follows the CLI, not the inherited value.
assert override_cfg.get("teacher_agents") == "random,heuristic", (
    f"override runtime_config teacher_agents={override_cfg.get('teacher_agents')!r}, expected the "
    f"explicit CLI value 'random,heuristic', not the inherited 'heuristic,random'"
)

# (4) Final config.json (written by the override invocation, the last one) mirrors the resolved
# state - the config JSON dump must carry the field.
config = json.loads((run_dir / "config.json").read_text())
assert config.get("teacher_agents") == "random,heuristic", (
    f"config.json teacher_agents={config.get('teacher_agents')!r}, expected 'random,heuristic'"
)

# (5) SEMANTIC FORK visibility: only the override invocation passes an explicit semantic flag
# (--teacher-agents random,heuristic, diverging from the checkpoint's stored heuristic,random) -
# exactly one semantic-fork-log.jsonl line should exist for the whole chain.
fork_log_path = run_dir / "semantic-fork-log.jsonl"
assert fork_log_path.exists(), (
    "expected semantic-fork-log.jsonl to exist after the explicit --teacher-agents override "
    "(checkpoint=heuristic,random -> explicit CLI=random,heuristic must be logged)"
)
fork_log_lines = fork_log_path.read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl line across this fresh+resume+override chain "
    f"(only the override invocation forks anything), got {len(fork_log_lines)}"
)
last_entry = json.loads(fork_log_lines[-1])
matching = [fork for fork in last_entry["forks"] if fork.startswith("teacher_agents:")]
assert matching, (
    f"no teacher_agents fork line in the semantic-fork-log.jsonl entry (saw: {last_entry['forks']})"
)
assert matching[0] == "teacher_agents: checkpoint=heuristic,random -> explicit CLI=random,heuristic", (
    f"unexpected fork line format: {matching[0]!r}"
)

print(
    "Native AlphaZero teacher_agents semantic inheritance + explicit-override fork visibility "
    f"validated (fresh={fresh_cfg['teacher_agents']}, resume inherited={resume_cfg['teacher_agents']}, "
    f"override={override_cfg['teacher_agents']}, fork_line={matching[0]!r})"
)
