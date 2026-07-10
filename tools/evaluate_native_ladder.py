"""Role-balanced native baseline-vs-baseline strength calibration."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.alphazero.env import TrainingLibrary
from tools.alphazero.progress import ProgressBar


AGENTS = {
    "random": 0, "scripted": 1, "heuristic": 2, "greedy": 3,
    "alpha-beta": 6, "mcts": 7, "evasive": 8,
}


def wilson_lower_bound(score: float, games: int,
                       z: float = 1.6448536269514722) -> float:
    if games <= 0:
        return 0.0
    z2 = z * z
    denominator = 1 + z2 / games
    center = score + z2 / (2 * games)
    spread = z * math.sqrt((score * (1 - score) + z2 / (4 * games)) / games)
    return max(0.0, min(1.0, (center - spread) / denominator))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library")
    parser.add_argument("--agent-a", choices=AGENTS, required=True)
    parser.add_argument("--agent-b", choices=AGENTS, required=True)
    parser.add_argument("--seed-base", type=int, required=True)
    parser.add_argument("--seeds", type=int, default=8)
    parser.add_argument("--width", type=int, default=13)
    parser.add_argument("--height", type=int, default=11)
    parser.add_argument("--max-steps", type=int, default=200)
    parser.add_argument("--crate-density", type=int, default=50)
    parser.add_argument("--agent-a-mcts-simulations", type=int, default=96)
    parser.add_argument("--agent-a-mcts-depth", type=int, default=12)
    parser.add_argument("--agent-b-mcts-simulations", type=int, default=96)
    parser.add_argument("--agent-b-mcts-depth", type=int, default=12)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--no-progress", action="store_true")
    args = parser.parse_args()
    library = TrainingLibrary(args.library)
    wins = draws = losses = total_steps = 0
    by_seat = [{"seat": seat, "wins": 0, "draws": 0, "losses": 0}
               for seat in (0, 1)]
    progress = ProgressBar("native-ladder", args.seeds * 2, "games",
                           not args.no_progress)
    completed = 0
    for seed in range(args.seed_base, args.seed_base + args.seeds):
        for agent_a_seat in (0, 1):
            env = library.create(args.width, args.height, args.max_steps,
                                 args.crate_density, seed)
            agents = [None, None]
            try:
                agents[agent_a_seat] = library.create_agent(
                    AGENTS[args.agent_a], seed * 2 + agent_a_seat)
                agents[1 - agent_a_seat] = library.create_agent(
                    AGENTS[args.agent_b], seed * 2 + 1 - agent_a_seat)
                if args.agent_a == "mcts":
                    agents[agent_a_seat].configure_mcts(
                        args.agent_a_mcts_simulations, args.agent_a_mcts_depth)
                if args.agent_b == "mcts":
                    agents[1 - agent_a_seat].configure_mcts(
                        args.agent_b_mcts_simulations, args.agent_b_mcts_depth)
                done = False
                while not done:
                    actions = [agents[seat].action(env, seat) for seat in (0, 1)]
                    done = env.step_joint(actions[0], actions[1])
                outcome = env.outcome(agent_a_seat)
                wins += outcome > 0
                losses += outcome < 0
                draws += outcome == 0
                by_seat[agent_a_seat]["wins"] += outcome > 0
                by_seat[agent_a_seat]["losses"] += outcome < 0
                by_seat[agent_a_seat]["draws"] += outcome == 0
                total_steps += env.step_count
                completed += 1
                progress.update(completed)
            finally:
                for agent in agents:
                    if agent is not None:
                        agent.close()
                env.close()
    games = wins + draws + losses
    score = (wins + 0.5 * draws) / games
    result = {
        "schema_version": 1,
        "agent_a": args.agent_a,
        "agent_b": args.agent_b,
        "seed_base": args.seed_base,
        "seeds": args.seeds,
        "games": games,
        "wins": wins,
        "draws": draws,
        "losses": losses,
        "score": score,
        "lower_confidence_bound": wilson_lower_bound(score, games),
        "mean_steps": total_steps / games,
        "by_agent_a_seat": by_seat,
        "mcts": {
            "agent_a_simulations": args.agent_a_mcts_simulations,
            "agent_a_depth": args.agent_a_mcts_depth,
            "agent_b_simulations": args.agent_b_mcts_simulations,
            "agent_b_depth": args.agent_b_mcts_depth,
        },
    }
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        temporary = args.output.with_name(args.output.name + ".tmp")
        temporary.parent.mkdir(parents=True, exist_ok=True)
        temporary.write_text(rendered, encoding="utf-8")
        temporary.replace(args.output)
    print(rendered, end="")


if __name__ == "__main__":
    main()
