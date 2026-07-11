"""KL-101 Part C regression: --fork-from must (1) inherit semantics from the parent, (2)
record complete, hash-verifiable provenance in fork-manifest.json, and (3) NOT re-trigger the
"iter-110 fake initial promotion" bug - a forked run's second-ever promotion check must see a
real incumbent (has_incumbent must not be blind to inherited lineage just because best.pt
didn't physically exist yet in the new run-dir)."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

parent_dir = Path(sys.argv[1])
child_dir = Path(sys.argv[2])

manifest = json.loads((child_dir / "fork-manifest.json").read_text())

parent_checkpoint = parent_dir / "latest.pt"
actual_sha256 = hashlib.sha256(parent_checkpoint.read_bytes()).hexdigest()
assert manifest["source_checkpoint_sha256"] == actual_sha256, (
    f"fork-manifest.json source_checkpoint_sha256={manifest['source_checkpoint_sha256']} "
    f"does not match the parent checkpoint's actual SHA-256 {actual_sha256}"
)
assert manifest["dirty_diff_digest"] == "ctest-regression-digest"
assert manifest["git_commit"], "fork-manifest.json must record the compiled-in git commit"
assert manifest["inherited_champion_lineage"]["best_iteration"] >= 0, (
    "a fork from a promoted parent must inherit a real (non-sentinel) best_iteration"
)

parent_best = parent_dir / "best.pt"
child_best = child_dir / "best.pt"
parent_best_sha256 = hashlib.sha256(parent_best.read_bytes()).hexdigest()
child_best_sha256 = hashlib.sha256(child_best.read_bytes()).hexdigest()
assert actual_sha256 != parent_best_sha256, (
    "fixture must advance latest.pt beyond best.pt or it cannot catch relabeling current "
    "weights as a historical champion"
)
assert child_best_sha256 == parent_best_sha256, (
    "forked child best.pt is not the exact parent champion artifact; current/latest weights "
    "must never be relabeled as an older inherited best_iteration"
)
assert manifest["source_champion_sha256"] == parent_best_sha256

child_config = json.loads((child_dir / "config.json").read_text())
assert abs(child_config["arena_crush_win_value"] - 0.22) < 1e-9, (
    f"child's resolved arena_crush_win_value={child_config['arena_crush_win_value']}, "
    f"expected 0.22 inherited from the parent (child never passed --arena-crush-win-value)"
)

assert (child_dir / "best.pt").is_file(), (
    "fork must preserve the exact parent best.pt before any promotion check"
)

manifest_semantics = dict(
    pair.split("=", 1) for pair in manifest["resolved_semantics"].split(";") if "=" in pair
)
assert int(manifest_semantics["learning_rate_schedule_updates"]) == int(
    child_config["learning_rate_schedule_updates"]
), "fork manifest must be written after LR horizon resolution and match config.json"

rows = [json.loads(line) for line in (child_dir / "metrics.jsonl").read_text().splitlines()]
promotion_reasons = [row["promotion_reason"] for row in rows if "promotion_reason" in row]
assert "promoted_initial_quality_gate" not in promotion_reasons, (
    f"child run's promotion reasons were {promotion_reasons} - "
    f"'promoted_initial_quality_gate' must never appear for a forked run with real "
    f"inherited lineage; this is exactly the iter-110 fake-promotion bug re-occurring"
)

print("Native AlphaZero fork provenance and exact champion inheritance validated "
      f"(source + champion SHA-256 verified, arena_crush_win_value=0.22 inherited, "
      f"promotion reasons={promotion_reasons})")
