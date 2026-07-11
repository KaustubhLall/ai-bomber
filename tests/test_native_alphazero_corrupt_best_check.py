"""Resume must reject a best.pt whose tensors are from a different iteration than its lineage."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


executable = Path(sys.argv[1])
run_dir = Path(sys.argv[2])
best = run_dir / "best.pt"
latest = run_dir / "latest.pt"
backup = run_dir / "best.pt.ctest-backup"

shutil.copy2(best, backup)
try:
    # The fixture deliberately has latest at iteration 2 and the retained champion at 1.
    # Recreate the historical corruption: current weights stored under best.pt while the
    # selection metadata still names iteration 1.
    shutil.copy2(latest, best)
    command = [
        str(executable), "train",
        "--run-dir", str(run_dir),
        "--iterations", "2", "--games", "2", "--simulations", "2",
        "--train-steps", "1", "--batch-size", "4", "--replay-capacity", "64",
        "--channels", "8", "--blocks", "1",
        "--teacher-games", "1", "--teacher-iterations", "1",
        "--eval-interval", "1", "--eval-games", "1", "--eval-simulations", "2",
        "--promotion-games", "1", "--promotion-simulations", "2",
        "--random-score-floor", "0", "--heuristic-score-floor", "0",
        "--heuristic-regression-margin", "1",
        "--snapshot-interval", "1", "--max-steps", "8", "--no-progress",
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    combined = result.stdout + result.stderr
    assert result.returncode != 0, "resume accepted a mismatched/fake best.pt"
    assert "existing best.pt champion artifact does not match" in combined, combined
finally:
    shutil.copy2(backup, best)
    backup.unlink(missing_ok=True)

print("Native AlphaZero resume rejected mismatched best.pt champion tensors")
