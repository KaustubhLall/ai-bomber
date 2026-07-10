# 02 — Linear Issue Ledger

_Last updated: 2026-07-08. Source: Linear workspace, team **Kaus** (`KL`)._

- **Project:** AI Bomber Search & Self-Play —
  https://linear.app/hutechventures/project/ai-bomber-search-and-self-play-f95f8784ab78
  (id `bb080cad-ef82-42e2-bf0b-5c603013d8e5`, lead Kaustubh Lall, priority High, In Progress).
- Note: issues live under team **Kaus** (`KL-*`). Some issue bodies still reference the
  older `AI-BOMBER-##` scheme and a `HUT-85` cross-link from before the move.

## Active / pending

| ID | Title | Status | Notes |
| --- | --- | --- | --- |
| **KL-94** | AI-BOMBER-16: Native C++/LibTorch AlphaZero scale and grokking run | **In Progress** | The experiment being monitored. Native trainer, long-horizon generalization run. Acceptance: native build vs LibTorch w/o Python in hot path; CUDA benchmark + native CTest smoke; fresh/reload/resume/shutdown verified; scaled run with recoverable checkpoints + live ETA; untouched eval materially improves *or* honestly establishes the next bottleneck; cross-game playbook + exact commands in docs. |
| KL-88 | AI-BOMBER-10: Human-vs-agent playable mode and input-safe match setup | **Backlog** | Not started. Human policy + opponent select, input-safe match setup. |

## Completed

| ID | Title | Status |
| --- | --- | --- |
| KL-93 | AI-BOMBER-15: Full-simulator AlphaZero self-play with durable recovery | Done (2026-07-08) — the prototype KL-94 scales up. |
| KL-92 | AI-BOMBER-12: Match history, replay catalog, per-match inspection | Done |
| KL-91 | AI-BOMBER-14: Adversarial evaluation audit and meaningful-results gate | Done — the anti-fake-progress gate (both seats, strong opponents, self-kill/idle diagnostics). |
| KL-90 | AI-BOMBER-11: Real-time simulation clock and inspectable playback controls | Done |
| KL-89 | AI-BOMBER-13: Live policy-vs-policy arena and matchup launcher | Done |
| KL-87 | AI-BOMBER-01: Search API foundations for cloneable joint-step simulation | Done — `env_copy`/clone, legal actions, `env_step_joint`. |
| KL-86 | AI-BOMBER-02: Heuristic evaluator and baseline evaluation metrics | Done — `evaluator_score_state`, used at search leaves + draw tiebreak. |
| KL-85 | AI-BOMBER-03: Alpha-beta/minimax search agent baseline | Done |
| KL-84 | AI-BOMBER-04: UCT/MCTS planning agent baseline | Done — the retained "upper boundary" opponent. |
| KL-83 | AI-BOMBER-05: Tournament runner and self-play league/evaluation harness | Done |
| KL-82 | AI-BOMBER-06: AlphaZero-lite self-play pipeline | Done — the first self-play path (`tools/learning.py alphazero-lite`). |
| KL-81 | AI-BOMBER-07: PPO self-play baseline for comparison | Done |
| KL-80 | AI-BOMBER-08: Visualizer search/self-play explanations and policy clarity | Done — the current `polish/viz-and-policy-clarity` branch continues this thread. |
| KL-79 | AI-BOMBER-09: Superhuman-results benchmark and experiment tracking protocol | Done — `docs/EXPERIMENTS.md`, seed suites, claim standards. |

## Reading order for context

KL-79 (protocol) → KL-82 (alphazero-lite) → KL-91 (adversarial eval gate) → KL-93
(full-sim AlphaZero + recovery) → **KL-94 (native scale + grokking run, current)**.
