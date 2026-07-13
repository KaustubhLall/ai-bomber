# 04 — Code Audit

_Last updated: 2026-07-08. Method: 7-dimension multi-agent static audit (32 agents, ~1.9M
tokens), each finding adversarially verified (a skeptic tried to refute it) then
synthesized. Read-only; nothing was modified or rebuilt. 24 findings raised → 3 refuted,
21 survive (15 CONFIRMED, 6 PLAUSIBLE): 4 medium, 14 low, 6 nit (corrected severities)._

## Bottom line

**The training and evaluation code is broadly trustworthy. No CONFIRMED
result-invalidating defect was found; nothing critical/high survived verification.** The
two trainers drive the *same* authoritative C simulator + encoder over a verified ABI, so
they cannot silently learn from different games. Decoupled zero-sum PUCT, marginal policy
targets, value-backup signs, replay, AdamW + cosine LR, and atomic checkpointing are all
correct. The published headline numbers (random 96.88%, heuristic 57.81%, MCTS 37.50%)
reproduce exactly from `(wins + 0.5·draws)/games`; draws are reported separately (never
counted as wins); eval is role-balanced across both seats; idling/timeouts can never score
as wins. So the reported results are **arithmetically honest** and free of
draw-as-win / role-bias / idling artifacts.

The issues that matter cluster in **evaluation/promotion validity**, not core learning.

## ⚠️ Most important for the *current* run (grokking-v1)

**Finding #1 (bootstrap heuristic leaks into the evaluation search) directly affects this
run.** The tactical-heuristic value blend used to warm up search is applied in *both*
self-play and evaluation, weighted `0.75·(1 − iter/30)`. So the in-training "vs heuristic"
scores during the bootstrap window are network+heuristic hybrids, not the standalone net:

| eval iter | heuristic_weight in eval search | reported heuristic score |
| ---: | ---: | --- |
| 5 | 0.625 | 0.594 |
| 10 | 0.500 | 0.594 |
| 20 | **0.250** | **0.641 → promoted best.pt** |
| 30 | 0.000 (clean) | 0.531 |
| 40 | 0.000 (clean) | 0.609 |

**Consequence:** `best.pt` was promoted at iteration 20 under a 25%-heuristic-aided eval,
setting `best_score = 0.640625`. The later **clean** evals (0.531 at 30, 0.609 at 40) are
*below* that bar, so (a) "best" model selection for this run is confounded, and (b) the
genuine standalone-network heuristic score is ~0.53–0.61 — the plateau/no-grokking reading
is if anything **stronger** than the raw metrics suggest. Treat the iter-20 `best.pt` and
all iter ≤ 25 scores with caution; trust the iter ≥ 30 clean evals. A clean re-eval of any
checkpoint via the standalone `evaluate` path (which should force `heuristic_weight=0`
once fixed) is the way to get an unconfounded number.

This does **not** invalidate the previously published holdout numbers, which come from the
separate NumPy-run untouched 150001-block holdout evaluated at iteration 45 (weight = 0).

## Findings table (corrected severity; refuted dropped)

