"""Validate the native smoke run exercised champion selection semantics."""

from __future__ import annotations

import json
import sys
from pathlib import Path


run_dir = Path(sys.argv[1])
rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert [row["iteration"] for row in rows] == [1, 2]
assert all(row["schema_version"] == 2 for row in rows)
assert rows[0]["promoted"]
assert rows[0]["promotion_reason"] == "promoted_initial_quality_gate"
assert rows[0]["best_iteration"] == 1
assert "incumbent" in rows[1]
assert rows[1]["promotion_reason"] in {
    "incumbent_confidence_gate_failed",
    "promoted_incumbent_confidence_gate",
}
assert rows[1]["incumbent"]["lower_confidence_bound"] <= rows[1]["incumbent"]["score"]
assert sum(seat["wins"] + seat["draws"] + seat["losses"]
           for seat in rows[1]["incumbent"]["by_seat"]) == 2
selection = rows[1]["selection"]
evaluation_end = selection["evaluation_seed_base"] + 1
promotion_end = selection["promotion_seed_base"] + selection["promotion_games"]
assert evaluation_end <= selection["promotion_seed_base"] or \
       promotion_end <= selection["evaluation_seed_base"]
assert (run_dir / "best.pt").is_file()
assert (run_dir / "latest.pt").is_file()

print("Native AlphaZero champion gate exercised and validated")
