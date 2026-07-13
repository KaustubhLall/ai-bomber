# STAGE-GATED - do not run without the planner's launch-gate sign-off. See
# docs/experiment-memory/14-v7-from-scratch-design.md (status line: "Stage 0 in implementation;
# training launches remain individually gated per the standing advisor/orchestrator protocol").
# This script establishes the FRESH v7 lineage (results/v7-stage1) - a from-scratch restart per
# doc 14 section 1's verdict ("restart" over "rescue": v6's weights/replay/optimizer state
# encode the passivity equilibrium the whole recipe exists to escape), NOT a --fork-from
# continuation of any v6 arm. It is a SKELETON: every Stage-0 code-level semantic (temperature
# anneal, Dirichlet alpha, the reward re-derivation) is wired in EXPLICITLY below, but the
# actual Stage-1 collection mix (algorithmic-ladder / teacher-game composition) is still
# undesigned - see the commented --league-heuristic-fraction placeholder - so this script is
# not a considered launch plan, only the template one becomes once Stage-1 IL design lands.
# --iterations / --lr-schedule-updates below are PROVISIONAL placeholders, not a deliberated
# campaign length - review and adjust both at actual launch-gate time.
#
# v7.0 Stage 0 semantics carried here (doc 14 section 3 table):
#   0.1 Dirichlet alpha 0.3 -> 1.5 (--dirichlet-alpha), fraction unchanged (0.25)
#   0.3 Temperature anneal 1.0 -> 0.25 over 60 steps, argmax after (--temperature-anneal
#       --temperature-final --temperature-steps)
#   Reward re-derivation: arena-crush win 0.3 -> 0.1 (--arena-crush-win-value); selfkill-win,
#       timeout-draw, mutual-death values kept at their v6 settings (see doc 14 "Reward
#       re-derivation" section - only arena-crush-win-value changes in v7.0)
# 0.2 (forced playouts) and 0.4 (search-contempt) are later units - nothing to wire here yet.
# 0.5 (the drift canary) needs no launcher flag: trainer.cpp now runs it automatically at every
#   --eval-interval, in-process, recorded into metrics.jsonl's "gates" field - see
#   run_gate_canary()/append_metrics() in src/training/native/trainer.cpp. At this launcher's
#   --eval-simulations 96 (full scale, not the tiny CI fixture budget), the canary is NOT free -
#   budget its per-eval-interval wall-clock cost into iteration timing expectations.
#
# Usage (only after explicit sign-off): .\tools\launch\v7-stage1-bootstrap.ps1 -IAcknowledgeLaunchGate
param(
    [switch]$IAcknowledgeLaunchGate
)

# The mandatory gate: this script must not be runnable to completion by accident. A bare
# invocation (no switch) throws before touching Torch, the run-dir, or the trainer at all.
if (-not $IAcknowledgeLaunchGate) {
    throw "v7 Stage 1 bootstrap is STAGE-GATED (docs/experiment-memory/14-v7-from-scratch-design.md). " +
          "Re-run with -IAcknowledgeLaunchGate only after the planner's launch-gate sign-off " +
          "for this specific launch."
}

. "$PSScriptRoot\_env.ps1"
Use-Torch

$runDir = "results/v7-stage1"

if ((Test-Path -LiteralPath $runDir) -and
    (Get-ChildItem -LiteralPath $runDir -Force -ErrorAction Stop | Select-Object -First 1)) {
    throw "Non-empty run dir $runDir already exists - refusing --fresh overwrite. If a re-run is deliberate, archive/remove the old attempt explicitly first."
}

# PROVISIONAL - review at launch-gate time. iterations*train_steps (150*128=19200) derives
# cleanly as the cosine LR horizon for a fresh run even if --lr-schedule-updates were omitted;
# it is passed explicitly anyway for auditability (project discipline: explicit > default),
# not because 150 is a settled campaign length.
$iterations = 150
$trainSteps = 128
$lrScheduleUpdates = $iterations * $trainSteps

