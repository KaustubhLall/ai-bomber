# v7 from-scratch design: ladder-bootstrapped AlphaZero with a repaired improvement operator

**Status: PROPOSAL — no training authorized. Written 2026-07-13 from (a) the complete v6
diagnostic corpus (docs 10-13, KL-96..KL-110 evidence), (b) two commissioned literature
surveys (Pommerman/curriculum practice; AlphaZero-family algorithmics), (c) the user's
direction: fresh look, open to starting from scratch, algorithmic-opponent ladder instinct,
exploration-robustness concern.**

## 1. The diagnosis, restated once, with literature anchors

Three mutually-reinforcing causes, all now either measured here or documented in print:

1. **The equilibrium is real, not a bug.** Against an equal-skill mirror, bombs essentially
   never land (measured: 0 bomb kills in 64 mirror games at iter 131); under our terminal
   values, waiting + crush-lottery is near-optimal play. The Pommerman testbed paper names
   our exact phenomenon as the domain's canonical local optimum (Resnick et al. 2018,
   arXiv:1809.07124); the exploration trap is quantified (1 of 5^9 random sequences survives
   a bomb placement — Gao et al. 2019, arXiv:1907.11788).
2. **The improvement operator is too weak to escape it.** 96 sims over 36 joint children ≈
   2-3 visits/child; visit-count policy targets provably fail to improve on the prior in this
   regime (Danihelka et al., ICLR 2022) — measured here as Q-says-act/visits-say-WAIT and as
   targets echoing the prior (+8.6pp WAIT drift in 24 control iterations). Weil et al. 2023
   (arXiv:2305.13206) independently measured the same passivity drift in RL-trained
   MCTS-Pommerman.
3. **The search's opponent model suppresses commitment moves.** Decoupled simultaneous PUCT
   grants the modeled opponent within-simulation reactions; decoupled UCT converges to
   exploitable non-Nash points (Shafiei et al. 2009), and regret matching beat decoupled
   selection at exactly our 6x6 joint scale (Lanctot et al. 2013). Measured here: H3a —
   truthful opponent model instantly restores the trap kill wherever BOMB prior ≥ ~0.10,
   starved below ~0.08. (No prior work isolates this mechanism in simultaneous games; H3a is
   plausibly novel.)

Phase 3 (replay reweighting) failed because reward/data-side fixes alone were never
sufficient in any published Pommerman result either — every winning recipe paired curriculum
with hard-coded safety and (often) imitation. v6's 50/50 mirror/heuristic blend from
iteration 1 is structurally unlike every winning recipe found.

**Verdict on rescue-vs-restart: restart.** The v6 lineage's weights, replay, and optimizer
state encode the equilibrium; two bounded interventions failed to move it; the literature
recipe differs from v6's at the foundation (bootstrap + sequenced ladder), not at the margin.
v6 artifacts remain frozen baselines + the diagnostic corpus.

## 2. What v7 keeps unchanged

All KL-101 integrity machinery (manifests, fork provenance, locks, write-once evidence), the
C sim + encoder, trace v4, the six tactical gates + achievability references, the
wait-diagnostic and verdict tooling, seed hygiene, KL-100 claim discipline, and the matched-
arm experiment template. v7 is a new recipe on proven infrastructure.

## 3. The recipe (four stages, each with a measured gate — never iteration-count promotion)

### Stage 0 — search/target repairs (code, before any training; each lands with tests)

| # | change | anchor | note |
|---|--------|--------|------|
| 0.1 | Dirichlet α 0.3 → **1.5** (fraction 0.25 unchanged) | AZ-family small-action-space practice (BRExIt α≈1.41 on 7 actions; direction corroborated across every tuned small-|A| example found) | semantic field; one line |
| 0.2 | **KataGo forced playouts + policy-target pruning**, per seat | Wu, arXiv:1902.10565; independently replicated by Trudeau & Bowling 2023 | directly counters measured prior-starvation; bolt-on to existing PUCT |
| 0.3 | Temperature: τ=1 for 30 steps → **τ annealed 1.0→0.25 over steps 0-60, argmax after** | data-diversity collapse after step 30 measured in idle-streak structure | semantic field |
| 0.4 | **Search-contempt prototype** (freeze opponent-seat PUCT adaptation after N_scl≈5 visits at interior nodes), behind a flag, default off | Joshi 2025, arXiv:2504.07757; our H3a | cheap; validated via gates before use in training |
| 0.5 | Canary wiring: trap-gate BOMB prior + full gates + wait-diagnostic **every eval interval**, recorded in metrics | our own drift finding | drift must never be invisible again |

