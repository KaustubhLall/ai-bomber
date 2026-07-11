"""KL-108 Brick 2: paired descriptive comparison between two per-match-row JSONL exports,
matched by evaluation seed and seat. Pairing removes seed-to-seed evaluation variance, but it
does not make independently trained checkpoints a causal treatment/control pair. Causal use
also requires the same starting checkpoint, RNG/replay, binary, absolute LR schedule, training
horizon, and every non-treatment semantic. The historical crush01/control03 artifacts fail
that bar because their LR horizons differ; they remain useful absolute negative evidence only.

Usage:
    python tools/analyze_paired_gate_eval.py <run-a.jsonl> <run-b.jsonl> [--label-a X] [--label-b Y]
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


def load_rows(path: Path) -> dict[int, list[dict]]:
    by_seed: dict[int, list[dict]] = defaultdict(list)
    seen: set[tuple[int, int]] = set()
    required = {"seed", "learner_seat", "outcome", "cause", "learner_wait_fraction"}
    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        if not line.strip():
            continue
        row = json.loads(line)
        missing = required - row.keys()
        if missing:
            raise ValueError(f"{path}:{line_number} missing required fields {sorted(missing)}")
        key = (row["seed"], row["learner_seat"])
        if key in seen:
            raise ValueError(f"{path}:{line_number} duplicates seed/seat {key}")
        if row["learner_seat"] not in (0, 1):
            raise ValueError(f"{path}:{line_number} has invalid learner_seat")
        seen.add(key)
        by_seed[row["seed"]].append(row)
    for seed, rows in by_seed.items():
        seats = {row["learner_seat"] for row in rows}
        if seats != {0, 1}:
            raise ValueError(f"{path} seed {seed} is partial; expected seats 0 and 1, got {seats}")
    return by_seed


def summarize(rows: list[dict], label: str) -> None:
    n = len(rows)
    wins = sum(1 for r in rows if r["outcome"] == "win")
    draws = sum(1 for r in rows if r["outcome"] == "draw")
    losses = sum(1 for r in rows if r["outcome"] == "loss")
    bomb_wins = sum(1 for r in rows if r["outcome"] == "win" and r["cause"] == "bomb")
    crush_wins = sum(1 for r in rows if r["outcome"] == "win" and r["cause"] == "arena_crush")
    mean_wait = sum(r["learner_wait_fraction"] for r in rows) / n if n else 0.0
    print(f"  {label}: N={n} {wins}W-{draws}D-{losses}L "
          f"(bomb-win={bomb_wins} [{100*bomb_wins/n:.1f}% of games], "
          f"crush-win={crush_wins}, mean_wait={100*mean_wait:.1f}%)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("treatment", type=Path)
    parser.add_argument("control", type=Path)
    parser.add_argument("--label-a", default="run-a")
    parser.add_argument("--label-b", default="run-b")
    args = parser.parse_args()

    treatment_by_seed = load_rows(args.treatment)
    control_by_seed = load_rows(args.control)

    treatment_rows = [row for rows in treatment_by_seed.values() for row in rows]
    control_rows = [row for rows in control_by_seed.values() for row in rows]

    print(f"=== Unpaired summaries ({args.treatment.name} vs {args.control.name}) ===")
    summarize(treatment_rows, args.label_a)
    summarize(control_rows, args.label_b)

    treatment_seeds = set(treatment_by_seed)
    control_seeds = set(control_by_seed)
    if treatment_seeds != control_seeds:
        raise ValueError(
            "paired evidence must contain identical complete seed blocks; "
            f"treatment-only={sorted(treatment_seeds-control_seeds)}, "
            f"control-only={sorted(control_seeds-treatment_seeds)}"
        )
    common_seeds = sorted(treatment_seeds)
    if not common_seeds:
        raise ValueError("paired evidence contains no matches")

    print(f"\n=== Paired comparison on {len(common_seeds)} common seeds "
          f"({len(treatment_rows)} vs {len(control_rows)} total games - unpaired if these "
          f"counts differ from len(common_seeds)*2, only paired seeds are compared below) ===")

    bomb_win_delta = 0
    both_bomb_win = 0
    only_treatment_bomb_win = 0
    only_control_bomb_win = 0
    neither_bomb_win = 0
    seat_deltas = {0: 0, 1: 0}

    for seed in common_seeds:
        t_rows = {r["learner_seat"]: r for r in treatment_by_seed[seed]}
        c_rows = {r["learner_seat"]: r for r in control_by_seed[seed]}
        for seat in (0, 1):
            t_bomb = t_rows[seat]["outcome"] == "win" and t_rows[seat]["cause"] == "bomb"
            c_bomb = c_rows[seat]["outcome"] == "win" and c_rows[seat]["cause"] == "bomb"
            if t_bomb and c_bomb:
                both_bomb_win += 1
            elif t_bomb and not c_bomb:
                only_treatment_bomb_win += 1
                seat_deltas[seat] += 1
            elif c_bomb and not t_bomb:
                only_control_bomb_win += 1
                seat_deltas[seat] -= 1
            else:
                neither_bomb_win += 1

    paired_n = both_bomb_win + only_treatment_bomb_win + only_control_bomb_win + neither_bomb_win
    print(f"  Bomb-win McNemar-style breakdown (N={paired_n} paired seat-matches):")
    print(f"    both bomb-win:              {both_bomb_win}")
    print(f"    only {args.label_a} bomb-win:  {only_treatment_bomb_win}")
    print(f"    only {args.label_b} bomb-win:    {only_control_bomb_win}")
    print(f"    neither bomb-win:           {neither_bomb_win}")
    net = only_treatment_bomb_win - only_control_bomb_win
    print(f"    net paired bomb-win delta ({args.label_a} - {args.label_b}): {net:+d} "
          f"({100*net/paired_n:+.1f} percentage points of paired matches)")
    print(f"    seat-0 delta: {seat_deltas[0]:+d}, seat-1 delta: {seat_deltas[1]:+d} "
          f"(a large imbalance here would suggest a seat-specific artifact, not a real effect)")
    print("\nThis is directional evidence, not a hypothesis-test p-value - read it alongside "
          "the unpaired summaries above and the WAIT/mean_wait trend, not in isolation.")


if __name__ == "__main__":
    main()
