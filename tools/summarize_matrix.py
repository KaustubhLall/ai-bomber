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
print(f"{'matchup':27} {'left W':>7} {'right W':>8} {'draw':>6} {'left own':>9} {'right own':>10} {'left self':>10} {'right self':>11}")
for i, left in enumerate(policies):
    for right in policies[i + 1:]:
        left_blue = lookup[left, right]
        right_blue = lookup[right, left]
        episodes = int(left_blue["episodes"])
        left_wins = round(left_blue["win_rate"] * episodes + right_blue["death_rate"] * episodes)
        right_wins = round(right_blue["win_rate"] * episodes + left_blue["death_rate"] * episodes)
        draws = 2 * episodes - left_wins - right_wins
        left_owned = int(left_blue.get("owned_eliminations", 0)) + int(right_blue.get("opponent_kills", 0))
        right_owned = int(right_blue.get("owned_eliminations", 0)) + int(left_blue.get("opponent_kills", 0))
        left_self = int(left_blue.get("self_kills", 0)) + int(right_blue.get("opponent_self_kills", 0))
        right_self = int(right_blue.get("self_kills", 0)) + int(left_blue.get("opponent_self_kills", 0))
        print(f"{left + ' vs ' + right:27} {left_wins:7d} {right_wins:8d} {draws:6d} {left_owned:9d} {right_owned:10d} {left_self:10d} {right_self:11d}")
