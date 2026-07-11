"""KL-107: human-readable summary of a --trace-output neural decision trace.

For each traced game (one seed + learner seat), prints a step-by-step or summary view of:
raw network policy (top actions + probabilities + entropy), MCTS-refined policy, whether the
final chosen action agreed with the raw policy's own preference or diverged from it (search
overriding the network vs. agreeing with it), and the search-derived value trajectory. This is
the tool for answering "did this tactical error come from the policy, the value head, or the
search" - the actual KL-107 ask - once a real trace file exists from a checkpoint worth
inspecting.

Usage:
    python tools/analyze_neural_trace.py trace.jsonl                  # summary of every game
    python tools/analyze_neural_trace.py trace.jsonl --seed 1300001 --seat 0 --steps
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

ACTION_NAMES = ["UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"]


def top_actions(policy: list[float], n: int = 2) -> str:
    ranked = sorted(range(len(policy)), key=lambda i: -policy[i])[:n]
    return ", ".join(f"{ACTION_NAMES[i]}={policy[i]:.2f}" for i in ranked)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--seed", type=int, default=None, help="restrict to one seed")
    parser.add_argument("--seat", type=int, default=None, help="restrict to one learner seat")
    parser.add_argument("--steps", action="store_true", help="print every step, not just a summary")
    args = parser.parse_args()

    lines = args.trace.read_text().splitlines()
    header = json.loads(lines[0])
    print(f"Trace: checkpoint={header['checkpoint_sha256'][:12]}... "
          f"executable={header['executable_sha256'][:12]}... "
          f"git={header['git_commit']} opponent={header['opponent_type']}\n")

    games: dict[tuple[int, int], list[dict]] = defaultdict(list)
    for line in lines[1:]:
        if not line.strip():
            continue
        row = json.loads(line)
        games[(row["seed"], row["learner_seat"])].append(row)

    for (seed, seat), steps in sorted(games.items()):
        if args.seed is not None and seed != args.seed:
            continue
        if args.seat is not None and seat != args.seat:
            continue
        steps.sort(key=lambda r: r["step"])

        n = len(steps)
        mean_entropy = sum(r["raw_policy_entropy"] for r in steps) / n if n else 0.0
        agree = sum(1 for r in steps
                    if r["chosen_action"] == max(range(6), key=lambda i: r["raw_policy"][i]))
        wait_count = sum(1 for r in steps if r["chosen_action"] == 5)
        value_trajectory = [round(r["search_value_estimate"], 3) for r in steps]

        print(f"seed={seed} seat={seat}: {n} steps, mean_raw_policy_entropy={mean_entropy:.3f}, "
              f"chosen==argmax(raw_policy) in {agree}/{n} steps "
              f"({100*agree/n:.0f}% - low means search is overriding the raw policy a lot), "
              f"WAIT chosen {wait_count}/{n} times ({100*wait_count/n:.0f}%)")
        print(f"  value trajectory (first 5 -> last 5): {value_trajectory[:5]} ... "
              f"{value_trajectory[-5:]}")

        if args.steps:
            for row in steps:
                argmax_raw = max(range(6), key=lambda i: row["raw_policy"][i])
                marker = "" if row["chosen_action"] == argmax_raw else "  <- search overrode raw policy"
                print(f"    step={row['step']:>3} chosen={ACTION_NAMES[row['chosen_action']]:>5} "
                      f"raw=[{top_actions(row['raw_policy'])}] "
                      f"mcts=[{top_actions(row['mcts_policy'])}] "
                      f"value={row['search_value_estimate']:+.3f} "
                      f"root_visits={row['root_visits']}{marker}")
        print()


if __name__ == "__main__":
    main()
