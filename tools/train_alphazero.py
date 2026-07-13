"""CLI for recoverable AlphaZero-style training on the real C simulator."""

from __future__ import annotations

import argparse

from alphazero.env import TrainingLibrary
from alphazero.trainer import TrainConfig, Trainer


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", help="Path to bomber_training shared library")
    parser.add_argument("--run-dir", default="results/alphazero")
    parser.add_argument("--iterations", type=int, default=100,
                        help="Target total iterations; resumed runs continue to this value")
    parser.add_argument("--self-play-games", type=int, default=16)
    parser.add_argument("--teacher-games", type=int, default=2)
    parser.add_argument("--teacher-iterations", type=int, default=20)
    parser.add_argument("--teacher-type", choices=["heuristic", "mcts"], default="mcts")
    parser.add_argument("--adversary-games", type=int, default=4)
    parser.add_argument("--adversary-start-iteration", type=int, default=5)
    parser.add_argument("--adversary-win-repeats", type=int, default=4)
    parser.add_argument("--adversary-seed-pool-size", type=int, default=48)
    parser.add_argument("--adversary-type", choices=["heuristic", "mcts"], default="heuristic")
    parser.add_argument("--simulations", type=int, default=64)
    parser.add_argument("--train-steps", type=int, default=64)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--buffer-capacity", type=int, default=20_000)
    parser.add_argument("--hidden-size", type=int, default=96)
    parser.add_argument("--conv-channels", type=int, default=16)
    parser.add_argument("--learning-rate", type=float, default=1e-3)
    parser.add_argument("--learning-rate-decay", type=float, default=0.995)
    parser.add_argument("--bootstrap-value-weight", type=float, default=0.75)
    parser.add_argument("--bootstrap-value-iterations", type=int, default=20)
    parser.add_argument("--width", type=int, default=13)
    parser.add_argument("--height", type=int, default=11)
    parser.add_argument("--max-steps", type=int, default=200)
    parser.add_argument("--crate-density", type=int, default=50)
    parser.add_argument("--temperature-steps", type=int, default=30)
    parser.add_argument("--evaluation-interval", type=int, default=1)
    parser.add_argument("--evaluation-games", type=int, default=8)
    parser.add_argument("--evaluation-simulations", type=int, default=16)
    parser.add_argument("--mcts-evaluation-interval", type=int, default=5)
    parser.add_argument("--mcts-evaluation-games", type=int, default=4)
    parser.add_argument("--snapshot-interval", type=int, default=5)
    parser.add_argument("--promotion-margin", type=float, default=0.02)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--fresh", action="store_true",
                        help="Delete this run directory's existing checkpoints/metrics")
    parser.add_argument("--no-progress", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = TrainConfig(
        run_dir=args.run_dir, width=args.width, height=args.height,
        max_steps=args.max_steps, crate_density=args.crate_density,
        iterations=args.iterations, self_play_games=args.self_play_games,
        teacher_games=args.teacher_games, teacher_iterations=args.teacher_iterations,
        teacher_type=args.teacher_type,
        adversary_games=args.adversary_games,
        adversary_start_iteration=args.adversary_start_iteration,
        adversary_win_repeats=args.adversary_win_repeats,
        adversary_seed_pool_size=args.adversary_seed_pool_size,
        adversary_type=args.adversary_type,
        simulations=args.simulations, train_steps=args.train_steps,
        batch_size=args.batch_size, buffer_capacity=args.buffer_capacity,
        hidden_size=args.hidden_size, conv_channels=args.conv_channels,
        learning_rate=args.learning_rate,
        learning_rate_decay=args.learning_rate_decay,
        bootstrap_value_weight=args.bootstrap_value_weight,
        bootstrap_value_iterations=args.bootstrap_value_iterations,
        temperature_steps=args.temperature_steps,
        evaluation_interval=args.evaluation_interval,
        evaluation_games=args.evaluation_games,
        evaluation_simulations=args.evaluation_simulations,
        mcts_evaluation_interval=args.mcts_evaluation_interval,
        mcts_evaluation_games=args.mcts_evaluation_games,
        snapshot_interval=args.snapshot_interval,
        promotion_margin=args.promotion_margin, seed=args.seed,
        progress=not args.no_progress)
    Trainer(config, TrainingLibrary(args.library), fresh=args.fresh).run()


if __name__ == "__main__":
    main()
