# Evidence: KL-107 v4 clean-binary diagnostic re-run, 2026-07-11

Committed evidence for Phase 1b of `docs/experiment-memory/12-post-audit-execution.md`
(the phased plan at `C:\Users\kaust\.claude\plans\can-you-audit-the-jolly-sutherland.md`).
Supersedes `../kl107-wait-diagnostic-2026-07-11/` (v3, produced by a binary stamped
`git_commit: d6eeb4ea30d3-dirty`) with a genuinely clean-HEAD v4 binary (`f81ceccf830a`, no
`-dirty` suffix, confirmed directly on every file below) that adds two new trace fields:
`opponent_modeled_as` and `learner_moved`.

## What's here

- `crush01-iter130-agg.json`, `control03-iter130-agg.json`,
  `lever2-iter160-SD160native-agg.json`, `lever2-iter160-SD120uniform-agg.json` — the aggregate
  evaluation JSON for each config's diagnostic run (the fourth is new: lever2-160 forced to
  SD120, its non-native sudden-death timing, for direct cross-checkpoint comparability with the
  other three's SD120 evaluation environment). Each embeds its exact invocation argv,
  checkpoint/executable SHA-256, working directory, git commit stamp, timestamp, and resolved
  runtime semantics — the write-once evidence format KL-101 established, unmodified.
- `SHA256SUMS` — hashes for everything above plus the raw trace JSONLs, the three evaluated
  checkpoints, and the executable that produced this evidence.

## What's NOT here (and why)

The raw per-step trace JSONLs (`*-trace.jsonl`, 4104/4004/5349/4051 rows respectively) are
**not committed** — they live locally at `results/kl107-wait-diagnostic-v4-2026-07-11/*.jsonl`
(git-ignored, per the project's `results/` convention). Their hashes are recorded in
`SHA256SUMS` so a local copy (or a regenerated one — the evaluation is fully reproducible from
the argv in each agg file above plus the checkpoint files, or via
`tools/launch/kl107-v4-diagnostic-rerun.ps1`) can be verified against this record.

## Exact reproduction of the v3 evidence, plus one genuinely new config

The three configs shared with the v3 evidence (crush01-130, control03-130, lever2-160-SD160)
reproduce their v3 numbers **exactly** — not just within noise — confirming the underlying
computation was unaffected by the v3→v4 change (which only adds fields, doesn't touch the
existing search/masking/policy path) and that the v3 evidence's dirty-stamp issue (Phase 0b) was
purely a self-description problem, not a data problem.

## v4 stats table (for reference without re-running the analyzer)

```
=== Forced vs. chosen WAIT ===
checkpoint                    steps   WAIT%  forced forced_lcb    chosen chosen_lcb
crush01-iter130                4104   62.0%    0.8%       0.6%     61.2%      59.7%
control03-iter130              4004   62.5%    0.6%       0.4%     61.9%      60.4%
lever2-iter160-SD160native     5349   64.2%    0.6%       0.5%     63.6%      62.3%
lever2-iter160-SD120uniform    4051   68.0%    0.8%       0.6%     67.2%      65.8%

=== Raw policy head, directly measured ===
checkpoint                   argmax(raw)==WAIT  chosen==WAIT   added  removed  mean_raw_P(WAIT)  mask_kills_raw_top
crush01-iter130                          63.0%         62.0%    4.2%     5.3%             0.367                0.0%
control03-iter130                        63.4%         62.5%    4.0%     4.9%             0.369                0.0%
lever2-iter160-SD160native               64.9%         64.2%    3.4%     4.1%             0.410                0.0%
lever2-iter160-SD120uniform              69.0%         68.0%    2.8%     3.8%             0.436                0.0%

=== Root Q: WAIT vs. best visited safe alternative ===
checkpoint                    comparable_rows  WAIT_loses   mean_gap
crush01-iter130                          3477       58.0%    -0.0158
control03-iter130                        3223       55.2%    -0.0220
lever2-iter160-SD160native               4461       54.3%    -0.0100
lever2-iter160-SD120uniform              3360       53.4%    -0.0071

=== Effective idle: WAIT + blocked movement (new in v4) ===
checkpoint                    WAIT%  blocked_move%  combined_idle%  combined_lcb  mean_streak  max_streak  streaks>=10  streaks>=40
crush01-iter130                62.0%           0.3%           62.3%         60.8%         38.8         111        30/32        13/32
control03-iter130              62.5%           0.4%           62.9%         61.4%         44.9         104        31/32        19/32
lever2-iter160-SD160native     64.2%           0.6%           64.8%         63.5%         63.3         144        31/32        20/32
lever2-iter160-SD120uniform    68.0%           0.4%           68.4%         67.0%         57.5         112        31/32        19/32
```

Field definitions and caveats: see the tool's own docstring (`tools/analyze_wait_diagnostic.py`)
or `docs/experiment-memory/12-post-audit-execution.md` Phase 1b.

Reproduce with:

```powershell
python tools/analyze_wait_diagnostic.py `
  "crush01-iter130=results/kl107-wait-diagnostic-v4-2026-07-11/crush01-iter130-trace.jsonl" `
  "control03-iter130=results/kl107-wait-diagnostic-v4-2026-07-11/control03-iter130-trace.jsonl" `
  "lever2-iter160-SD160native=results/kl107-wait-diagnostic-v4-2026-07-11/lever2-iter160-SD160native-trace.jsonl" `
  "lever2-iter160-SD120uniform=results/kl107-wait-diagnostic-v4-2026-07-11/lever2-iter160-SD120uniform-trace.jsonl"
```

(requires the local, git-ignored trace files above; each agg's `invocation_argv` field is the
exact command that regenerates its own trace file if not present, or run
`tools/launch/kl107-v4-diagnostic-rerun.ps1` for all four at once — **note the one-native-GPU-
process rule: do not run this while training or another evaluate is active**)

Full context, methodology, the two provenance mistakes caught and fixed during this specific
run, and the independent audit that drove this plan: KL-107 (Linear), KL-105 (Linear),
`docs/experiment-memory/11-kl107-v3-audit-review.md`,
`docs/experiment-memory/12-post-audit-execution.md`.
