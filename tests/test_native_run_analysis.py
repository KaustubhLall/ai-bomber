import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.analyze_native_run import load_rows, summarize


with tempfile.TemporaryDirectory() as directory:
    run_dir = Path(directory)
    rows = [
        {"iteration": 3, "elapsed_seconds": 10, "self_play": {"wins": 1, "draws": 2, "losses": 1},
         "optimization": {"loss": 2.0, "entropy": 1.2}, "best_iteration": 2,
         "best_score": 0.6, "promotion_count": 1, "promoted": False},
        {"iteration": 4, "elapsed_seconds": 14, "self_play": {"wins": 2, "draws": 2, "losses": 0},
         "optimization": {"loss": 1.5, "entropy": 1.1}, "best_iteration": 4,
         "best_score": 0.7, "promotion_count": 2, "promoted": True,
         "heuristic": {"score": 0.7}, "incumbent": {"score": 0.6}},
    ]
    (run_dir / "metrics.jsonl").write_text(
        "".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
    assert len(load_rows(run_dir / "metrics.jsonl")) == 2
    result = summarize(run_dir, target_iteration=10, window=2)
    assert result["latest_iteration"] == 4
    assert result["remaining_iterations"] == 6
    assert result["mean_iteration_seconds"] == 12
    assert result["eta_seconds"] == 72
    assert result["loss_slope_per_iteration"] == -0.5
    assert result["mean_decisive_self_play_rate"] == 0.5
    assert result["latest_decisive_self_play_rate"] == 0.5
    assert result["decisive_rate_slope_per_iteration"] == 0.0
    assert result["latest_evaluation"]["iteration"] == 4

print("Native run analysis tests passed")
