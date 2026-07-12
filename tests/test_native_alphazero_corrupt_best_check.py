"""Resume must reject a best.pt whose tensors are from a different iteration than its lineage;
forking such a corrupted parent must also fail closed by default, and succeed with an explicit
--fork-reset-champion whose child gets a self-consistent, fork-point champion (the sanctioned
escape hatch for historically inconsistent parents - added when exactly this corruption was
found in the wild in control03's run dir at the KL-105 Phase 3 arm launch, a leftover of the
pre-KL-101 relabeling bug that the fork validation correctly refused to inherit)."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path


executable = Path(sys.argv[1])
run_dir = Path(sys.argv[2])
best = run_dir / "best.pt"
latest = run_dir / "latest.pt"
backup = run_dir / "best.pt.ctest-backup"
fork_child = run_dir.parent / (run_dir.name + "-reset-champion-fork")

BASE_FLAGS = [
    "--games", "2", "--simulations", "2",
    "--train-steps", "1", "--batch-size", "4", "--replay-capacity", "64",
    "--channels", "8", "--blocks", "1",
    "--teacher-games", "1", "--teacher-iterations", "1",
    "--eval-interval", "1", "--eval-games", "1", "--eval-simulations", "2",
    "--promotion-games", "1", "--promotion-simulations", "2",
    "--random-score-floor", "0", "--heuristic-score-floor", "0",
    "--heuristic-regression-margin", "1",
    "--snapshot-interval", "1", "--max-steps", "8", "--no-progress",
]

shutil.copy2(best, backup)
try:
    # The fixture deliberately has latest at iteration 2 and the retained champion at 1.
    # Recreate the historical corruption: current weights stored under best.pt while the
    # selection metadata still names iteration 1.
    shutil.copy2(latest, best)

    # 1. Resuming the corrupted run dir itself fails closed (the original check).
    result = subprocess.run(
        [str(executable), "train", "--run-dir", str(run_dir), "--iterations", "2"]
        + BASE_FLAGS, capture_output=True, text=True, check=False)
    combined = result.stdout + result.stderr
    assert result.returncode != 0, "resume accepted a mismatched/fake best.pt"
    assert "existing best.pt champion artifact does not match" in combined, combined

    # 2. FORKING the corrupted parent also fails closed by default - the exact production
    # failure the KL-105 arm bootstrap hit against control03's historical best.pt.
    if fork_child.exists():
        shutil.rmtree(fork_child)
    result = subprocess.run(
        [str(executable), "train", "--run-dir", str(fork_child),
         "--fresh", "--fork-from", str(latest), "--iterations", "3"]
        + BASE_FLAGS, capture_output=True, text=True, check=False)
    combined = result.stdout + result.stderr
    assert result.returncode != 0, "fork inherited a mismatched parent champion"
    assert "parent champion artifact does not match" in combined, combined

    # 3. The same fork WITH --fork-reset-champion succeeds: the child's champion history
    # starts at its own fork point instead of inheriting the parent's corruption.
    if fork_child.exists():
        shutil.rmtree(fork_child)
    result = subprocess.run(
        [str(executable), "train", "--run-dir", str(fork_child),
         "--fresh", "--fork-from", str(latest), "--fork-reset-champion",
         "--iterations", "3"] + BASE_FLAGS, capture_output=True, text=True, check=False)
    combined = result.stdout + result.stderr
    assert result.returncode == 0, f"--fork-reset-champion fork failed:\n{combined}"
    assert "Champion lineage RESET at fork" in combined, combined
    manifest = json.loads((fork_child / "fork-manifest.json").read_text())
    assert manifest["champion_reset"] is True, manifest
    # Fork point is the fixture's iteration 2; the reset lineage must be self-consistent
    # (best_iteration == the fork iteration, zeroed score/promotions).
    lineage = manifest["inherited_champion_lineage"]
    assert lineage["best_iteration"] == 2, lineage
    assert lineage["promotion_count"] == 0, lineage
    assert (fork_child / "best.pt").exists(), "reset fork wrote no best.pt"

    # 4. And the child's own lineage stays healthy: a plain resume of the reset fork passes
    # reconcile_or_restore_champion (which would reject a copied-parent best.pt whose embedded
    # selection metadata still carried the parent's inconsistent champion claims).
    result = subprocess.run(
        [str(executable), "train", "--run-dir", str(fork_child), "--iterations", "4"]
        + BASE_FLAGS, capture_output=True, text=True, check=False)
    combined = result.stdout + result.stderr
    assert result.returncode == 0, f"resume of the reset fork failed:\n{combined}"
finally:
    shutil.copy2(backup, best)
    backup.unlink(missing_ok=True)
    if fork_child.exists():
        shutil.rmtree(fork_child, ignore_errors=True)

print("Native AlphaZero champion checks: corrupt resume rejected, corrupt-parent fork "
      "rejected, --fork-reset-champion fork + its resume both healthy")
