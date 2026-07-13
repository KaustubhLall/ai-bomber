# KL-98 reserve lever 2: --sudden-death-start 120 -> 160 (more combat runway before the
# arena starts forcing a decision). Forked from control03's iteration-130 checkpoint (the
# STRONGER unmodified baseline, not crush01) via --fork-from - one variable at a time means
# lever 1's reward change should not carry forward into lever 2.
#
# Launched with CALIBRATED LOW CONFIDENCE: a KL-107 trace analysis of both crush01-130 and
# control03-130's SD-on games found the final chosen action agrees with the RAW network
# policy's own argmax 74-93% of the time in both - meaning search is mostly agreeing with an
# already-passive policy, not overriding an aggressive one into passivity. Neither lever 1
# nor lever 2 touches the policy's own training signal, so lever 2 is expected to plausibly
# fail for the same underlying reason lever 1 did. Run anyway since it's next in the
# predeclared order and costs nothing new to verify honestly - see KL-98 for full reasoning.
#
# Run ONCE, directly (NOT under the watchdog) - one-time --fresh --fork-from bootstrap.
# Runs to iteration 131 (one past the fork point) and stops. Use lever2-sdstart160-resume.ps1
# (watchdog-wrapped, ordinary resume) to continue to iteration 160+ from there.
#
# Usage: .\tools\launch\lever2-sdstart160-bootstrap.ps1
. "$PSScriptRoot\_env.ps1"
Use-Torch

$parentCheckpoint = "results/alphazero-native-superhuman-control03-from102/iteration_000130.pt"
if (-not (Test-Path $parentCheckpoint)) {
    throw "Parent checkpoint not found: $parentCheckpoint"
}

$trainerArgs = @(
    "--run-dir", "results/alphazero-native-superhuman-lever2-sdstart160",
    "--fresh", "--fork-from", $parentCheckpoint,
    "--iterations", "131",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "160", "--shrink-interval", "4",
    "--channels", "128", "--blocks", "10", "--games", "128", "--simulations", "96",
    "--train-steps", "128", "--batch-size", "512", "--replay-capacity", "200000",
    "--learning-rate", "0.0002", "--min-learning-rate", "0.00001",
    "--weight-decay", "0.0001", "--c-puct", "1.5", "--dirichlet-alpha", "0.3", "--dirichlet-fraction", "0.25",
    "--temperature", "1", "--temperature-steps", "30",
    "--teacher-games", "32", "--teacher-iterations", "20",
    "--eval-interval", "10", "--eval-games", "32", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--promotion-games", "64", "--promotion-simulations", "96", "--promotion-seed-base", "1100001",
    "--promotion-margin", "0", "--random-score-floor", "0.95", "--heuristic-score-floor", "0.55", "--heuristic-regression-margin", "0.03",
    "--mcts-eval-interval", "50", "--mcts-eval-games", "4", "--baseline-mcts-simulations", "256", "--baseline-mcts-depth", "16",
    "--mcts-eval-seed-base", "1300001",
    "--snapshot-interval", "10", "--seed", "1",
    "--timeout-draw-value", "-0.5", "--mutual-death-value", "-0.2",
    "--arena-crush-win-value", "0.3", "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5"
    # arena-crush-win-value stays 0.3 (control03's own value, explicit for auditability) -
    # only sudden-death-start changes. Not a legacy checkpoint (control03 postdates KL-101),
    # no --legacy-accept-unverified-semantics needed.
)

Write-Host "Bootstrapping lever2-sdstart160 (fork from $parentCheckpoint, sudden-death-start 120->160)..."
& $NativeExe train @trainerArgs
if ($LASTEXITCODE -eq 0) {
    Write-Host "Bootstrap complete. Run .\tools\launch\lever2-sdstart160-resume.ps1 to continue to iteration 160+."
} else {
    Write-Host "Bootstrap exited with code $LASTEXITCODE - check output above before running lever2-sdstart160-resume.ps1."
}
