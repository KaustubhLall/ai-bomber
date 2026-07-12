"""KL-107 systematic WAIT-diagnostic pass: aggregate forced-vs-chosen WAIT across one or more
--trace-output files (typically one per checkpoint), with a Wilson lower confidence bound on
each proportion so the finding is load-bearing rather than a single spot-check's raw percentage.

"Forced" means WAIT was the position's only safe action (safe_action_count==1); this is not a
policy choice and says nothing about passivity. "Chosen" means WAIT was picked with real
alternatives available (safe_action_count>1) - this is the actual passivity signal. A high WAIT%
that turns out to be mostly forced would mean the environment, not the policy, explains it; a
high WAIT% that is mostly chosen means the policy prefers it despite having other options.

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


def wilson_lower_bound(successes: int, n: int, z: float = 1.96) -> float:
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

    print(f"{'checkpoint':<16} {'steps':>7} {'WAIT%':>7} "
          f"{'forced':>7} {'forced_lcb':>10}   {'chosen':>7} {'chosen_lcb':>10}   "
          f"{'raw==masked_top':>16}")
    for label, path in zip(labels, paths):
        rows = load_rows(path)
        n = len(rows)
        wait_chosen_total = sum(1 for r in rows if r["chosen_action"] == WAIT)
        forced = sum(1 for r in rows if r["wait_forced"])
        chosen_not_forced = sum(
            1 for r in rows if r["chosen_action"] == WAIT and not r["wait_forced"])
        raw_top_eq_masked_top = sum(
            1 for r in rows
            if max(range(6), key=lambda i: r["policy_head_raw_recomputed"][i])
            == max(range(6), key=lambda i: r["policy_prior_after_safety_mask"][i]))

        forced_frac = forced / n if n else 0.0
        chosen_frac = chosen_not_forced / n if n else 0.0
        forced_lcb = wilson_lower_bound(forced, n)
        chosen_lcb = wilson_lower_bound(chosen_not_forced, n)

        print(f"{label:<16} {n:>7} {100*wait_chosen_total/n:>6.1f}% "
              f"{100*forced_frac:>6.1f}% {100*forced_lcb:>9.1f}%   "
              f"{100*chosen_frac:>6.1f}% {100*chosen_lcb:>9.1f}%   "
              f"{100*raw_top_eq_masked_top/n:>15.1f}%")

    print("\nforced = WAIT was the only safe action (not a policy choice, environment-forced).")
    print("chosen = WAIT picked with real alternatives available (the actual passivity signal).")
    print("*_lcb  = one-sided 95% Wilson lower confidence bound on that fraction across all "
          "traced steps in the file - a conservative floor, not a point estimate, on how much "
          "of the behavior is real regardless of trace-length sampling noise.")
    print("raw==masked_top = how often the safety mask leaves the raw policy's own top action "
          "untouched; low values would mean the mask itself is doing a lot of the passivity "
          "work rather than the policy head's own preference.")
    print("\nCaveat: steps within one game are autocorrelated (not independent trials), and "
          "different checkpoints faced the same seeds but not fully independent game "
          "trajectories once they diverge - treat the LCB as a floor against sampling noise "
          "within this data, not a formal independent-trials confidence interval.")


if __name__ == "__main__":
    main()
