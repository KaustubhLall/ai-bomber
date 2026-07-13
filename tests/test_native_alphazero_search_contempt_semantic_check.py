"""v7 Stage 0 item 0.4 (docs/experiment-memory/14-v7-from-scratch-design.md; SEARCH-CONTEMPT
PROTOTYPE): search_contempt_nscl must ride the FULL semantic manifest machinery exactly like
forced_playouts_k did - persisted, inherited on a plain resume (silently, no fork-log entry), and
explicit-override-logged as a SEMANTIC FORK when a later resume's CLI value diverges from the
checkpoint's stored manifest. This is a manifest-plumbing claim only: unlike forced_playouts_k,
search_contempt_nscl has NO effect on what any of these three `train` invocations actually
compute (no training call site ever sets SearchConstraint::contempt_seat - see
gate_search_constraint(), the field's only construction site) - see
test_native_alphazero_gates_contempt_check.py for the claim that the field is behaviorally live
somewhere (through the `gates` subcommand's --gates-search-contempt).

The fixture is a 3-invocation chain into ONE run-dir:
  1. fresh:    --search-contempt-nscl 5                       (iteration 0->1)
  2. resume:   no search-contempt flag at all - must inherit search_contempt_nscl=5 silently
               (iteration 1->2)
  3. override: --search-contempt-nscl 7 - must log a SEMANTIC FORK line for
               search_contempt_nscl (iteration 2->3)

config-history.jsonl accumulates one row per PROCESS invocation (appended in the constructor,
before that process's own training loop advances `iteration` further), so its "runtime_config"
field (runtime_config_signature(config) - a semicolon key=value string) gives a direct snapshot
of what apply_semantic_manifest() resolved at the START of each of the three invocations above -
exactly the "inherited vs overridden" moment this test needs, without splitting into three
separate ctest steps. Structurally mirrors test_native_alphazero_forced_playouts_semantic_check.py,
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
assert int(fresh_cfg["search_contempt_nscl"]) == 5, (
    f"fresh runtime_config search_contempt_nscl={fresh_cfg.get('search_contempt_nscl')!r}, "
    f"expected 5"
)

# (2) Plain resume, no search-contempt flag at all: inherited from the checkpoint manifest, not
# silently reset to the compiled default (search_contempt_nscl=0) the way a pre-existing KL-101
# bug would have done.
assert int(resume_cfg["search_contempt_nscl"]) == 5, (
    f"resume (no override) runtime_config search_contempt_nscl="
    f"{resume_cfg.get('search_contempt_nscl')!r}, expected inherited 5 - a plain resume must "
    f"not silently fall back to the compiled default 0"
)

# (3) Explicit override resume: search_contempt_nscl follows the CLI (7), not the inherited 5.
assert int(override_cfg["search_contempt_nscl"]) == 7, (
    f"override runtime_config search_contempt_nscl={override_cfg.get('search_contempt_nscl')!r}, "
    f"expected the explicit CLI value 7, not the inherited 5"
)

# (4) Final config.json (written by the override invocation, the last one) mirrors the same
# resolved state - the config JSON dump must carry the field like forced_playouts_k does.
config = json.loads((run_dir / "config.json").read_text())
assert config["search_contempt_nscl"] == 7, (
    f"config.json search_contempt_nscl={config.get('search_contempt_nscl')!r}, expected 7"
)

# (5) SEMANTIC FORK visibility: the override invocation is the ONLY one of the three that passes
# any explicit semantic flag (--search-contempt-nscl 7, diverging from the checkpoint's stored
# 5) - exactly one semantic-fork-log.jsonl line should exist for the whole chain (proving BOTH
# that the fresh run's --fresh truncation left it empty beforehand, and that the plain resume in
# step 2 contributed nothing - "fork-log silent" on inherit).
fork_log_path = run_dir / "semantic-fork-log.jsonl"
assert fork_log_path.exists(), (
    "expected semantic-fork-log.jsonl to exist after the explicit --search-contempt-nscl 7 "
    "override (checkpoint=5 -> explicit CLI=7 must be logged as a SEMANTIC FORK, exactly like "
    "any other semantic field's explicit override)"
)
fork_log_lines = fork_log_path.read_text().splitlines()
assert len(fork_log_lines) == 1, (
    f"expected exactly 1 semantic-fork-log.jsonl line across this fresh+resume+override chain "
    f"(only the override invocation forks anything), got {len(fork_log_lines)} - either --fresh "
    f"stopped truncating this file, or the plain resume in step 2 unexpectedly forked something"
)
last_entry = json.loads(fork_log_lines[-1])
matching = [fork for fork in last_entry["forks"] if fork.startswith("search_contempt_nscl:")]
assert matching, (
    f"no search_contempt_nscl fork line in the semantic-fork-log.jsonl entry (saw: "
    f"{last_entry['forks']}) - the explicit --search-contempt-nscl 7 override against a "
    f"checkpoint manifest value of 5 must be logged"
)
assert matching[0] == "search_contempt_nscl: checkpoint=5 -> explicit CLI=7", (
    f"unexpected fork line format: {matching[0]!r}"
)

print("Native AlphaZero search-contempt-nscl semantic inheritance + explicit-override fork "
      f"visibility validated (fresh={fresh_cfg['search_contempt_nscl']}, "
      f"resume inherited={resume_cfg['search_contempt_nscl']}, "
      f"override={override_cfg['search_contempt_nscl']}, fork_line={matching[0]!r})")
