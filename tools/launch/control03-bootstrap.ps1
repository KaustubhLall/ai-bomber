# Brick 2 (KL-108/KL-98) matched control: forks from the SAME v6-league iteration-102
# checkpoint crush01 forked from, changing NOTHING semantically (arena-crush-win-value stays
# 0.3, matching what this parent was actually trained under its entire history). This is what
# v6-league would have kept doing if the reward lever had never been touched - the missing
# matched control the crush01 gate-eval numbers were never actually compared against
# (comparing crush01-130 to parent-100 directly would be confounded by 28 iterations of
# general training, replay turnover, and LR phase, none of which is the lever under test).
#
# NOTE: this parent checkpoint predates KL-101's semantic-manifest fix (it was saved before
# that commit), so it has no manifest to inherit from and correctly fails closed without
# --legacy-accept-unverified-semantics - this is the fail-closed behavior working as intended,
# not a bug to route around silently. --arena-crush-win-value 0.3 is passed EXPLICITLY here
# (not left to the struct default, even though they're numerically the same) so the value
# actually used is loud and auditable rather than a coincidental default match.
#
# Run ONCE, directly (NOT under the watchdog) - this is a one-time --fresh --fork-from bootstrap
# to establish the fork's provenance (fork-manifest.json) and produce one real checkpoint.
# Runs to iteration 103 (one iteration past the fork point) and stops. From there, use
# control03-resume.ps1 (watchdog-wrapped, ordinary resume - no --fresh, no --fork-from, which
# would re-fork and discard progress on every watchdog restart) to continue to iteration 130.
#
# Usage: .\tools\launch\control03-bootstrap.ps1
. "$PSScriptRoot\_env.ps1"
Use-Torch

$parentCheckpoint = "results/alphazero-native-superhuman-v6-league/latest.pt"
if (-not (Test-Path $parentCheckpoint)) {
    throw "Parent checkpoint not found: $parentCheckpoint - v6-league's iteration-102 latest.pt must exist to fork from."
}

$trainerArgs = @(
    "--run-dir", "results/alphazero-native-superhuman-control03-from102",
    "--fresh", "--fork-from", $parentCheckpoint,
    "--iterations", "103",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "120", "--shrink-interval", "4",
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
    "--arena-crush-win-value", "0.3", "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5",
    "--legacy-accept-unverified-semantics"
)

Write-Host "Bootstrapping control03-from102 (fork from $parentCheckpoint, arena-crush-win-value=0.3 explicit)..."
& $NativeExe train @trainerArgs
if ($LASTEXITCODE -eq 0) {
    Write-Host "Bootstrap complete. Run .\tools\launch\control03-resume.ps1 to continue to iteration 130+."
} else {
    Write-Host "Bootstrap exited with code $LASTEXITCODE - check output above before running control03-resume.ps1."
}