| # | Sev | Dimension | File:line | Issue | Verdict |
|--|--|--|--|--|--|
| 1 | **Med** | puct / optim | `native/trainer.cpp:522`, `:969` | Bootstrap heuristic value blend leaks into the **evaluation** search (iter<30) → early metrics & bootstrap-window best.pt are net+heuristic hybrids | CONFIRMED |
| 2 | **Med** | eval-integrity | `native/trainer.cpp:1173` | Native promotion gate omits two-seat head-to-head vs incumbent — promotes on max heuristic score alone, no margin (Python does this correctly) | CONFIRMED |
| 3 | **Med** | python-parity | `alphazero/trainer.py:502` | Python promotes the **first** evaluated (near-random) iteration to best.npz, bypassing the random≥0.9 gate | CONFIRMED |
| 4 | Low | puct / bridge / env | `trainer.cpp:227`, `training_api.c:110`, `alphazero/env.py:176` | `outcome()` scores a decisive kill on the exact final tick as a **draw** (timeout check precedes alive check, inverting the engine's terminal priority) | CONFIRMED |
| 5 | Low | puct | `native/trainer.cpp:421` | Dirichlet noise applied over the joint 36-cell dist instead of per-seat marginals → per-seat exploration acts like Dir(1.8) not Dir(0.3) (milder) | PLAUSIBLE |
| 6 | Low | optim | `native/trainer.cpp:884` | Cosine LR horizon rescales (non-monotonic LR jump) if `--iterations` changes on resume (`iterations` not in config signature) | CONFIRMED |
| 7 | Low | checkpoint | `native/trainer.cpp:1053` | Checkpoint temp not fsync/FlushFileBuffers'd before the atomic rename → power-loss can leave `latest.pt` on unflushed tail blocks | CONFIRMED |
| 8 | Low | checkpoint | `native/trainer.cpp:557` | Atomic rename has no retry; a transient reader lock on `latest.pt` (evaluate/viz/AV) aborts the whole run at a checkpoint boundary | CONFIRMED |
| 9 | Low | checkpoint | `native/trainer.cpp:577` | config signature is structural-only; non-structural hyperparams change silently on resume and `config.json` is overwritten → provenance loss | CONFIRMED |
| 10 | Low | checkpoint | `native/trainer.cpp:1082` | Hand-rewound resume restores stale `best_score` without reconciling the newer on-disk `best.pt` → weaker model can overwrite stronger best | PLAUSIBLE |
| 11 | Low | eval-integrity | `native/trainer.cpp:959` | Native `evaluate` reuses the promotion selection block (900001+) as its only eval set — no built-in untouched holdout | CONFIRMED |
| 12 | Low | eval / python | `alphazero/trainer.py:322`, `:296` | Self-play/teacher seeds drawn from full int32 range, structurally overlapping validation/holdout blocks; disjointness is probabilistic (~1e-5), not by construction | PLAUSIBLE |
| 13 | Low | python-parity | `alphazero/trainer.py:518` | Metrics row fsync'd **before** the checkpoint → duplicate/orphan metrics row on crash-resume (native ordering is safer) | CONFIRMED |
| 14 | Low | python-parity | `alphazero/trainer.py:430` | `evaluate()` reloads the full replay buffer every iteration just to get model weights — wasted hot-path I/O | CONFIRMED |
| 15 | Low | env | `env/bomber_danger.c:150` | Safe-action trap check hardcodes bomb `timer=4`; would desync the safe mask if `bomb_timer` were ever changed (matches config today) | PLAUSIBLE |
| 16 | Nit | optim | `native/trainer.cpp:529` | (Clarification) bootstrap shapes only search value/visits, never the value-regression target — by design, not a defect | CONFIRMED |
| 17 | Nit | bridge / env | `env/bomber_env.c:228` | SAFE-mask reads uninitialized `action_safe[]` for a dead/out-of-range perspective (latent; trainers only query on non-terminal states) | PLAUSIBLE |
| 18 | Nit | bridge | `env/bomber_env.c:212` | `env_state_hash` is history-sensitive (raw struct + stale bomb bytes), not canonical; harmless today (only seeds the fixed baseline opponent) | PLAUSIBLE |

## Medium findings — detail & fix

**#1 Bootstrap heuristic in eval search** — `expand_and_backup()` is shared by self-play
and eval and always blends the tactical heuristic when
`heuristic_weight = bootstrap_value_weight·(1 − iteration/bootstrap_value_iterations) > 0`;
`evaluate_baseline()` builds its `BatchedMcts` with the live `iteration` and no
eval/self-play flag. **Fix:** thread an `evaluation`/`bootstrap_enabled=false` flag through
`BatchedMcts::search()` so eval forces `heuristic_weight=0` (or construct the eval search
with `iteration ≥ bootstrap_value_iterations`). Keep the blend only in `collect_self_play`.

**#2 Native promotion gate** — gate is `random ≥ 0.90 && heuristic > best_score`, with **no
eval against the previous best checkpoint** anywhere in `trainer.cpp`. Diverges from the
documented/Python KL-91 gate (incumbent head-to-head > 0.5+margin). Over a long run
`best.pt` tracks heuristic-block score, not proven relative strength. **Fix:** before
promoting, load `best.pt` and run a role-balanced learner-vs-incumbent match; require
`head-to-head > 0.5 + promotion_margin AND random ≥ 0.9 AND heuristic ≥ best_score − margin`.
Add `promotion_margin` to the native `TrainConfig`.

**#3 Python first-iteration promotion** — `promoted = bool(evaluation) and (not
best_path.exists() or <quality conditions>)`; the `not best_path.exists()` branch has no
score gate, so iteration 1 (near-random) becomes `best.npz`, and subsequent `previous_best`
head-to-heads run against that weak baseline. **Fix:** apply the same quality bar on the
first promotion (`random_score ≥ 0.9`). Also `incumbent_score` is misnamed (it is the
challenger's win-rate vs the prior best) and `candidate ≥ best − margin` lets `best_score`
ratchet *down*.

## Low findings worth a follow-up

- **#4 final-tick draw** is a real cross-cutting correctness inconsistency in **three**
  places (native `outcome()`, `training_api.c`, Python `env.py`): a kill on the exact
  `max_steps` tick is a WIN in the engine but scored a DRAW by `outcome()` because the
  `step >= max_steps` check precedes the alive check. Rare (<1% of games) and softened by
  the tactical value fallback, but it should be fixed identically in all three to keep
  native/Python agreement. **Fix:** compute alive flags first; return ±1 on a decisive
  elimination even at the horizon; return 0 only for a true both-alive timeout / mutual death.
- **#7/#8** checkpoint durability hardening (fsync-before-rename; retry the rename on a
  Windows sharing violation) — cheap insurance for unattended long runs.
- **#11** add a `holdout_seed_base` disjoint from `evaluation_seed_base` so `evaluate_only`
  defaults to an untouched block (today you must pass `--eval-seed-base 150001` by hand).

## What was checked and found SOUND (coverage)

- **Search/policy:** joint 36-action enumeration + both-seat marginalization (index-consistent),
  decoupled PUCT signs, fixed-perspective zero-sum backup (correct for simultaneous moves),
  SAFE/legal masking with legal fallback, root-only Dirichlet, temperature/argmax split,
  deterministic OpenMP (thread-local envs, serial RNG). No crash or sign-flip.
- **Optimization/replay:** ring buffer, loss/entropy math, AdamW + weight decay, FP32-opt /
  BF16-infer boundary, BatchNorm toggling, value-target sign, cosine LR (matches the metrics
  trace). No stale-index/double-count/off-by-one/BF16-in-backward bug.
- **Checkpoint/recovery:** atomic `latest.pt` swap, immutable snapshots + best after
  finalization, emergency on SIGINT, a single archive that saves *and reloads* a complete,
  mutually-consistent state (weights, BN buffers, AdamW moments/step, replay tensors + ring
  position + order, counters, mt19937_64 state, best_score, signature), signature checked
  before load, `--fresh` clean, orphan `.tmp` never loaded, `--iterations` total. No
  saved-but-not-loaded field, no counter drift, no metrics duplication on normal resume.
- **Encoding/ABI:** centered 17ch×11×11 encoder, consistent local↔world translation, defined
  off-board padding, each channel written once, in-bounds, `[C][y][x]` order matching the
  `{N,C,H,W}` reshape on both native and Python → identical features. ABI v6 matched.
- **Eval integrity:** numbers reproduce from the score formula; role-balanced; timeouts/mutual
  deaths scored 0; draws separate; conservative opponent modeling; Python gate implements full
  KL-91. No draw-as-win / role-bias / actual seed leakage / idling-as-strength in the results.
- **Python parity:** ctypes signatures match `training_api.h` exactly (no Win64 truncation),
  atomic checkpoint writes, RNG state persisted, same C sim/encoder, PUCT/marginalization/
  value-backup/bootstrap/tanh-shaping match native line-for-line.
- **Env determinism:** struct-local splitmix64, no global/hidden state, full-memcpy `env_copy`
  with `opponent` nulled at every clone, byte-identical clone continuation (PUCT fidelity),
  order-independent seat-symmetric simultaneous resolution, mirror-symmetric spawns + seat
  alternation (no player-0 advantage), antisymmetric value targets identical across trainers.

## Refuted (excluded)

Self-play joint-visit sampling as a training-distribution bias (marginal law preserved);
the "genuinely strong" wording as an overstatement (doc substantiates with draw-free facts
and discloses draws); Python↔native optimizer/architecture divergence as a "bug" (both
internally correct; docs already scope parity to the shared simulator).

## Provenance

Full synthesized report + per-finding JSON are in this session's scratchpad
(`audit-report.md`, `audit-findings.json`); the workflow journal is under
`.claude/.../subagents/workflows/wf_1c65b52d-d5b/journal.jsonl`.
