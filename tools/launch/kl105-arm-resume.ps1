# KL-105 Phase 3 arm resume (doc 13 section 3): continue a bootstrapped arm to iteration 154
# under the watchdog. Resume-only (no --fresh/--fork-from - the watchdog refuses those anyway).
# --lr-schedule-updates and --replay-cause-balance-cap are re-passed explicitly with the SAME
# values the bootstrap forked to: they now match the arm's own manifest, so no new semantic
# fork is logged - they are here so the values actually in force are loud in every attempt's
# argv rather than invisible inheritance.
#
# Usage: .\tools\launch\kl105-arm-resume.ps1 -Arm treatment
#        .\tools\launch\kl105-arm-resume.ps1 -Arm control
param(
    [Parameter(Mandatory = $true)][ValidateSet("treatment", "control")][string]$Arm,
    [int]$Iterations = 154
)
. "$PSScriptRoot\_env.ps1"

$runDir = "results/kl105-arm-$Arm-from-control03-130"
$cap = if ($Arm -eq "treatment") { "0.25" } else { "0" }
if (-not (Test-Path (Join-Path $runDir "latest.pt"))) {
    throw "$runDir has no latest.pt - run kl105-arm-bootstrap.ps1 -Arm $Arm first."
}

$trainerArgs = @(
    "--run-dir", $runDir,
    "--iterations", "$Iterations",
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
)

& "$PSScriptRoot\watchdog-train.ps1" -TrainerArgs $trainerArgs
