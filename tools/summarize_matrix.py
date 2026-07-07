"""Print role-balanced head-to-head records from a benchmark matrix JSON file."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("matrix")
args = parser.parse_args()
rows = json.loads(Path(args.matrix).read_text())["results"]

policies = sorted({row["agent"] for row in rows})
lookup = {(row["agent"], row["opponent"]): row for row in rows}
print(f"{'matchup':27} {'left wins':>10} {'right wins':>11} {'draws':>7}")
for i, left in enumerate(policies):
    for right in policies[i + 1:]:
        left_blue = lookup[left, right]
        right_blue = lookup[right, left]
        episodes = int(left_blue["episodes"])
        left_wins = round(left_blue["win_rate"] * episodes + right_blue["death_rate"] * episodes)
        right_wins = round(right_blue["win_rate"] * episodes + left_blue["death_rate"] * episodes)
        draws = 2 * episodes - left_wins - right_wins
        print(f"{left + ' vs ' + right:27} {left_wins:10d} {right_wins:11d} {draws:7d}")
