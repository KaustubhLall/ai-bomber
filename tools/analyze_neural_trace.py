"""KL-107: human-readable summary of a --trace-output neural decision trace.

For each traced game, prints the safety-masked root prior, MCTS-refined policy, agreement, and
search backup trajectory. Important boundary: v1 called the masked prior `raw_policy`, but it
was already filtered by the safe-action mask; v1/v2 contain no unmasked policy head or raw value
head at all. v3 adds a genuinely recomputed pre-mask policy/value (policy_head_raw_recomputed,
value_head_raw_recomputed), the safe_action_mask used to derive the masked prior, a wait_forced
flag (idling was the position's only safe action, not a preference), and root Q per action
(search_root_q_values). v4 adds opponent_modeled_as (what the search's internal lookahead
assumed for the opposing seat - "self" or a fixed baseline agent name; see docs/NATIVE_ALPHAZERO.md
for why this can differ from the actual match opponent) and learner_moved (false for a movement
action blocked by terrain/a bomb/the opponent, or a lost simultaneous-move collision - "effective
idle" beyond explicit WAIT). This tool reports all of these when present, and falls back to the
v2 masked-prior-only view on older files.

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
        def prior(row: dict) -> list[float]:
            return row.get("policy_prior_after_safety_mask", row.get("raw_policy"))

        def prior_entropy(row: dict) -> float:
            return row.get("policy_prior_entropy", row.get("raw_policy_entropy"))

        mean_entropy = sum(prior_entropy(r) for r in steps) / n if n else 0.0
        agree = sum(1 for r in steps
                    if r["chosen_action"] == max(range(6), key=lambda i: prior(r)[i]))
        wait_count = sum(1 for r in steps if r["chosen_action"] == 5)
        value_trajectory = [round(r["search_value_estimate"], 3) for r in steps]

        print(f"seed={seed} seat={seat}: {n} steps, mean_masked_prior_entropy={mean_entropy:.3f}, "
              f"chosen==argmax(masked_prior) in {agree}/{n} steps "
              f"({100*agree/n:.0f}% - low means search changes the masked prior often), "
              f"WAIT chosen {wait_count}/{n} times ({100*wait_count/n:.0f}%)")
        print(f"  value trajectory (first 5 -> last 5): {value_trajectory[:5]} ... "
              f"{value_trajectory[-5:]}")

        has_raw = all("policy_head_raw_recomputed" in r for r in steps) if steps else False
        if has_raw:
            raw_agree = sum(1 for r in steps if r["chosen_action"] ==
                             max(range(6), key=lambda i: r["policy_head_raw_recomputed"][i]))
            mask_changed_top = sum(
                1 for r in steps
                if max(range(6), key=lambda i: r["policy_head_raw_recomputed"][i])
                != max(range(6), key=lambda i: prior(r)[i]))
            forced = sum(1 for r in steps if r["wait_forced"])
            wait_not_forced = sum(1 for r in steps if r["chosen_action"] == 5 and not r["wait_forced"])
            print(f"  [v3] chosen==argmax(RAW pre-mask policy) in {raw_agree}/{n} steps "
                  f"({100*raw_agree/n:.0f}%), mask changed the top action in "
                  f"{mask_changed_top}/{n} steps ({100*mask_changed_top/n:.0f}%)")
            print(f"  [v3] WAIT forced (only safe action) {forced}/{n} steps "
                  f"({100*forced/n:.0f}%); WAIT chosen with alternatives available "
                  f"{wait_not_forced}/{n} steps ({100*wait_not_forced/n:.0f}%) - "
                  f"the latter is the real passivity signal, the former is not a policy choice")

        has_v4 = all("learner_moved" in r for r in steps) if steps else False
        if has_v4:
            movement_actions = (0, 1, 2, 3)
            blocked = sum(1 for r in steps
                          if r["chosen_action"] in movement_actions and not r["learner_moved"])
            combined_idle = sum(1 for r in steps if r["chosen_action"] == 5 or
                                (r["chosen_action"] in movement_actions and not r["learner_moved"]))
            modeled_as = {r["opponent_modeled_as"] for r in steps}
            print(f"  [v4] blocked movement (chose a direction, didn't move) {blocked}/{n} steps "
                  f"({100*blocked/n:.0f}%); combined idle (WAIT + blocked) {combined_idle}/{n} "
                  f"({100*combined_idle/n:.0f}%); opponent modeled as: {', '.join(sorted(modeled_as))}")

        if args.steps:
            for row in steps:
                masked_prior = prior(row)
                argmax_prior = max(range(6), key=lambda i: masked_prior[i])
                marker = "" if row["chosen_action"] == argmax_prior else "  <- search changed masked prior"
                line = (f"    step={row['step']:>3} chosen={ACTION_NAMES[row['chosen_action']]:>5} "
                        f"masked_prior=[{top_actions(masked_prior)}] "
                        f"mcts=[{top_actions(row['mcts_policy'])}] "
                        f"value={row['search_value_estimate']:+.3f} "
                        f"root_visits={row['root_visits']}{marker}")
                if "policy_head_raw_recomputed" in row:
                    forced_tag = " FORCED" if row["wait_forced"] else ""
                    line += (f"\n        raw=[{top_actions(row['policy_head_raw_recomputed'])}] "
                             f"raw_value={row['value_head_raw_recomputed']:+.3f} "
                             f"safe={row['safe_action_count']}/6{forced_tag}")
                if "learner_moved" in row:
                    blocked_tag = (" BLOCKED" if row["chosen_action"] in (0, 1, 2, 3)
                                   and not row["learner_moved"] else "")
                    line += (f"\n        modeled_opponent={row['opponent_modeled_as']} "
                             f"moved={row['learner_moved']}{blocked_tag}")
                print(line)
        print()


if __name__ == "__main__":
    main()
