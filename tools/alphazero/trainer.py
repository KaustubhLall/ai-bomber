"""Self-play, optimization, evaluation, checkpointing, and recovery orchestration."""

from __future__ import annotations

import json
import os
import signal
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from .checkpoint import ReplayBuffer, copy_checkpoint, load_checkpoint, save_checkpoint
from .env import ABI_VERSION, ACTIONS, CHANNELS, BomberEnv, TrainingLibrary
from .mcts import JointMCTS, masked_policy, sample_joint
from .model import PolicyValueNet
from .progress import ProgressBar


NATIVE_AGENT_TYPES = {"heuristic": 2, "mcts": 7}


@dataclass
class TrainConfig:
    run_dir: str = "results/alphazero"
    width: int = 13
    height: int = 11
    channels: int = CHANNELS
    abi_version: int = ABI_VERSION
    max_steps: int = 200
    crate_density: int = 50
    iterations: int = 100
    self_play_games: int = 16
    teacher_games: int = 2
    teacher_iterations: int = 20
    teacher_type: str = "mcts"
    adversary_games: int = 4
    adversary_start_iteration: int = 5
    adversary_win_repeats: int = 4
    adversary_seed_pool_size: int = 48
    adversary_type: str = "heuristic"
    simulations: int = 64
    train_steps: int = 64
    batch_size: int = 128
    buffer_capacity: int = 20_000
    hidden_size: int = 96
    conv_channels: int = 16
    learning_rate: float = 1e-3
    learning_rate_decay: float = 0.995
    c_puct: float = 1.5
    bootstrap_value_weight: float = 0.75
    bootstrap_value_iterations: int = 20
    dirichlet_alpha: float = 0.3
    dirichlet_fraction: float = 0.25
    temperature: float = 1.0
    temperature_steps: int = 30
    evaluation_interval: int = 1
    evaluation_games: int = 8
    evaluation_simulations: int = 16
    mcts_evaluation_interval: int = 5
    mcts_evaluation_games: int = 4
    snapshot_interval: int = 5
    promotion_margin: float = 0.02
    seed: int = 1
    progress: bool = True


