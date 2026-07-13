"""v7 Stage-1 IL (D3): collect_teacher()'s OpenMP parallelization must be BIT-IDENTICAL to the
serial path for any thread count - proven by test, not asserted. This is the determinism oracle
test named in TrainConfig::teacher_threads's own doc comment (trainer.h) and in
collect_teacher()'s doc comment (trainer.cpp): the license for keeping teacher_threads OUT of
runtime_config_signature()/the manifest/semantic_field_flags() is that it can never change what a
checkpoint's training data looked like, only how fast it was collected.

The fixture is TWO fresh tiny trains into separate run-dirs, identical flags except
--teacher-threads 1 vs --teacher-threads 4 (--teacher-agents heuristic,random --teacher-games 4
--teacher-iterations 2 --iterations 2 --eval-interval 1), so both iterations (0 and 1, both below
teacher_iterations=2) collect teacher games and emit a "teacher_actions" + "teacher_sample_digest"
metrics block. heuristic/random keeps this fast and deterministic in CI; mcts teacher thread-safety
runs the exact same collect_teacher() parallel-for code path and is exercised only via a local,
out-of-CI smoke comparison (not this test - see the D3 implementation notes).

Properties checked, per iteration (matched by the "iteration" key, not row position):
  (1) Both runs' metrics.jsonl carry a well-formed teacher_sample_digest (16-char lowercase hex,
      the FNV-1a 64-bit digest) and a well-formed teacher_actions block (all six action keys plus
      bomb_fraction).
  (2) teacher_sample_digest is IDENTICAL between the threads=1 and threads=4 runs.
  (3) teacher_actions is IDENTICAL (every key, exact value) between the two runs.
  (4) config.json for each run records teacher_threads at the value that run was launched with
      (1 and 4 respectively) - the non-semantic field still round-trips through the full config
      dump.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ACTION_KEYS = ["UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"]
HEX_DIGITS = set("0123456789abcdef")


def load_rows(run_dir: Path) -> dict[int, dict]:
    rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
    by_iteration = {row["iteration"]: row for row in rows}
    assert len(by_iteration) == len(rows), (
        f"{run_dir}: duplicate 'iteration' keys in metrics.jsonl ({[r['iteration'] for r in rows]})"
    )
    return by_iteration


def assert_well_formed_teacher_block(run_dir: Path, iteration: int, row: dict) -> None:
    teacher = row.get("teacher_actions")
    assert teacher is not None, (
        f"{run_dir}: iteration {iteration} missing 'teacher_actions' - both iterations of this "
        f"fixture are below --teacher-iterations 2 with --teacher-games 4, so every row must "
        f"carry it"
    )
    for key in ACTION_KEYS:
        assert key in teacher, (
            f"{run_dir}: iteration {iteration} teacher_actions missing key {key!r} (keys present: "
            f"{sorted(teacher.keys())})"
        )
        count = teacher[key]
        assert isinstance(count, int) and not isinstance(count, bool) and count >= 0, (
            f"{run_dir}: iteration {iteration} teacher_actions[{key!r}]={count!r} is not a "
            f"non-negative int"
        )
    assert "bomb_fraction" in teacher, (
        f"{run_dir}: iteration {iteration} teacher_actions missing 'bomb_fraction'"
    )

    digest = row.get("teacher_sample_digest")
    assert isinstance(digest, str) and len(digest) == 16 and set(digest) <= HEX_DIGITS, (
        f"{run_dir}: iteration {iteration} teacher_sample_digest={digest!r} is not a 16-char "
        f"lowercase hex string"
    )


run_dir_1 = Path(sys.argv[1])
run_dir_4 = Path(sys.argv[2])

rows_1 = load_rows(run_dir_1)
rows_4 = load_rows(run_dir_4)

assert set(rows_1.keys()) == set(rows_4.keys()), (
    f"the two runs produced different iteration sets: threads=1 has {sorted(rows_1.keys())}, "
    f"threads=4 has {sorted(rows_4.keys())} - they must be the same fixture run to the same "
    f"horizon for a digest comparison to mean anything"
)
assert len(rows_1) == 2, (
    f"expected 2 metrics.jsonl rows (one per completed iteration of the fresh 2-iteration "
    f"fixture, both inside the teacher window), got {sorted(rows_1.keys())}"
)

mismatches: list[str] = []
for iteration in sorted(rows_1.keys()):
    row_1 = rows_1[iteration]
    row_4 = rows_4[iteration]
    assert_well_formed_teacher_block(run_dir_1, iteration, row_1)
    assert_well_formed_teacher_block(run_dir_4, iteration, row_4)

    digest_1 = row_1["teacher_sample_digest"]
    digest_4 = row_4["teacher_sample_digest"]
    if digest_1 != digest_4:
        mismatches.append(
            f"iteration {iteration}: teacher_sample_digest differs - threads=1 {digest_1!r} != "
            f"threads=4 {digest_4!r}"
        )

    actions_1 = row_1["teacher_actions"]
    actions_4 = row_4["teacher_actions"]
    for key in ACTION_KEYS + ["bomb_fraction"]:
        if actions_1[key] != actions_4[key]:
            mismatches.append(
                f"iteration {iteration}: teacher_actions[{key!r}] differs - threads=1 "
                f"{actions_1[key]!r} != threads=4 {actions_4[key]!r}"
            )

assert not mismatches, (
    "collect_teacher() output is NOT thread-count-invariant - the parallel path has diverged "
    "from serial (or two independently-scheduled parallel runs have diverged from each other), "
    "which breaks the whole premise that teacher_threads is a non-semantic performance knob:\n  "
    + "\n  ".join(mismatches)
)

config_1 = json.loads((run_dir_1 / "config.json").read_text())
config_4 = json.loads((run_dir_4 / "config.json").read_text())
assert config_1.get("teacher_threads") == 1, (
    f"{run_dir_1}: config.json teacher_threads={config_1.get('teacher_threads')!r}, expected 1"
)
assert config_4.get("teacher_threads") == 4, (
    f"{run_dir_4}: config.json teacher_threads={config_4.get('teacher_threads')!r}, expected 4"
)

print(
    "Native AlphaZero collect_teacher() threads=1 vs threads=4 determinism validated across "
    f"{len(rows_1)} iterations (digests="
    f"{[rows_1[i]['teacher_sample_digest'] for i in sorted(rows_1.keys())]}, "
    "config.json teacher_threads=1/4 confirmed)"
)