Pre-registered fallbacks (NOT in v7.0): Gumbel completed-Q targets (if forced playouts don't
close the echo loop — the only mechanism with a proof at 2-3 visits/child, but no simultaneous
-game precedent exists, so it's our second swing, not our first); regret-matching root
selection (the sound simultaneous fix, bigger swap); COMBAT-style destination+bomb action
redesign (two Pommerman teams concluded the bare BOMB action is where the optimum lives —
arXiv:1812.07297 — but this is a KL-106-scale ABI change).

### Stage 1 — teacher bootstrap (IL), guarding the named handoff failure

Behavior-clone policy from **MCTS-256-vs-MCTS-256 games** (they demonstrably bomb and trap)
plus heuristic-vs-heuristic games for diversity; train value separately on outcomes
(Meisheri et al. 2019, arXiv:1911.04947: shared trunks + default entropy cause "Bombing
Collapse" during the RL handoff — mitigations: separate/staged value warmup, an entropy floor
on the policy loss, and the Stage-0.5 canary from iteration 0). Gate to Stage 2: gates ≥3/6
deployed (search mode) AND bomb-usage rate in probe games within 2x of the teacher's.

### Stage 2 — algorithmic ladder with gated promotion (the user's instinct, literature-blessed)

Rungs, all agents already in the repo: `random` → `evasive` (the moves-but-never-bombs rung —
Skynet's "SmartRandomNoBomb" insight: deny wins-by-opponent-suicide so the reward signal is
real blasting skill, arXiv:1905.01360) → `greedy_crate` → `heuristic` → `alphabeta` →
`MCTS-64` → `MCTS-256`. Promotion at **≥55% seat-balanced win rate** on the standing
diagnostic block (Huynh et al. 2024, arXiv:2407.00662), demotion guard at <45%.
**Scripted rungs are never retired**: permanent ≥25% share of collection sampled over all
passed rungs, weighted toward the worst current win rate (TiZero's (1-winrate)^2,
arXiv:2302.07515). **Backplay/Go-Exploit starts**: 20-30% of games start from constructed
near-contact states (gates machinery) and from an archive of previously-visited states
(Resnick et al. arXiv:1807.06919 — 41.6% vs near-total failure for standard starts; Trudeau &
Bowling's archive starts stacked with KataGo's tricks).

### Stage 3 — self-play refinement (only after the full ladder is passed)

Mirror games introduced at 20% → grown as gates hold; within the self-play share, **80%
current / 20% past-self snapshots** (independently validated by OpenAI Five arXiv:1912.06680
and TiZero); scripted pool floor stays. Search-contempt or aligned-type league modeling per
Stage 0.4 evaluation. Failure tripwire: if the canary shows two consecutive eval intervals of
trap-prior decline below 0.10 while gates regress, self-play share freezes and the incident
is written up before any knob turns.

### Reward re-derivation (one deliberate change, then frozen)

The only v7.0 terminal-value change: **arena-crush win 0.3 → 0.1** — removing most of the
passive-lottery payoff while keeping win>loss ordering. (Kept from v6: bomb-kill 1.0, loss
−1.0, timeout −0.5.) Reconsidered and deliberately kept: mutual-death −0.2 (better than
timeout — mild aggression tiebreak near the wall). No dense per-step shaping in v7.0: the
ladder + Backplay + forced playouts supply the aggression gradient the shaping would fake,
and every added semantic is another confound; Skynet-style event shaping is the pre-registered
reserve lever. NOTE: kill-type-differentiated terminal values have no literature coverage at
all (both surveys) — we are in undocumented territory here by construction; the canary is the
guard.

## 4. Measurement plan (unchanged discipline)

Same matched-arm template for any within-v7 ablation; gates + wait-diagnostic + SD-on/SD-off
per eval interval; per-stage promotion evidence archived write-once; KL-100 checklist for any
claim; fresh final-holdout blocks pre-registered before any strength claim (A3/B3 remain
burned). Stage gates are behavioral and measured — never clock- or iteration-based.

## 5. Cost sketch

Stage 0 ≈ 2-3 executor units + review (days, no GPU beyond ctest/gates). Stage 1 teacher
collection: MCTS-256 games are ~30s/game single-threaded-ish → a few thousand games ≈ 1-2
GPU-days interleaved (or overnight batches). Stages 2-3: at measured ~8 min/iteration,
100-150 iterations ≈ 2-3 overnights per stage-block, with promotion gates deciding actual
length. Total to a defensible go/no-go on the recipe: roughly a week of part-time GPU, all
under advisor-gated launches.
