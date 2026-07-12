# Evidence: KL-110 H3a aligned-opponent gates ablation, 2026-07-12

**Verdict (frozen discriminator, `h3a-verdict.json`, machine-readable): `mixed`** — exactly one
counted restoration among the three required checkpoints. Per the pre-registered ladder:
traces inspected (below), **no training run launched**.

All 8 runs (4 checkpoints × `--gates-opponent-model self|aligned`) on ONE binary
(commit `1e734ba`, exe `bcc7df1a...`, stamps verified in every file), 96 simulations, gates
seed block. `self` = search models both seats as the network (the deployed configuration);
`aligned` = search's internal opponent model matches the scenario's real opponent
(action-exact for NONE/CONSTANT scenarios, type-aligned for AGENT). The verdict tool
hard-checks same-checkpoint/same-binary across each self/aligned pair.

## Decision table (trap gate; corridor-clear regression check)

| checkpoint | required | trap self | trap aligned | visit lead @ bomb step | counted | corridor self→aligned |
|---|---|---|---|---|---|---|
| control03-130 | yes | FAIL | **PASS (4 steps)** | yes (BOMB 37/96 @ step 0) | **yes** | PASS→PASS |
| control-154 | yes | FAIL | FAIL (no bomb ever) | — | no | PASS→PASS |
| treatment-154 | yes | FAIL | FAIL (no bomb ever) | — | no | **FAIL→PASS** (improvement) |
| lever2-160 | descriptive | FAIL | **PASS (4 steps)** | yes (BOMB 53/96 @ step 0) | (descriptive) | PASS→PASS |

1 counted / 3 required → `mixed`. No corridor regressions; no visit-lead disagreements.

## The mechanism, read directly from the per-step records

Step-0 priors are identical within each checkpoint across modes (same weights, same
position); the entire self→aligned difference is visit allocation:

| checkpoint | BOMB prior (masked marginal) | BOMB visits self → aligned |
|---|---|---|
| control03-130 | 0.105 | 8 → **37** (argmax) |
| lever2-160 | 0.149 | 12 → **53** (argmax) |
| control-154 | 0.084 | 8 → 15 (UP stays 43) |
| treatment-154 | **0.071** | 4 → **3** (UP grows to 72) |

**Two findings:**

1. **H3a's mechanism is real.** On both pre-drift checkpoints — exactly the two where the
   raw-pass/search-fail inversion was originally observed — telling search the truth about a
   static victim flips the value signal into a BOMB visit lead and an immediate kill. The
   trap-gate suppression on those checkpoints WAS opponent-model mismatch, not value
   suppression.
2. **Prior starvation gates the rescue.** The 24 drift iterations (both arms identically)
   pushed BOMB's prior in this position below the level where 96 simulations can accumulate a
   case, no matter how favorable the (aligned) value signal — PUCT's prior weighting starves
   the action before value can speak. The treatment arm, despite its kill-sample enrichment,
   has the LOWEST bomb prior of the four. Both 154 checkpoints also wander away from the trap
   mouth entirely (UP/LEFT histograms) rather than approaching.

**Joint implication (scope-noted in the verdict JSON):** a rescue supports scenario-aligned
constrained search and also benefits from reduced opponent branching; a no-rescue/mixed does
NOT kill H3 globally (leaf values remain self-play-conditioned everywhere). Concretely here:
opponent-aware search alone would help pre-drift checkpoints but is defeated by continued
training's prior erosion; generation-side change addresses the erosion but (per the Phase 3
arms) sampler-side reweighting alone did not. The two are complementary levers, not
alternatives — this is the specific, mechanism-level input the next design pass was run to
obtain.

## Files

The 8 gates evidence JSONs (each embeds argv, checkpoint/executable SHA-256, git stamp,
resolved semantics, per-step search telemetry incl. `prior_after_safety_mask_marginal`,
visit/Q/opponent marginals, `fixed_opponent_internal_violations` — structurally 0
everywhere) + `h3a-verdict.json` + `SHA256SUMS`. Reproduce:
`tools/launch/kl110-h3a-battery.ps1` then `tools/analyze_h3a_verdict.py` (rules frozen in its
module docstring).
