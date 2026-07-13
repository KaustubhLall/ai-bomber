# 08 — Simulator Fidelity Audit (vs canonical Bomberman)

_2026-07-08. Multi-agent audit (49 agents), adversarially verified. 45 findings: 40
CONFIRMED, 1 PLAUSIBLE, 3 unverified nits, 1 refuted. Full report + per-finding JSON in
this session's scratchpad (`fidelity-report.md`, `fidelity-findings.json`)._

## Verdict

The **object-level rules are faithful (~7/10)**: 13×11 board, border+even/even pillars, ~50%
crates, L-spawn clearing, plus-shaped blast (stops at walls, destroys first crate, includes
center), chain reactions (re-entrancy safe), ammo return, self-damage, one-bomb-per-tile,
step-off-own-bomb. The **joint-move resolver is the strongest subsystem** (deterministic,
overlap-free, symmetric). Terminal detection (win/loss/mutual-death-draw/buzzer-beater/
timeout) is correct. The divergences are **temporal and systemic**, and they are the
mechanical substrate of the draw collapse.

## Confirmed issues (ranked)

| Sev | Kind | File:line | Issue |
|--|--|--|--|
| **High** | bug | trainer.cpp:249 | Both-alive **timeout valued ≈0** in AZ target; `timeout_penalty`/`stall_penalty` are dead code in native self-play; tanh(tactical) tie-break rewards **farmable material** → passive stalling is a safe ≈0. |
| **High** | fidelity | blast.c:63 | **Flame is instantaneous** (lethal only on the detonation tick; tile safe next tick) → no zoning/area-denial → kills rarely land. |
| **High\*** | fidelity | rules.c:79 | **No sudden-death / arena shrink** → nothing forces engagement; mutual avoidance to the cap is stable. |
| Med | fidelity | encoding.c:49 | **Egocentric 11×11 view < 13×11 board**, no global opponent bearing → a >5-tile-away opponent is *invisible* → agent farms in ignorance. |
| Med | fidelity | rules.c:56 | **SPEED powerup inert** (never affects movement) and unencoded → a paid no-op; removes the out-tempo tool. |
| Med | fidelity | danger.c:41 | Predictive danger ignores chain-reaction timing (understates imminence). Not a draw driver. |
| Low | bug | danger.c:36 | `current_blast` permanently 0 → **dead NN channel 8** (fixed for free by persistent flame). |
| Low | bug | danger.c:150 | `danger_would_trap_agent` hardcodes fuse=4 (ignores `cfg->bomb_timer`) + dist-4 off-by-one. |
| Low | fidelity | blast.c:82 | Powerups **indestructible** by later blasts (only crates convert). |
| Low | fidelity | env.c:146 | Two-agent **swap/rotation passes through** (solid otherwise) — internally inconsistent. |
| Low | bug | env.c:79 | Legal-action set ignores agent occupancy → a move into a stationary opponent is advertised legal, then rejected + penalized (asymmetry). |
| Low | fidelity | map.c:7 | 2-player spawns are **adjacent top corners (reflection)**, not diagonal-opposite (rotation) → perfect-mirror strategy = guaranteed draw. |
| Nit | bug | blast.c:24 | `compute_blast_tiles` no bounds guard vs `MAX_BLAST_TILES` (margin 3, not currently triggerable). |
| Nit | bug | encoding.c:21 | Uncapped ammo/range can push normalized features >1.0 (OOD). |

\* rated medium in a minority of passes (intentional RL simplification, cf. Pommerman); High for draw-collapse leverage.

**Refuted:** per-tile-random crate layout — this *matches* canonical Bomberman (soft blocks randomized each round); not a bug.

## Draw-collapse levers (audit's recommended sequencing)

1. **Value-target fix** (cheapest, pure training code): both-alive timeout → configurable negative (≈−0.5); drop the material tanh tie-break. Removes the "safe ≈0 stall" attractor.
2. **Diagonal 2-player spawns** (cheap, fidelity): weakens the exact-mirror-draw equilibrium.
3. **Persistent flame** (medium, fidelity): raises lethality so cornering-and-burning converts; revives channel 8.
4. Speed powerup functional (medium) — or remove from drop table to stop the paid no-op.
5. **Global opponent bearing** (medium, fidelity): always-populated relative dx/dy so agents can navigate to engage instead of farming blind.
6. **Sudden-death / arena shrink** (most invasive, fidelity): the definitive structural cure; makes timeouts (and lever #1) moot.

Audit recommendation: **#1 + #3 are the highest-leverage pair**; #6 is the definitive fix if training-side changes prove insufficient.

## v3 implementation decision (this cycle)

Bundle #1 + #2 + #3 + #5 + the cheap correctness bug fixes in one rebuild (fresh run —
mechanics + channel count change make old weights incompatible). Defer #6 (sudden-death) to
v4 if draws persist, and #4 speed (implement true sub-tick speed later; for now **remove
SPEED from the drop table** to stop the wasted-reward no-op). Concretely:

- **state.h**: add `flame_ttl[H][W]`, `flame_owner[H][W]`.
- **config**: add `flame_duration` (battle default 2).
- **blast/bombs/env**: persistent flame — set ttl on explosion, post-move lethal pass, decrement per tick, chain via flame; destroy powerups on blast tiles; blast bounds guard; drop SPEED from powerup draw.
- **danger**: `current_blast` ← `flame_ttl>0`; chain-reaction fixpoint for `time_to_blast`; `danger_would_trap` uses `cfg->bomb_timer`.
- **env**: agent-aware legal actions (no phantom-legal moves into occupants); reject 2-cycles in move resolution.
- **encoding**: channel 8 = flame; **+2 channels** opp_dx, opp_dy (17→19); cap ammo/range; bump `BOMBER_TRAINING_ABI_VERSION` 6→7.
- **map**: diagonal 2-player spawns; fix `is_spawn_safe_tile` spawn_count.
- **trainer.cpp**: `terminal_training_value` → negative `timeout_draw_value` for both-alive timeout (thread the value); per-seat value assignment in `collect_self_play`/teacher.
- **tests**: update blast/bombs/danger/determinism/hardened; add persistent-flame + timeout-value + trap-fuse regression tests.

**Verify before the long run:** dependency-free + native test suites green, then a headless
baseline decisive-rate check (heuristic-vs-heuristic, MCTS-vs-MCTS) to confirm persistent
flame raises decisiveness — *before* spending hours on the neural run.