$trainerArgs = @(
    "--run-dir", $runDir,
    "--fresh",
    "--iterations", "$iterations",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "120", "--shrink-interval", "4",
    "--channels", "128", "--blocks", "10", "--games", "128", "--simulations", "96",
    "--train-steps", "$trainSteps", "--batch-size", "512", "--replay-capacity", "200000",
    "--learning-rate", "0.0002", "--min-learning-rate", "0.00001",
    "--lr-schedule-updates", "$lrScheduleUpdates",
    "--weight-decay", "0.0001", "--c-puct", "1.5",
    # v7 Stage 0 item 0.1: alpha 0.3 -> 1.5 (fraction unchanged) - AZ-family small-action-space
    # practice; explicit here rather than changed as trainer.cpp's compiled default (project
    # discipline: explicit > default for a deliberate recipe change).
    "--dirichlet-alpha", "1.5", "--dirichlet-fraction", "0.25",
    # v7 Stage 0 item 0.3: linear anneal 1.0 -> 0.25 over steps 0-60, argmax after - replaces
    # the old flat-then-argmax step function (data-diversity collapse after step 30 was
    # measured in idle-streak structure under it).
    "--temperature", "1.0", "--temperature-final", "0.25",
    "--temperature-anneal", "--temperature-steps", "60",
    "--teacher-games", "32", "--teacher-iterations", "20",
    "--eval-interval", "10", "--eval-games", "32", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--promotion-games", "64", "--promotion-simulations", "96", "--promotion-seed-base", "1100001",
    "--promotion-margin", "0", "--random-score-floor", "0.95", "--heuristic-score-floor", "0.55", "--heuristic-regression-margin", "0.03",
    "--mcts-eval-interval", "50", "--mcts-eval-games", "4", "--baseline-mcts-simulations", "256", "--baseline-mcts-depth", "16",
    "--mcts-eval-seed-base", "1300001",
    "--snapshot-interval", "10", "--seed", "1",
    # Reward re-derivation (doc 14 "Reward re-derivation" section): arena-crush win 0.3 -> 0.1
    # is v7.0's ONE deliberate terminal-value change - removing most of the passive-lottery
    # payoff while keeping win>loss ordering. Everything else on this line is kept at its v6
    # setting (selfkill-win unchanged at 0.3; timeout-draw/mutual-death unchanged at -0.5/-0.2 -
    # mutual-death was reconsidered and deliberately kept, not raised toward -1, as a mild
    # aggression tiebreak near the wall).
    "--arena-crush-win-value", "0.1", "--selfkill-win-value", "0.3",
    "--timeout-draw-value", "-0.5", "--mutual-death-value", "-0.2"
    # --league-heuristic-fraction X - deliberately OMITTED, not defaulted-and-forgotten. Stage 1
    # (teacher bootstrap / IL, doc 14 section 3) has not yet designed the actual collection mix
    # (algorithmic-ladder rungs, teacher-game composition, Backplay/Go-Exploit starts); the
    # league-heuristic-fraction knob is the closest EXISTING lever to "how much of collection is
    # non-mirror," but plugging in a number here before that design lands would be exactly the
    # kind of silent, unconsidered default this project's discipline exists to prevent. Un-
    # comment and set explicitly once Stage 1's collection-mix design is settled:
    # "--league-heuristic-fraction", "0.5",
    #
    # --replay-cause-balance-cap X - deliberately OMITTED (stays at the compiled default 0/off).
    # This is the KL-105 Phase 3 replay-reweighting lever; doc 14 section 1 names it explicitly
    # as a v6 lever that "failed because reward/data-side fixes alone were never sufficient in
    # any published Pommerman result either" - re-enabling it is not part of v7.0's recipe.
)

Write-Host "Bootstrapping v7 Stage 1 (fresh, dirichlet-alpha=1.5, temperature-anneal 1.0->0.25/60, arena-crush-win-value=0.1, iterations=$iterations, lr-schedule-updates=$lrScheduleUpdates)..."
& $NativeExe train @trainerArgs
if ($LASTEXITCODE -eq 0) {
    Write-Host "Bootstrap complete."
} else {
    Write-Host "Bootstrap exited with code $LASTEXITCODE - investigate before resuming."
}
