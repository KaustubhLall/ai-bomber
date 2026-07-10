"""Evaluate a saved AlphaZero checkpoint on fixed two-seat seed blocks."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import fields
from pathlib import Path

import numpy as np

from alphazero.checkpoint import ReplayBuffer, load_checkpoint
from alphazero.env import TrainingLibrary
from alphazero.model import PolicyValueNet
from alphazero.progress import ProgressBar
from alphazero.trainer import TrainConfig, Trainer


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", help="Path to bomber_training shared library")
    parser.add_argument("--run-dir", default="results/alphazero-conv-main")
    parser.add_argument("--checkpoint", default="best.npz",
                        help="Checkpoint filename within run-dir, or an explicit path")
    parser.add_argument("--opponents", default="random,heuristic,mcts",
                        help="Comma-separated subset of random,heuristic,mcts")
    parser.add_argument("--seed-start", type=int, default=150001)
    parser.add_argument("--seeds", type=int, default=16)
    parser.add_argument("--simulations", type=int, default=8)
    parser.add_argument("--output", help="Optional JSON output path")
    parser.add_argument("--no-progress", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    run_dir = Path(args.run_dir)
    checkpoint = Path(args.checkpoint)
    if not checkpoint.is_file():
        checkpoint = run_dir / checkpoint
    if not checkpoint.is_file():
        raise FileNotFoundError(checkpoint)

    with np.load(checkpoint, allow_pickle=False) as data:
        stored_config = json.loads(str(data["config_json"]))
        checkpoint_iteration = int(data["iteration"])
    allowed = {field.name for field in fields(TrainConfig)}
    values = {key: value for key, value in stored_config.items() if key in allowed}
    values.update(run_dir=str(run_dir), evaluation_simulations=args.simulations,
                  progress=not args.no_progress)
    config = TrainConfig(**values)
    trainer = Trainer(config, TrainingLibrary(args.library))
    model = PolicyValueNet(trainer.model.input_size, config.hidden_size,
                           conv_channels=config.conv_channels)
    load_checkpoint(checkpoint, model, ReplayBuffer(1), stored_config,
                    np.random.default_rng(0))
    trainer.model = model

    opponents = [item.strip() for item in args.opponents.split(",") if item.strip()]
    invalid = sorted(set(opponents) - {"random", "heuristic", "mcts"})
    if invalid:
        raise ValueError(f"unsupported opponents: {', '.join(invalid)}")
    seeds = list(range(args.seed_start, args.seed_start + args.seeds))
    progress = ProgressBar("audit", len(seeds) * 2 * len(opponents), "games",
                           not args.no_progress)
    completed = 0
    results: dict[str, dict] = {}
    for name in opponents:
        results[name], completed = trainer._evaluate_opponent(
            None if name == "random" else name, seeds, progress, completed)

    report = {
        "schema_version": 1,
        "checkpoint": str(checkpoint),
        "checkpoint_iteration": checkpoint_iteration,
        "checkpoint_sha256": hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
        "seed_start": args.seed_start,
        "seed_count": args.seeds,
        "two_seat_games_per_opponent": args.seeds * 2,
        "evaluation_simulations": args.simulations,
        "results": results,
    }
    rendered = json.dumps(report, indent=2)
    print(rendered)
    if args.output:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