class Trainer:
    def __init__(self, config: TrainConfig, library: TrainingLibrary, fresh: bool = False):
        self._validate_config(config)
        self.config = config
        self.library = library
        self.run_dir = Path(config.run_dir)
        self.run_dir.mkdir(parents=True, exist_ok=True)
        probe = library.create(config.width, config.height, config.max_steps,
                               config.crate_density, config.seed)
        input_size = probe.observation_size
        probe.close()
        self.model = PolicyValueNet(input_size, config.hidden_size, config.seed,
                                    config.conv_channels)
        self.replay = ReplayBuffer(config.buffer_capacity)
        self.rng = np.random.default_rng(config.seed)
        self.iteration = 0
        self.global_steps = 0
        self.best_score = float("-inf")
        self.last_evaluation: dict = {}
        self.stop_requested = False
        self.latest_path = self.run_dir / "latest.npz"
        self.best_path = self.run_dir / "best.npz"
        self.metrics_path = self.run_dir / "metrics.jsonl"
        if self.latest_path.exists() and not fresh:
            state = load_checkpoint(self.latest_path, self.model, self.replay,
                                    asdict(config), self.rng)
            self.iteration = state["iteration"]
            self.global_steps = state["global_steps"]
            self.best_score = state["best_score"]
            self.last_evaluation = state["evaluation"]
            print(f"Resumed {self.latest_path} at iteration {self.iteration} "
                  f"with {len(self.replay)} replay samples")
        elif fresh:
            for path in self.run_dir.glob("*.npz"):
                path.unlink()
            if self.metrics_path.exists():
                self.metrics_path.unlink()

    @staticmethod
    def _validate_config(config: TrainConfig) -> None:
        if not (5 <= config.width <= 31 and config.width % 2 == 1 and
                5 <= config.height <= 31 and config.height % 2 == 1):
            raise ValueError("width and height must be odd values between 5 and 31")
        positive = {
            "self_play_games": config.self_play_games, "simulations": config.simulations,
            "batch_size": config.batch_size, "buffer_capacity": config.buffer_capacity,
            "hidden_size": config.hidden_size, "conv_channels": config.conv_channels,
            "max_steps": config.max_steps,
            "evaluation_interval": config.evaluation_interval,
            "evaluation_games": config.evaluation_games,
            "evaluation_simulations": config.evaluation_simulations,
            "mcts_evaluation_interval": config.mcts_evaluation_interval,
            "mcts_evaluation_games": config.mcts_evaluation_games,
            "snapshot_interval": config.snapshot_interval,
        }
        invalid = [name for name, value in positive.items() if value <= 0]
        if invalid:
            raise ValueError(f"configuration values must be positive: {', '.join(invalid)}")
        if config.iterations < 0 or config.train_steps < 0:
            raise ValueError("iterations and train_steps cannot be negative")
        if config.teacher_games < 0 or config.teacher_iterations < 0:
            raise ValueError("teacher_games and teacher_iterations cannot be negative")
        if config.adversary_games < 0 or config.adversary_start_iteration < 0:
            raise ValueError("adversary curriculum values cannot be negative")
        if config.adversary_win_repeats <= 0:
            raise ValueError("adversary_win_repeats must be positive")
        if config.adversary_seed_pool_size <= 0:
            raise ValueError("adversary_seed_pool_size must be positive")
        if config.adversary_type not in NATIVE_AGENT_TYPES:
            raise ValueError(f"adversary_type must be one of: {', '.join(NATIVE_AGENT_TYPES)}")
        if config.teacher_type not in NATIVE_AGENT_TYPES:
            raise ValueError(f"teacher_type must be one of: {', '.join(NATIVE_AGENT_TYPES)}")
        if not 0 <= config.crate_density <= 100:
            raise ValueError("crate_density must be between 0 and 100")
        if config.learning_rate <= 0 or not 0 < config.learning_rate_decay <= 1:
            raise ValueError("learning rate must be positive and decay must be in (0, 1]")
        if not 0 <= config.bootstrap_value_weight <= 1:
            raise ValueError("bootstrap_value_weight must be between 0 and 1")

    def _mcts(self, fixed_opponent_type: int | None = None,
              fixed_opponent_seat: int | None = None, opponent_seed: int = 1) -> JointMCTS:
        progress = min(self.iteration / max(self.config.bootstrap_value_iterations, 1), 1.0)
        heuristic_weight = self.config.bootstrap_value_weight * (1.0 - progress)
        return JointMCTS(self.model, self.config.simulations, self.config.c_puct,
                         self.config.dirichlet_alpha, self.config.dirichlet_fraction,
                         self.rng, heuristic_weight=heuristic_weight,
                         fixed_opponent_type=fixed_opponent_type,
                         fixed_opponent_seat=fixed_opponent_seat,
                         opponent_seed=opponent_seed)

    def self_play_game(self, seed: int) -> tuple[list[tuple[np.ndarray, np.ndarray, float]], dict]:
        env = self.library.create(self.config.width, self.config.height, self.config.max_steps,
                                  self.config.crate_density, seed)
        trajectory: list[tuple[np.ndarray, np.ndarray, int]] = []
        done = False
        try:
            while not done:
                visits = self._mcts().search(env, add_root_noise=True)
                total = visits.sum()
                policy_zero = visits.sum(axis=1) / total
                policy_one = visits.sum(axis=0) / total
                trajectory.append((env.encode(0), policy_zero.astype(np.float32), 0))
                trajectory.append((env.encode(1), policy_one.astype(np.float32), 1))
                temperature = (self.config.temperature if
                               env.step_count < self.config.temperature_steps else 0.0)
                action_zero, action_one = sample_joint(visits, temperature, self.rng)
                done = env.step_joint(action_zero, action_one)
            outcome_zero = env.outcome(0)
            value_zero = self._terminal_training_value(env, 0)
            samples = [(state, policy, float(value_zero if perspective == 0 else -value_zero))
                       for state, policy, perspective in trajectory]
            return samples, {"outcome": outcome_zero, "steps": env.step_count,
                             "samples": len(samples)}
        finally:
            env.close()

    @staticmethod
    def _terminal_training_value(env: BomberEnv, perspective: int) -> float:
        outcome = env.outcome(perspective)
        if outcome != 0:
            return float(outcome)
        opponent = 1 - perspective
        advantage = env.tactical_value(perspective) - env.tactical_value(opponent)
        return float(np.tanh(advantage / 250.0))

    def teacher_game(self, seed: int) -> tuple[list[tuple[np.ndarray, np.ndarray, float]], dict]:
        env = self.library.create(self.config.width, self.config.height, self.config.max_steps,
                                  self.config.crate_density, seed)
        agent_type = NATIVE_AGENT_TYPES[self.config.teacher_type]
        expert_seat = seed & 1
        agents = [self.library.create_agent(0, seed * 2 + seat) for seat in (0, 1)]
        agents[expert_seat].close()
        agents[expert_seat] = self.library.create_agent(agent_type,
                                                        seed * 2 + expert_seat)
        trajectory: list[tuple[np.ndarray, np.ndarray]] = []
        done = False
        try:
            while not done:
                actions = [agents[seat].action(env, seat) for seat in (0, 1)]
                policy = np.zeros(ACTIONS, np.float32)
                policy[actions[expert_seat]] = 1.0
                trajectory.append((env.encode(expert_seat), policy))
                done = env.step_joint(actions[0], actions[1])
            outcome = env.outcome(expert_seat)
            value = self._terminal_training_value(env, expert_seat)
            samples = [(state, policy, value) for state, policy in trajectory]
            return samples, {"outcome": outcome, "steps": env.step_count,
                             "samples": len(samples)}
        finally:
            for agent in agents:
                agent.close()
            env.close()

    def adversary_game(self, seed: int) -> tuple[list[tuple[np.ndarray, np.ndarray, float]], dict]:
        env = self.library.create(self.config.width, self.config.height, self.config.max_steps,
                                  self.config.crate_density, seed)
        learner_seat = seed & 1
        opponent_type = NATIVE_AGENT_TYPES[self.config.adversary_type]
        opponent = self.library.create_agent(opponent_type, seed * 2)
        trajectory: list[tuple[np.ndarray, np.ndarray]] = []
        done = False
        try:
            while not done:
                visits = self._mcts(
                    fixed_opponent_type=(opponent_type
                                         if self.config.adversary_type == "heuristic" else None),
                    fixed_opponent_seat=(1 - learner_seat
                                         if self.config.adversary_type == "heuristic" else None),
                    opponent_seed=seed).search(env, add_root_noise=True)
                marginal = visits.sum(axis=1 if learner_seat == 0 else 0)
                policy = (marginal / marginal.sum()).astype(np.float32)
                trajectory.append((env.encode(learner_seat), policy))
                learner_action = int(np.argmax(marginal))
                opponent_action = opponent.action(env, 1 - learner_seat)
                actions = ([learner_action, opponent_action] if learner_seat == 0 else
                           [opponent_action, learner_action])
                done = env.step_joint(actions[0], actions[1])
            outcome = env.outcome(learner_seat)
            value = self._terminal_training_value(env, learner_seat)
            samples = [(state, policy, value) for state, policy in trajectory]
            return samples, {"outcome": outcome, "value": value,
                             "steps": env.step_count, "samples": len(samples)}
        finally:
            opponent.close()
            env.close()

    def collect_adversary(self, iteration: int) -> dict:
        progress = ProgressBar("adversary", self.config.adversary_games, "games",
                               self.config.progress)
        outcomes = {-1: 0, 0: 0, 1: 0}
        samples = steps = 0
        values: list[float] = []
        started = time.monotonic()
        for game in range(self.config.adversary_games):
            pool_index = ((iteration - 1) * self.config.adversary_games + game) % \
                         self.config.adversary_seed_pool_size
            seed = 50_001 + pool_index
            game_samples, stats = self.adversary_game(seed)
            repeats = self.config.adversary_win_repeats if stats["outcome"] > 0 else 1
            for _ in range(repeats):
                self.replay.extend(game_samples)
            outcomes[stats["outcome"]] += 1
            values.append(stats["value"])
            samples += stats["samples"]
            steps += stats["steps"]
            self.global_steps += stats["steps"]
            progress.update(game + 1)
            if self.stop_requested:
                break
        completed = sum(outcomes.values())
        if completed < self.config.adversary_games:
            progress.update(completed, force=True)
        elapsed = max(time.monotonic() - started, 1e-9)
        return {"opponent": self.config.adversary_type, "games": completed, "samples": samples,
                "steps": steps, "wins": outcomes[1], "losses": outcomes[-1],
                "draws": outcomes[0], "mean_shaped_value": float(np.mean(values)) if values else 0.0,
                "win_replay_repeats": self.config.adversary_win_repeats,
                "seed_pool_size": self.config.adversary_seed_pool_size,
                "steps_per_second": steps / elapsed}

    def collect_teacher(self) -> dict:
        progress = ProgressBar("teacher", self.config.teacher_games, "games",
                               self.config.progress)
        outcomes = {-1: 0, 0: 0, 1: 0}
        samples = steps = 0
        started = time.monotonic()
        for game in range(self.config.teacher_games):
            seed = int(self.rng.integers(1, np.iinfo(np.int32).max))
            game_samples, stats = self.teacher_game(seed)
            self.replay.extend(game_samples)
            outcomes[stats["outcome"]] += 1
            samples += stats["samples"]
            steps += stats["steps"]
            self.global_steps += stats["steps"]
            progress.update(game + 1)
            if self.stop_requested:
                break
        completed = sum(outcomes.values())
        if completed < self.config.teacher_games:
            progress.update(completed, force=True)
        elapsed = max(time.monotonic() - started, 1e-9)
        return {"type": self.config.teacher_type, "games": completed,
                "samples": samples, "steps": steps,
                "wins_teacher": outcomes[1], "losses_teacher": outcomes[-1],
                "draws": outcomes[0], "steps_per_second": steps / elapsed}

    def collect_self_play(self, iteration: int) -> dict:
        progress = ProgressBar("self-play", self.config.self_play_games, "games",
                               self.config.progress)
        outcomes = {-1: 0, 0: 0, 1: 0}
        samples = steps = 0
        started = time.monotonic()
        for game in range(self.config.self_play_games):
            seed = int(self.rng.integers(1, np.iinfo(np.int32).max))
            game_samples, stats = self.self_play_game(seed)
            self.replay.extend(game_samples)
            outcomes[stats["outcome"]] += 1
            samples += stats["samples"]
            steps += stats["steps"]
            self.global_steps += stats["steps"]
            progress.update(game + 1)
            if self.stop_requested:
                break
        if sum(outcomes.values()) < self.config.self_play_games:
            progress.update(sum(outcomes.values()), force=True)
        elapsed = max(time.monotonic() - started, 1e-9)
        return {"games": sum(outcomes.values()), "samples": samples, "steps": steps,
                "wins_player0": outcomes[1], "wins_player1": outcomes[-1],
                "draws": outcomes[0], "steps_per_second": steps / elapsed}

    def optimize(self) -> dict:
        if len(self.replay) == 0 or self.config.train_steps == 0:
            return {"loss": 0.0, "policy_loss": 0.0, "value_loss": 0.0, "entropy": 0.0}
        progress = ProgressBar("optimize", self.config.train_steps, "batches",
                               self.config.progress)
        totals = {"loss": 0.0, "policy_loss": 0.0, "value_loss": 0.0, "entropy": 0.0}
        completed = 0
        learning_rate = self.config.learning_rate * (
            self.config.learning_rate_decay ** self.model.scheduler_step)
        for step in range(self.config.train_steps):
            batch = self.replay.sample(self.config.batch_size, self.rng)
            metrics = self.model.train_batch(*batch, learning_rate=learning_rate)
            for key in totals:
                totals[key] += metrics[key]
            completed += 1
            progress.update(completed)
            if self.stop_requested:
                break
        if completed < self.config.train_steps:
            progress.update(completed, force=True)
        self.model.scheduler_step += 1
        result = {key: value / max(completed, 1) for key, value in totals.items()}
        result["learning_rate"] = learning_rate
        return result

    def _policy_action(self, policy: PolicyValueNet | str | None, env: BomberEnv,
                       perspective: int, rng: np.random.Generator, baseline=None,
                       opponent_policy: PolicyValueNet | str | None = None) -> int:
        legal = env.safe_mask(perspective)
        if policy is None:
            return int(rng.choice(np.flatnonzero(legal)))
        if isinstance(policy, str):
            return baseline.action(env, perspective)
        search = JointMCTS(policy, self.config.evaluation_simulations,
                           self.config.c_puct, self.config.dirichlet_alpha, 0.0, rng,
                           fixed_opponent_type=(NATIVE_AGENT_TYPES[opponent_policy]
                                                if isinstance(opponent_policy, str) else None),
                           fixed_opponent_seat=(1 - perspective
                                                if isinstance(opponent_policy, str) else None))
        visits = search.search(env, add_root_noise=False)
        marginal = visits.sum(axis=1 if perspective == 0 else 0)
        return int(np.argmax(marginal))

    def _load_best_model(self) -> PolicyValueNet | None:
        if not self.best_path.exists():
            return None
        model = PolicyValueNet(self.model.input_size, self.model.hidden_size,
                               conv_channels=self.model.conv_channels)
        replay = ReplayBuffer(1)
        scratch_rng = np.random.default_rng(0)
        load_checkpoint(self.best_path, model, replay, asdict(self.config), scratch_rng)
        return model

    def _evaluate_opponent(self, opponent: PolicyValueNet | str | None, seeds: list[int],
                           progress: ProgressBar, completed: int) -> tuple[dict, int]:
        wins = losses = draws = total_steps = 0
        for seed in seeds:
            for current_seat in (0, 1):
                match_rng = np.random.default_rng(seed * 2 + current_seat)
                baseline = (self.library.create_agent(NATIVE_AGENT_TYPES[opponent],
                                                      seed * 2 + (1 - current_seat))
                            if isinstance(opponent, str) else None)
                env = self.library.create(self.config.width, self.config.height,
                                          self.config.max_steps, self.config.crate_density, seed)
                done = False
                try:
                    while not done:
                        models = ((self.model, opponent) if current_seat == 0 else
                                  (opponent, self.model))
                        actions = [self._policy_action(models[seat], env, seat, match_rng,
                                                       baseline, models[1 - seat])
                                   for seat in (0, 1)]
                        done = env.step_joint(actions[0], actions[1])
                    result = env.outcome(current_seat)
                    wins += result > 0
                    losses += result < 0
                    draws += result == 0
                    total_steps += env.step_count
                finally:
                    env.close()
                    if baseline is not None:
                        baseline.close()
                completed += 1
                progress.update(completed)
        games = wins + losses + draws
        return {"games": games, "wins": wins, "losses": losses, "draws": draws,
                "score": (wins + 0.5 * draws) / max(games, 1),
                "mean_steps": total_steps / max(games, 1)}, completed

    def evaluate(self, include_mcts: bool = False) -> dict:
        seeds = [90_001 + index for index in range(self.config.evaluation_games)]
        best_model = self._load_best_model()
        total = len(seeds) * 4
        if best_model is not None:
            total += len(seeds) * 2
        mcts_seeds = seeds[:self.config.mcts_evaluation_games] if include_mcts else []
        total += len(mcts_seeds) * 2
        progress = ProgressBar("evaluate", total, "games",
                               self.config.progress)
        random_result, completed = self._evaluate_opponent(None, seeds, progress, 0)
        heuristic_result, completed = self._evaluate_opponent(
            "heuristic", seeds, progress, completed)
        result = {"random": random_result, "heuristic": heuristic_result}
        if best_model is not None:
            result["previous_best"], completed = self._evaluate_opponent(
                best_model, seeds, progress, completed)
        if mcts_seeds:
            result["mcts"], completed = self._evaluate_opponent(
                "mcts", mcts_seeds, progress, completed)
        if completed < progress.total:
            progress.update(completed, force=True)
        return result

    @staticmethod
    def _should_promote(evaluation: dict, best_exists: bool, best_score: float,
                        promotion_margin: float) -> bool:
        if not evaluation:
            return False
        candidate_score = evaluation.get("heuristic", {}).get("score", float("-inf"))
        random_score = evaluation.get("random", {}).get("score", float("-inf"))
        if random_score < 0.9:
            return False
        if not best_exists:
            return candidate_score >= 0.5
        incumbent_score = evaluation.get("previous_best", {}).get("score")
        return (incumbent_score is not None and
                incumbent_score > 0.5 + promotion_margin and
                candidate_score >= best_score - promotion_margin)

    def _append_metrics(self, metrics: dict) -> None:
        with self.metrics_path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(metrics, sort_keys=True) + "\n")
            handle.flush()
            os.fsync(handle.fileno())

    def checkpoint(self, emergency: bool = False) -> Path:
        target = self.run_dir / "emergency.npz" if emergency else self.latest_path
        save_checkpoint(target, self.model, self.replay, asdict(self.config), self.iteration,
                        self.global_steps, self.rng, self.best_score, self.last_evaluation)
        return target

    def run(self) -> None:
        previous_handler = signal.getsignal(signal.SIGINT)

        def request_stop(_signum, _frame):
            if self.stop_requested:
                raise KeyboardInterrupt
            self.stop_requested = True
            print("\nInterrupt received; finishing the current safe unit before emergency checkpoint...")

        signal.signal(signal.SIGINT, request_stop)
        run_iterations = max(self.config.iterations - self.iteration, 0)
        iteration_progress = ProgressBar("iterations", run_iterations, "iters",
                                         self.config.progress)
        completed_this_run = 0
        try:
            while self.iteration < self.config.iterations and not self.stop_requested:
                next_iteration = self.iteration + 1
                started = time.monotonic()
                teacher = {}
                if (self.config.teacher_games > 0 and
                        next_iteration <= self.config.teacher_iterations):
                    teacher = self.collect_teacher()
                self_play = self.collect_self_play(next_iteration)
                adversary = {}
                if (self.config.adversary_games > 0 and
                        next_iteration >= self.config.adversary_start_iteration):
                    adversary = self.collect_adversary(next_iteration)
                optimization = self.optimize()
                evaluation = {}
                if (next_iteration % self.config.evaluation_interval == 0 and
                        not self.stop_requested):
                    evaluation = self.evaluate(
                        include_mcts=next_iteration % self.config.mcts_evaluation_interval == 0)
                    self.last_evaluation = evaluation
                self.iteration = next_iteration
                candidate_score = evaluation.get("heuristic", {}).get("score", float("-inf"))
                promoted = self._should_promote(
                    evaluation, self.best_path.exists(), self.best_score,
                    self.config.promotion_margin)
                if promoted:
                    # Keep the historical validation maximum as a monotonic
                    # anti-regression bar even when a head-to-head winner is
                    # within the configured heuristic-score tolerance.
                    self.best_score = max(self.best_score, candidate_score)
                elapsed = time.monotonic() - started
                metrics = {"schema_version": 1, "iteration": self.iteration,
                           "global_steps": self.global_steps, "replay_size": len(self.replay),
                           "elapsed_seconds": elapsed, "self_play": self_play,
                           "teacher": teacher,
                           "adversary": adversary,
                           "optimization": optimization, "evaluation": evaluation,
                           "promoted": promoted, "best_score": self.best_score}
                self._append_metrics(metrics)
                self.checkpoint()
                if self.iteration % self.config.snapshot_interval == 0:
                    copy_checkpoint(self.latest_path,
                                    self.run_dir / f"iteration_{self.iteration:06d}.npz")
                if promoted:
                    copy_checkpoint(self.latest_path, self.best_path)
                print(json.dumps(metrics, indent=2))
                completed_this_run += 1
                iteration_progress.update(completed_this_run)
            if self.stop_requested:
                path = self.checkpoint(emergency=True)
                print(f"Emergency checkpoint saved to {path}")
        except KeyboardInterrupt:
            path = self.checkpoint(emergency=True)
            print(f"Forced interrupt checkpoint saved to {path}")
        finally:
            signal.signal(signal.SIGINT, previous_handler)
