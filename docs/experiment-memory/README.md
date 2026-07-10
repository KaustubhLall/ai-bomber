# AI-Bomber AlphaZero — Experiment Memory

Self-contained working memory for the AlphaZero-style self-play experiment in this
repo. Written and maintained by Claude (Opus 4.8) during the code audit + overnight
monitoring session started **2026-07-08**. Kept updated as the run and understanding
evolve.

> Canonical location: `docs/experiment-memory/` in the `ai-bomber` repo
> (`C:\Users\kaust\IdeaProjects\ai-bomber`). A pointer to this folder is stored in
> Claude's persistent memory (`MEMORY.md` → `ai-bomber-experiment.md`).

## How to use this folder

Read `00-overview.md` first. Each file is standalone; you do not need the others to
understand one. When facts change (new iteration, run finishes, code changes), update
the relevant file and bump its "Last updated" line.

| File | What it holds |
| --- | --- |
| [00-overview.md](00-overview.md) | What the experiment is, the goal (KL-94), and the current bottom line. |
| [01-architecture.md](01-architecture.md) | System architecture: C sim → C bridge → native C++ PUCT → LibTorch. Model, encoding, self-play/eval/checkpoint flow. |
| [02-linear-issues.md](02-linear-issues.md) | Linear project + full issue ledger (done and pending), with IDs and links. |
| [03-running-experiment.md](03-running-experiment.md) | The live run: process, run-dir, exact command, config, metrics schema, seed partitions, current trajectory. |
| [04-code-audit.md](04-code-audit.md) | Findings from the full multi-agent code audit: **no result-invalidating defect**; 4 medium eval/promotion issues (one confounds this run's early metrics + best.pt). |
| [05-monitoring-log.md](05-monitoring-log.md) | Hourly health-check log for the overnight run (5 checks). |
| [06-commands-and-map.md](06-commands-and-map.md) | Build/train/resume/evaluate commands and a source-tree map with gotchas. |
| [07-grokking-campaign.md](07-grokking-campaign.md) | **Active (KL-96):** the draw-collapse diagnosis, sim-fidelity gaps, and the intervention plan to reach superhuman. |
| [08-sim-fidelity-audit.md](08-sim-fidelity-audit.md) | Findings from the multi-agent audit of the C sim vs canonical Bomberman (fidelity gaps + bugs + fixes). |

## One-line status

See the top of [05-monitoring-log.md](05-monitoring-log.md) for the latest health check.
The native run **COMPLETED CLEANLY at ~05:33 (100/100 iterations)** on 2026-07-08 — natural
finish, best model = `best.pt` (iteration 70, 0.656 vs heuristic). Outcome: plateau, no
grokking.
