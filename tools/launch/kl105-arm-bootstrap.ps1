# KL-105 Phase 3 arm bootstrap (doc 13 section 3). Forks BOTH experiment arms from the SAME
# base checkpoint - control03-from102's iteration_000130.pt (SHA-256
# ab5771d34f3be18e0911b1da51690fa62dc75f5ec6d0a06dba082cdc069cb288) - with an IDENTICAL
# explicit --lr-schedule-updates 33280 override (both arms would otherwise inherit the parent's
# 13,184-update horizon, already fully consumed at global_updates=16,640, i.e. floor LR 1e-5
# for the whole arm - a mute test at a learning rate chosen by an old bug, not a fair one; at
# 33,280 the cosine resumes at ~1.05e-4 annealing to ~6e-5 over the arm, mirroring the LR range
# crush01 actually trained under). The ONLY difference between the arms is
# --replay-cause-balance-cap: 0.25 (treatment) vs 0 (control, byte-for-byte the pre-existing
# uniform sampler). Both overrides are passed explicitly so they are loud in argv and land in
# semantic-fork-log.jsonl via the manifest machinery, not silently defaulted.
#
# Run ONCE per arm, directly (NOT under the watchdog) - one-time --fresh --fork-from to
# establish fork provenance (fork-manifest.json) and produce one real checkpoint at iteration
# 131. Then continue with kl105-arm-resume.ps1 (watchdog-wrapped, resume-only) to 154.
# Single-native-GPU-process rule applies: never run while any train/evaluate/gates is active.
#
# Usage: .\tools\launch\kl105-arm-bootstrap.ps1 -Arm treatment
#        .\tools\launch\kl105-arm-bootstrap.ps1 -Arm control
param(
    [Parameter(Mandatory = $true)][ValidateSet("treatment", "control")][string]$Arm
)
. "$PSScriptRoot\_env.ps1"
Use-Torch

$parentCheckpoint = "results/alphazero-native-superhuman-control03-from102/iteration_000130.pt"
$runDir = "results/kl105-arm-$Arm-from-control03-130"
$cap = if ($Arm -eq "treatment") { "0.25" } else { "0" }

if (-not (Test-Path $parentCheckpoint)) {
    throw "Fork parent not found: $parentCheckpoint"
}
if ((Test-Path -LiteralPath $runDir) -and
    (Get-ChildItem -LiteralPath $runDir -Force -ErrorAction Stop | Select-Object -First 1)) {
    throw "Non-empty run dir $runDir already exists - refusing --fresh overwrite. If a re-run is deliberate, archive/remove the old arm explicitly first."
}

$trainerArgs = @(
    "--run-dir", $runDir,
    "--fresh", "--fork-from", $parentCheckpoint,
    # control03's own best.pt is historically inconsistent (v6-league iteration-102 weights
    # relabeled as its iteration-10 champion - the pre-KL-101 bug, caught in the wild by the
    # fork validation when the first bootstrap attempt failed closed). The arms deliberately
    # start their champion history at their own fork point instead of inheriting or repairing
    # the parent's (its run dir is retained evidence, not to be mutated). Champion state never
    # feeds training in this trainer, and both arms get the identical reset.
    "--fork-reset-champion",
    "--iterations", "131",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "120", "--shrink-interval", "4",
    "--channels", "128", "--blocks", "10", "--games", "128", "--simulations", "96",
    "--train-steps", "128", "--batch-size", "512", "--replay-capacity", "200000",
    "--learning-rate", "0.0002", "--min-learning-rate", "0.00001",
    "--lr-schedule-updates", "33280",
    "--replay-cause-balance-cap", $cap,
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
    "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5"
    # Deliberately NOT --arena-crush-win-value: inherited from the parent's manifest (0.3).
    # No --legacy-accept-unverified-semantics: the parent postdates KL-101 and has a manifest;
    # if this fork ever fails closed on semantics, that is a real problem to investigate, not
    # a flag to add.
)

Write-Host "Bootstrapping KL-105 $Arm arm (fork from control03-130, cap=$cap, horizon=33280)..."
& $NativeExe train @trainerArgs
if ($LASTEXITCODE -eq 0) {
    Write-Host "Bootstrap complete. Continue with: .\tools\launch\kl105-arm-resume.ps1 -Arm $Arm"
} else {
    Write-Host "Bootstrap exited with code $LASTEXITCODE - investigate before resuming."
}
