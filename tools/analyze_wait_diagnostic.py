"""KL-107 systematic WAIT-diagnostic pass: aggregate forced-vs-chosen WAIT and direct raw-policy-
head statistics across one or more --trace-output files (typically one per checkpoint), with a
Wilson lower confidence bound on each proportion so the finding is load-bearing rather than a
single spot-check's raw percentage.

"Forced" means WAIT was the position's only safe action (safe_action_count==1); this is not a
policy choice and says nothing about passivity. "Chosen" means WAIT was picked with real
alternatives available (safe_action_count>1) - this is the actual passivity signal. A high WAIT%
that turns out to be mostly forced would mean the environment, not the policy, explains it; a
high WAIT% that is mostly chosen means the policy prefers it despite having other options.

The second table reports the network's own raw (pre-mask) policy head directly, rather than only
inferring its behavior from what survives masking and search:
- argmax(raw)==WAIT: how often the raw policy head's own top-ranked action is WAIT - the direct
  measurement of "is passivity the head's own preference", not inferred from downstream agreement.
- chosen==WAIT: the final decision's WAIT rate (unpacked into forced/chosen above too).
- added/removed: search+mask can change the top-ranked action away from what raw preferred.
  "added" = raw's top pick was NOT WAIT but the final chosen action IS WAIT (something made it
  more passive than the raw head alone). "removed" = raw's top pick WAS WAIT but the final chosen
  action is NOT WAIT (something made it less passive). These are directional: chosen==WAIT% =
  argmax(raw)==WAIT% - removed% + added%, which is a useful arithmetic check on this table.
- mean_raw_P(WAIT): the raw policy head's own average WAIT probability, not just how often it
  wins the argmax - a head that assigns WAIT 0.9 probability behaves differently downstream than
  one that assigns it 0.34 even if both have WAIT as the argmax most of the time.
- masked_rows: fraction of steps where at least one action was unsafe (some masking occurred).
- mask_kills_raw_top: fraction of ALL steps (not just masked ones) where the raw policy's own
  favorite action was itself the one the safety mask zeroed out - i.e. safe_action_mask at the
  raw argmax index is 0. This is a precise, boolean check on one specific index, not an argmax
  comparison between two independently-computed distributions (which would also pick up
  recompute-vs-search floating-point noise near-ties as spurious "disagreement").

The third table compares the search's own root Q for WAIT against the best Q among other
VISITED actions (mcts_policy[a] > 0 used as the visited proxy, since search_root_q_values
defaults to 0.0 for actions with zero visits - comparing against that default would conflate
"genuinely evaluated as worthless" with "search never looked at this option"). Restricted to
"comparable" rows where WAIT itself was visited and at least one other action was too.

The fourth table (v4 trace files only, needs learner_moved) reports EFFECTIVE idle, not just
explicit WAIT: a movement action (UP/DOWN/LEFT/RIGHT) that didn't actually change the learner's
board position - blocked by terrain, a bomb, the opponent, or a lost simultaneous-move collision
- is functionally idle even though it isn't a WAIT. This is exactly the gap the prior audit
flagged: "in one heuristic replay the agents repeatedly attempted moves into the same middle
tile, the joint resolver rejected both for about 40 ticks" - invisible to WAIT% alone. Also
reports a per-game idle-streak histogram (longest run of consecutive combined-idle steps within
a game), since a handful of very long stalls can produce the same aggregate idle rate as many
short ones but represents a materially different failure mode.

Usage:
    python tools/analyze_wait_diagnostic.py crush01=crush01-trace.jsonl \
                                             control03=control03-trace.jsonl \
                                             lever2=lever2-trace.jsonl
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

WAIT = 5
BOMB = 4
ACTIONS = 6
MOVEMENT_ACTIONS = (0, 1, 2, 3)  # UP, DOWN, LEFT, RIGHT


def wilson_lower_bound(successes: int, n: int, z: float = 1.96) -> float:
    """One-sided lower confidence bound at confidence level 1 - (1 - Phi(z)); z=1.96 is the
    one-sided 97.5% bound (equivalently, the lower end of a two-sided 95% interval), not a
    one-sided 95% bound (that would be z=1.645). Kept at z=1.96 deliberately - conservative in
    the direction that matters here - just labeled correctly."""
    if n <= 0:
        return 0.0
    p = successes / n
    denominator = 1.0 + z * z / n
    center = p + z * z / (2 * n)
    spread = z * math.sqrt((p * (1 - p) + z * z / (4 * n)) / n)
    return max(0.0, (center - spread) / denominator)


def load_rows(path: Path) -> list[dict]:
    lines = [line for line in path.read_text().splitlines() if line.strip()]
    header = json.loads(lines[0])
    if header.get("trace_format_version", 0) < 3:
        raise ValueError(
            f"{path}: trace_format_version {header.get('trace_format_version')!r} predates "
            f"the raw policy/safe-mask fields (v3) this diagnostic depends on"
        )
    return [json.loads(line) for line in lines[1:]]


def argmax(values: list[float]) -> int:
    return max(range(len(values)), key=lambda i: values[i])


def is_combined_idle(row: dict) -> bool:
    """WAIT, or a movement action that didn't actually change the learner's position (blocked
    by terrain/a bomb/the opponent, or lost a simultaneous-move collision). PLACE_BOMB is
    deliberately NOT idle even though it never moves the agent - placing a bomb is a real
    tactical action, unlike a rejected movement attempt."""
    if row["chosen_action"] == WAIT:
        return True
    return row["chosen_action"] in MOVEMENT_ACTIONS and not row["learner_moved"]


def group_games(rows: list[dict]) -> dict[tuple[int, int], list[dict]]:
    games: dict[tuple[int, int], list[dict]] = {}
    for row in rows:
        games.setdefault((row["seed"], row["learner_seat"]), []).append(row)
    for steps in games.values():
        steps.sort(key=lambda r: r["step"])
    return games


def longest_idle_streak(steps: list[dict]) -> int:
    longest = current = 0
    for row in steps:
        if is_combined_idle(row):
            current += 1
            longest = max(longest, current)
        else:
            current = 0
    return longest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("traces", nargs="+",
                         help="label=path.jsonl (label defaults to the filename if omitted)")
    args = parser.parse_args()
    labels: list[str] = []
    paths: list[Path] = []
    for entry in args.traces:
        label, sep, path_str = entry.partition("=")
        path = Path(path_str if sep else label)
        labels.append(label if sep else path.stem)
        paths.append(path)

    print("=== Forced vs. chosen WAIT ===")
    print(f"{'checkpoint':<16} {'steps':>7} {'WAIT%':>7} "
          f"{'forced':>7} {'forced_lcb':>10}   {'chosen':>7} {'chosen_lcb':>10}")
    rows_by_label: dict[str, list[dict]] = {}
    for label, path in zip(labels, paths):
        rows = load_rows(path)
        rows_by_label[label] = rows
        n = len(rows)
        wait_chosen_total = sum(1 for r in rows if r["chosen_action"] == WAIT)
        forced = sum(1 for r in rows if r["wait_forced"])
        chosen_not_forced = sum(
            1 for r in rows if r["chosen_action"] == WAIT and not r["wait_forced"])

        forced_frac = forced / n if n else 0.0
        chosen_frac = chosen_not_forced / n if n else 0.0
        forced_lcb = wilson_lower_bound(forced, n)
        chosen_lcb = wilson_lower_bound(chosen_not_forced, n)

        print(f"{label:<16} {n:>7} {100*wait_chosen_total/n:>6.1f}% "
              f"{100*forced_frac:>6.1f}% {100*forced_lcb:>9.1f}%   "
              f"{100*chosen_frac:>6.1f}% {100*chosen_lcb:>9.1f}%")

    print("\nforced = WAIT was the only safe action (not a policy choice, environment-forced).")
    print("chosen = WAIT picked with real alternatives available (the actual passivity signal).")
    print("*_lcb  = one-sided 97.5% Wilson lower confidence bound on that fraction across all "
          "traced steps in the file (the lower end of a two-sided 95% interval) - a conservative "
          "floor, not a point estimate, on how much of the behavior is real regardless of "
          "trace-length sampling noise.")

    print("\n=== Raw policy head, directly measured ===")
    print(f"{'checkpoint':<16} {'argmax(raw)==WAIT':>18} {'chosen==WAIT':>13} "
          f"{'added':>7} {'removed':>8} {'mean_raw_P(WAIT)':>17} "
          f"{'masked_rows':>12} {'mask_kills_raw_top':>19}")
    for label in labels:
        rows = rows_by_label[label]
        n = len(rows)
        raw_argmax_is_wait = sum(
            1 for r in rows if argmax(r["policy_head_raw_recomputed"]) == WAIT)
        chosen_is_wait = sum(1 for r in rows if r["chosen_action"] == WAIT)
        added = sum(
            1 for r in rows
            if argmax(r["policy_head_raw_recomputed"]) != WAIT and r["chosen_action"] == WAIT)
        removed = sum(
            1 for r in rows
            if argmax(r["policy_head_raw_recomputed"]) == WAIT and r["chosen_action"] != WAIT)
        mean_raw_p_wait = sum(r["policy_head_raw_recomputed"][WAIT] for r in rows) / n if n else 0.0
        masked_rows = sum(1 for r in rows if r["safe_action_count"] < ACTIONS)
        mask_kills_raw_top = sum(
            1 for r in rows
            if r["safe_action_mask"][argmax(r["policy_head_raw_recomputed"])] == 0)

        print(f"{label:<16} {100*raw_argmax_is_wait/n:>17.1f}% {100*chosen_is_wait/n:>12.1f}% "
              f"{100*added/n:>6.1f}% {100*removed/n:>7.1f}% {mean_raw_p_wait:>17.3f} "
              f"{100*masked_rows/n:>11.1f}% {100*mask_kills_raw_top/n:>18.1f}%")

        # Arithmetic check: chosen==WAIT% should equal argmax(raw)==WAIT% - removed% + added%,
        # since every row is in exactly one of {raw==WAIT & not removed, raw!=WAIT & added,
        # raw!=WAIT & not added, raw==WAIT & removed} and the first two are exactly the rows
        # where chosen==WAIT. A mismatch here means the added/removed logic has a bug.
        predicted = raw_argmax_is_wait - removed + added
        if predicted != chosen_is_wait:
            print(f"  WARNING: arithmetic check failed for {label}: "
                  f"argmax(raw)==WAIT ({raw_argmax_is_wait}) - removed ({removed}) "
                  f"+ added ({added}) = {predicted}, expected chosen==WAIT ({chosen_is_wait})")

    print("\nargmax(raw)==WAIT = the raw policy head's own top-ranked action is WAIT - the direct")
    print("  measurement of whether passivity is the head's own preference (not inferred from")
    print("  what survives masking/search).")
    print("added/removed = search+mask changing the outcome relative to the raw head's own top")
    print("  pick: added = raw preferred something else but the final choice is WAIT anyway;")
    print("  removed = raw preferred WAIT but the final choice is something else. Net negative")
    print("  (removed > added) means search/mask is, on balance, slightly ANTI-passive here.")
    print("mask_kills_raw_top = how often the mask specifically zeroes out the raw head's own")
    print("  favorite action (a precise boolean check, not an argmax-vs-argmax comparison, which")
    print("  would also pick up floating-point recompute-vs-search noise near ties).")

    print("\n=== Root Q: WAIT vs. best visited safe alternative ===")
    print(f"{'checkpoint':<16} {'comparable_rows':>15} {'WAIT_loses':>11} {'mean_gap':>10}")
    for label in labels:
        rows = rows_by_label[label]
        comparable = []
        for r in rows:
            mcts_policy = r["mcts_policy"]
            q = r["search_root_q_values"]
            if mcts_policy[WAIT] <= 0:
                continue
            other_visited = [a for a in range(ACTIONS) if a != WAIT and mcts_policy[a] > 0]
            if not other_visited:
                continue
            best_other_q = max(q[a] for a in other_visited)
            comparable.append(q[WAIT] - best_other_q)
        n_comparable = len(comparable)
        if n_comparable == 0:
            print(f"{label:<16} {0:>15} {'n/a':>11} {'n/a':>10}")
            continue
        loses = sum(1 for gap in comparable if gap < 0)
        mean_gap = sum(comparable) / n_comparable
        print(f"{label:<16} {n_comparable:>15} {100*loses/n_comparable:>10.1f}% {mean_gap:>10.4f}")

    print("\ncomparable_rows = steps where WAIT and at least one other action were both actually")
    print("  visited by search (mcts_policy>0 as the visited proxy - search_root_q_values")
    print("  defaults to 0.0 for unvisited actions, which would look like a real Q if not")
    print("  excluded, conflating 'genuinely evaluated as worthless' with 'never considered').")
    print("WAIT_loses = fraction of comparable rows where search's own Q ranks WAIT below the")
    print("  best visited alternative - i.e. search's value judgment does NOT favor WAIT here,")
    print("  even on rows where the visit-count-based decision may still land on WAIT (PUCT")
    print("  weights the prior heavily at low simulation counts). This is evidence for where in")
    print("  the pipeline the passivity bottleneck sits: a high WAIT_loses rate alongside a high")
    print("  chosen==WAIT rate would mean the prior, not search's own value estimate, is")
    print("  dominating the final decision.")

    print("\n=== Effective idle: WAIT + blocked movement (v4 trace files only) ===")
    any_v4 = any(rows and all("learner_moved" in r for r in rows) for rows in rows_by_label.values())
    if not any_v4:
        print("  (no v4 trace file in this run - learner_moved not present; skipping)")
    else:
        print(f"{'checkpoint':<16} {'WAIT%':>7} {'blocked_move%':>14} {'combined_idle%':>15} "
              f"{'combined_lcb':>13} {'mean_streak':>12} {'max_streak':>11} "
              f"{'streaks>=10':>12} {'streaks>=40':>12}")
        for label in labels:
            rows = rows_by_label[label]
            n = len(rows)
            has_v4 = rows and all("learner_moved" in r for r in rows)
            if not has_v4:
                print(f"{label:<16} (predates v4 - no learner_moved field)")
                continue
            wait_frac = sum(1 for r in rows if r["chosen_action"] == WAIT) / n
            blocked_move = sum(
                1 for r in rows if r["chosen_action"] in MOVEMENT_ACTIONS and not r["learner_moved"])
            combined = sum(1 for r in rows if is_combined_idle(r))
            combined_lcb = wilson_lower_bound(combined, n)

            games = group_games(rows)
            streaks = [longest_idle_streak(steps) for steps in games.values()]
            mean_streak = sum(streaks) / len(streaks) if streaks else 0.0
            max_streak = max(streaks) if streaks else 0
            streaks_10plus = sum(1 for s in streaks if s >= 10)
            streaks_40plus = sum(1 for s in streaks if s >= 40)

            print(f"{label:<16} {100*wait_frac:>6.1f}% {100*blocked_move/n:>13.1f}% "
                  f"{100*combined/n:>14.1f}% {100*combined_lcb:>12.1f}% "
                  f"{mean_streak:>12.1f} {max_streak:>11} "
                  f"{streaks_10plus:>7}/{len(streaks):<4} {streaks_40plus:>7}/{len(streaks):<4}")

        print("\nblocked_move% = movement action (UP/DOWN/LEFT/RIGHT) chosen but the learner's")
        print("  position didn't actually change - terrain/bomb/opponent blocked it, or it lost a")
        print("  simultaneous-move collision. Not counted in WAIT% at all.")
        print("combined_idle% = WAIT% + blocked_move% (PLACE_BOMB excluded - it's a real tactical")
        print("  action even though it never moves the agent) - the true floor on passivity that")
        print("  explicit WAIT counting alone understates.")
        print("mean/max_streak = per-game longest run of consecutive combined-idle steps,")
        print("  averaged/maxed across all traced games. streaks>=N = how many games had at least")
        print("  one idle run of N+ consecutive steps (>=40 matches the prior audit's own")
        print("  qualitative observation of a ~40-tick mutual-rejection stall).")

    print("\nCaveat: steps within one game are autocorrelated (not independent trials), and "
          "different checkpoints faced the same seeds but not fully independent game "
          "trajectories once they diverge - treat the LCB as a floor against sampling noise "
          "within this data, not a formal independent-trials confidence interval. The Q-gap "
          "figures are similarly a descriptive summary over autocorrelated steps, not a "
          "hypothesis-test statistic.")


if __name__ == "__main__":
    main()
