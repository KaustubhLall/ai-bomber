"""KL-101 Part D regression: a second `train` process must be refused (clear error, non-zero
exit) while a first one is still running - even against a DIFFERENT run directory. This is the
actual failure mode that crashed training twice earlier in this project's history (two full
training loops on different run-dirs silently contending for one GPU, both killed with no
in-app error trace). Doesn't fit the existing chained-CTest smoke/resume/gate pattern (that
pattern is sequential dependencies on one run-dir; this needs a genuinely concurrent second
process), so it's a self-contained script that manages its own background process."""

from __future__ import annotations

import shutil
import subprocess
import sys
import time
from pathlib import Path

exe = Path(sys.argv[1])
run_dir_a = Path(sys.argv[2])
run_dir_b = Path(sys.argv[3])

for d in (run_dir_a, run_dir_b):
    if d.exists():
        shutil.rmtree(d)

slow_args = [
    str(exe), "train", "--run-dir", str(run_dir_a), "--fresh",
    "--iterations", "200", "--games", "8", "--simulations", "32",
    "--train-steps", "8", "--batch-size", "16", "--replay-capacity", "512",
    "--channels", "32", "--blocks", "2", "--teacher-games", "2", "--teacher-iterations", "1",
    "--eval-interval", "1000", "--eval-games", "1", "--eval-simulations", "2",
    "--promotion-games", "1", "--promotion-simulations", "2",
    "--random-score-floor", "0", "--heuristic-score-floor", "0",
    "--heuristic-regression-margin", "1", "--snapshot-interval", "50",
    "--max-steps", "32", "--no-progress",
]

background = subprocess.Popen(slow_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True)
try:
    deadline = time.time() + 30
    while time.time() < deadline:
        if (run_dir_a / "latest.pt").exists():
            break
        assert background.poll() is None, (
            f"background trainer exited early (code {background.returncode}) before "
            f"writing a first checkpoint - cannot test concurrency"
        )
        time.sleep(0.5)
    else:
        raise AssertionError("background trainer never wrote latest.pt within 30s")

    assert background.poll() is None, "background trainer exited before the concurrency check"

    second_args = [
        str(exe), "train", "--run-dir", str(run_dir_b), "--fresh",
        "--iterations", "1", "--games", "2", "--simulations", "2",
        "--train-steps", "1", "--batch-size", "4", "--replay-capacity", "64",
        "--channels", "8", "--blocks", "1", "--teacher-games", "1", "--teacher-iterations", "1",
        "--eval-interval", "1", "--eval-games", "1", "--eval-simulations", "2",
        "--promotion-games", "1", "--promotion-simulations", "2",
        "--random-score-floor", "0", "--heuristic-score-floor", "0",
        "--heuristic-regression-margin", "1", "--snapshot-interval", "1",
        "--max-steps", "8", "--no-progress",
    ]
    second = subprocess.run(second_args, capture_output=True, text=True, timeout=30)
    assert second.returncode != 0, (
        "a second `train` process on a DIFFERENT run-dir was allowed to start while the "
        "first was still running - this is the exact GPU-contention failure mode that "
        "crashed training twice earlier in this project's history, now silently unguarded"
    )
    combined_output = second.stdout + second.stderr
    assert "cannot acquire exclusive lock" in combined_output, (
        f"second train process failed (good) but not with the expected lock error - "
        f"got: {combined_output!r}"
    )
finally:
    background.terminate()
    try:
        background.wait(timeout=10)
    except subprocess.TimeoutExpired:
        background.kill()
        background.wait(timeout=10)
    for d in (run_dir_a, run_dir_b):
        if d.exists():
            shutil.rmtree(d, ignore_errors=True)

print("Native AlphaZero single-trainer lock validated "
      "(second concurrent train process correctly refused with a clear error)")
