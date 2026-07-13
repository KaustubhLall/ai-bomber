"""Summarize a recoverable native AlphaZero campaign from append-only metrics."""

from __future__ import annotations

import argparse
import json
import math
import statistics
from pathlib import Path


def load_rows(path: Path) -> list[dict]:
    rows: list[dict] = []
    if not path.exists():
        return rows
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(f"invalid JSONL row {line_number}: {error}") from error
        if not isinstance(row, dict) or "iteration" not in row:
            raise ValueError(f"metrics row {line_number} has no iteration")
        rows.append(row)
    return rows


def slope(values: list[float]) -> float | None:
    if len(values) < 2:
        return None
    x_mean = (len(values) - 1) / 2
    y_mean = statistics.fmean(values)
    denominator = sum((index - x_mean) ** 2 for index in range(len(values)))
    return (sum((index - x_mean) * (value - y_mean)
                for index, value in enumerate(values)) / denominator
            if denominator else None)


def summarize(run_dir: Path, target_iteration: int | None, window: int) -> dict:
    rows = load_rows(run_dir / "metrics.jsonl")
    if not rows:
        return {"run_dir": str(run_dir), "status": "no_metrics"}
    latest = rows[-1]
    recent = rows[-max(window, 1):]
    if target_iteration is None:
        config_path = run_dir / "config.json"
        target_iteration = (json.loads(config_path.read_text(encoding="utf-8"))
                            .get("iterations", latest["iteration"])
                            if config_path.exists() else latest["iteration"])
    elapsed = [float(row.get("elapsed_seconds", 0.0)) for row in recent]
    mean_elapsed = statistics.fmean(elapsed)
    remaining = max(int(target_iteration) - int(latest["iteration"]), 0)
    losses = [float(row.get("optimization", {}).get("loss", math.nan)) for row in recent]
    entropies = [float(row.get("optimization", {}).get("entropy", math.nan)) for row in recent]
    decisive_rates = []
    mean_steps = []
    for row in recent:
        self_play = row.get("self_play", {})
        total = sum(int(self_play.get(key, 0)) for key in ("wins", "draws", "losses"))
        decisive_rates.append(
            (int(self_play.get("wins", 0)) + int(self_play.get("losses", 0))) / total
            if total else 0.0)
        mean_steps.append(float(self_play.get("mean_steps", 0.0)))
    evaluations = [row for row in rows
                   if any(name in row for name in ("random", "heuristic", "incumbent", "mcts"))]
    promotions = [row for row in rows if row.get("promoted")]
    result = {
        "run_dir": str(run_dir),
        "status": "running_or_recoverable",
        "rows": len(rows),
        "latest_iteration": int(latest["iteration"]),
        "target_iteration": int(target_iteration),
        "remaining_iterations": remaining,
        "window": len(recent),
        "mean_iteration_seconds": mean_elapsed,
        "eta_seconds": mean_elapsed * remaining,
        "latest_loss": losses[-1],
        "loss_slope_per_iteration": slope(losses),
        "latest_entropy": entropies[-1],
        "entropy_slope_per_iteration": slope(entropies),
        "mean_decisive_self_play_rate": statistics.fmean(decisive_rates),
        "latest_decisive_self_play_rate": decisive_rates[-1],
        "decisive_rate_slope_per_iteration": slope(decisive_rates),
        "latest_mean_self_play_steps": mean_steps[-1],
        "mean_steps_slope_per_iteration": slope(mean_steps),
        "best_iteration": latest.get("best_iteration"),
        "best_score": latest.get("best_score"),
        "promotion_count": latest.get("promotion_count", len(promotions)),
        "evaluation_iterations": [int(row["iteration"]) for row in evaluations],
    }
    if evaluations:
        last_evaluation = evaluations[-1]
        result["latest_evaluation"] = {
            key: last_evaluation[key]
            for key in ("iteration", "random", "heuristic", "incumbent", "mcts",
                        "promoted", "promotion_reason")
            if key in last_evaluation
        }
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--target-iteration", type=int)
    parser.add_argument("--window", type=int, default=20)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = summarize(args.run_dir, args.target_iteration, args.window)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        temporary = args.output.with_name(args.output.name + ".tmp")
        temporary.parent.mkdir(parents=True, exist_ok=True)
        temporary.write_text(rendered, encoding="utf-8")
        temporary.replace(args.output)
    print(rendered, end="")


if __name__ == "__main__":
    main()
