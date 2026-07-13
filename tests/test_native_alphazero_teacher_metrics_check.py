"""v7 Stage-1 IL (docs/experiment-memory/14-v7-from-scratch-design.md Stage 1): collect_teacher()'s
per-iteration teacher-action histogram must be well-formed on every metrics.jsonl row of a run
whose whole horizon is inside the teacher window, and config.json / the fresh config-history row
must carry the roster this run was launched with.

The fixture is a single fresh tiny train (--teacher-agents heuristic,random --teacher-games 2
--teacher-iterations 2 --iterations 2 --eval-interval 1), so BOTH iterations (0 and 1, both below
teacher_iterations=2) collect teacher games and therefore emit a "teacher_actions" block.

Properties checked:
  (1) Every metrics.jsonl row carries a "teacher_actions" object with all six action keys
      (UP,DOWN,LEFT,RIGHT,BOMB,WAIT) plus bomb_fraction, bomb_fraction in [0,1].
  (2) The action counts are non-negative ints and bomb_fraction == BOMB/total (the documented
      denominator for the Stage-1 exit gate).
  (3) runtime_config_signature (config-history.jsonl fresh row) and config.json both record
      teacher_agents=heuristic,random - the roster this run was launched with.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ACTION_KEYS = ["UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"]

run_dir = Path(sys.argv[1])
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 2, (
    f"expected 2 metrics.jsonl rows (one per completed iteration of the fresh 2-iteration run, "
    f"both inside the teacher window), got {len(rows)}"
)

for row in rows:
    iteration = row.get("iteration")
    teacher = row.get("teacher_actions")
    assert teacher is not None, (
        f"iteration {iteration}: missing 'teacher_actions' - both iterations of this fixture are "
        f"below --teacher-iterations 2 with --teacher-games 2, so every row must carry it"
    )
    for key in ACTION_KEYS:
        assert key in teacher, (
            f"iteration {iteration}: teacher_actions missing key {key!r} (keys present: "
            f"{sorted(teacher.keys())})"
        )
        count = teacher[key]
        assert isinstance(count, int) and not isinstance(count, bool) and count >= 0, (
            f"iteration {iteration}: teacher_actions[{key!r}]={count!r} is not a non-negative int"
        )
    assert "bomb_fraction" in teacher, (
        f"iteration {iteration}: teacher_actions missing 'bomb_fraction'"
    )
    bomb_fraction = teacher["bomb_fraction"]
    assert isinstance(bomb_fraction, (int, float)) and not isinstance(bomb_fraction, bool), (
        f"iteration {iteration}: teacher_actions.bomb_fraction={bomb_fraction!r} is not numeric"
    )
    assert 0.0 <= float(bomb_fraction) <= 1.0, (
        f"iteration {iteration}: teacher_actions.bomb_fraction={bomb_fraction} outside [0,1]"
    )
    total = sum(teacher[key] for key in ACTION_KEYS)
    assert total > 0, (
        f"iteration {iteration}: teacher_actions total is 0 - the block is only emitted when "
        f"teacher games ran, and every game contributes at least one two-seat step"
    )
    expected_fraction = teacher["BOMB"] / total
    assert abs(float(bomb_fraction) - expected_fraction) < 1e-9, (
        f"iteration {iteration}: bomb_fraction={bomb_fraction} != BOMB/total={expected_fraction} "
        f"(BOMB={teacher['BOMB']}, total={total})"
    )


def parse_runtime_config(raw: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for pair in raw.split(";"):
        if "=" not in pair:
            continue
        key, _, value = pair.partition("=")
        result[key] = value
    return result


history = [
    json.loads(line)
    for line in (run_dir / "config-history.jsonl").read_text().splitlines()
]
assert history, "expected at least one config-history.jsonl row"
fresh_cfg = parse_runtime_config(history[0]["runtime_config"])
assert fresh_cfg.get("teacher_agents") == "heuristic,random", (
    f"fresh runtime_config teacher_agents={fresh_cfg.get('teacher_agents')!r}, expected "
    f"'heuristic,random' - the roster this run was launched with"
)

config = json.loads((run_dir / "config.json").read_text())
assert config.get("teacher_agents") == "heuristic,random", (
    f"config.json teacher_agents={config.get('teacher_agents')!r}, expected 'heuristic,random'"
)

print(
    "Native AlphaZero teacher-actions metrics validated across "
    f"{len(rows)} rows (bomb_fractions="
    f"{['%.3f' % float(r['teacher_actions']['bomb_fraction']) for r in rows]}, "
    f"teacher_agents=heuristic,random in runtime_config + config.json)"
)
