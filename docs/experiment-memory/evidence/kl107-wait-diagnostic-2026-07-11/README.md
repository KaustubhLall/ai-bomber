# Evidence: KL-107 systematic WAIT-diagnostic pass, 2026-07-11

Committed evidence for the finding written up in KL-107 (Linear) and
`docs/experiment-memory/12-post-audit-execution.md` Phase 0a: the raw policy
head's own argmax is WAIT on 63-65% of traced steps across three independently-
trained checkpoints, directly measured (not inferred) from `--trace-output`
data.

## What's here

- `crush01-iter130-agg.json`, `control03-iter130-agg.json`, `lever2-iter160-agg.json`
  — the aggregate evaluation JSON for each checkpoint's diagnostic run. Each
  embeds its exact invocation argv, checkpoint/executable SHA-256, working
  directory, git commit stamp, timestamp, and resolved runtime semantics — this
  is the write-once evidence format KL-101 established, unmodified.
- `SHA256SUMS` — hashes for everything above plus the raw trace JSONLs and both
  the binary that produced this evidence and the corrected clean-HEAD binary
  from Phase 0b (see `SHA256SUMS`'s own comments for which is which).

## What's NOT here (and why)

The raw per-step trace JSONLs (`*-trace.jsonl`, 4104/4004/5349 rows respectively)
are **not committed** — they live locally at
`results/kl107-wait-diagnostic-2026-07-11/*.jsonl` (git-ignored, per the
project's `results/` convention) and are multi-MB files not suited to version
control. Their hashes are recorded in `SHA256SUMS` so a local copy (or a
regenerated one, since the evaluation is fully reproducible from the argv in
each agg file above plus the checkpoint files) can be verified against this
record.

## Phase 0a direct-measurement stats table (for reference without re-running the analyzer)

```
checkpoint         argmax(raw)==WAIT  chosen==WAIT   added  removed  mean_raw_P(WAIT)  mask_kills_raw_top
crush01-iter130           63.0%          62.0%      4.2%    5.3%       0.367              0.0%
control03-iter130         63.4%          62.5%      4.0%    4.9%       0.369              0.0%
lever2-iter160            64.9%          64.2%      3.4%    4.1%       0.410              0.0%

checkpoint         comparable_rows  WAIT_loses   mean_gap
crush01-iter130            3477       58.0%     -0.0158
control03-iter130          3223       55.2%     -0.0220
lever2-iter160             4461       54.3%     -0.0100
```

Reproduce with:

```powershell
python tools/analyze_wait_diagnostic.py `
  "crush01-iter130-SD120=results/kl107-wait-diagnostic-2026-07-11/crush01-iter130-trace.jsonl" `
  "control03-iter130-SD120=results/kl107-wait-diagnostic-2026-07-11/control03-iter130-trace.jsonl" `
  "lever2-iter160-SD160=results/kl107-wait-diagnostic-2026-07-11/lever2-iter160-trace.jsonl"
```

(requires the local, git-ignored trace files above; each agg's `invocation_argv`
field is the exact command that regenerates them if they're not present)

Full context, methodology, and the independent audit that drove this evidence
archive: KL-107 (Linear), `docs/experiment-memory/11-kl107-v3-audit-review.md`,
`docs/experiment-memory/12-post-audit-execution.md`.
