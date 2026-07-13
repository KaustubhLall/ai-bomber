# The ONE measurement that gets to decide whether v6-league is working: eval-time
# (dirichlet noise OFF, greedy argmax action selection - see evaluate_baseline()/
# marginal_action() in src/training/native/trainer.cpp) win-cause + WAIT% against the
# native MCTS baseline. Everything else printed during training (vs-heuristic score,
# training-time win-cause under exploration noise) is a training-health signal only -
# it is exactly the class of measurement that produced the retracted v5 "superhuman"
# claim (see docs/experiment-memory/07-grokking-campaign.md Part 2, and the KL-96
# Linear issue "Update 2026-07-10" section). Do not let a vs-heuristic or
# training-time number substitute for this.
#
# Uses the standing MCTS-eval seed block (1300001) at N=32 (vs the in-loop N=4) -
# this is a mid-course gate check, NOT the final claim, so it deliberately does not
# touch the reserved final-holdout blocks (A3 = 2400001/2410001, B3 = 2500001/2510001).
# See tools/launch/README.md "Seed hygiene" for why those must stay untouched until a
# checkpoint has actually cleared this gate.
#
# Usage:
#   .\tools\launch\v6-league-gate-eval.ps1                    # v6-league, checkpoint=latest.pt, N=32
#   .\tools\launch\v6-league-gate-eval.ps1 -Checkpoint iteration_000100.pt
#   .\tools\launch\v6-league-gate-eval.ps1 -RunDir results/alphazero-native-superhuman-v6-league-crush01
#   .\tools\launch\v6-league-gate-eval.ps1 -MctsGames 64      # tighter Wilson LCB
#   .\tools\launch\v6-league-gate-eval.ps1 -SuddenDeathOff    # KL-108 diagnostic control
#   .\tools\launch\v6-league-gate-eval.ps1 -SuddenDeathStart 160 # native lever-2 semantics
#
# -Checkpoint is resolved by the native trainer relative to --run-dir (same
# convention as Resolve-Champion in _env.ps1, which returns a bare filename,
# not a joined path) - passing a path that already includes the run-dir
# prefix doubles it and fails with "checkpoint not found". This script
# strips any leading path so either form works.
#
# -SuddenDeathOff passes --sudden-death-start 0 EXPLICITLY, which differs from every
# checkpoint's trained value (120) - KL-101's semantic-fork logging will correctly flag
# this as a deliberate diagnostic override (semantic-fork-log.jsonl), not a silent
# substitution. This is intentional: SD-off is the discriminating control that tells
# real combat skill apart from "survives to the crush window" - see KL-96/KL-108.
#
# -LegacyArenaCrushWinValue X: required for any checkpoint saved BEFORE KL-101's
# semantic-manifest fix (e.g. crush01's iteration_000130.pt, stopped before that commit
# existed) - such a checkpoint has no manifest to inherit from and fails closed
# otherwise. Passes --legacy-accept-unverified-semantics plus an explicit
# --arena-crush-win-value X (X = whatever that checkpoint was actually trained under -
# check its launcher script, do not guess). Checkpoints created after KL-101 (e.g.
# control03) don't need this - their manifest is authoritative.
#
# -TraceOutput PATH: KL-107 per-step neural decision trace (see trainer.cpp
# evaluate_baseline). Optional - files get large at N=32 (thousands of rows); pass
# explicitly only for the runs you intend to actually inspect.
param(
    [string]$RunDir = "results/alphazero-native-superhuman-v6-league",
    [string]$Checkpoint = "latest.pt",
    [int]$MctsGames = 32,
    [int]$MctsSims = 256,
    [string]$Output = "",
    [string]$PerMatchOutput = "",
    [string]$TraceOutput = "",
    [switch]$SuddenDeathOff,
    [int]$SuddenDeathStart = -1,
    [string]$LegacyArenaCrushWinValue = "",
    [switch]$OverwriteEvidence
)
. "$PSScriptRoot\_env.ps1"
Use-Torch
$Checkpoint = Split-Path -Leaf $Checkpoint
$suffix = if ($SuddenDeathOff) { "-sdoff" } else { "" }
$checkpointStem = [System.IO.Path]::GetFileNameWithoutExtension($Checkpoint)
$resolvedSuddenDeathStart = if ($SuddenDeathOff) { 0 } elseif ($SuddenDeathStart -ge 0) { $SuddenDeathStart } else { 120 }
$evidenceTag = "$checkpointStem-sd$resolvedSuddenDeathStart"
if ($Output -eq "") { $Output = Join-Path $RunDir "gate-eval-$evidenceTag.json" }
if ($PerMatchOutput -eq "") { $PerMatchOutput = Join-Path $RunDir "gate-eval-$evidenceTag-per-match.jsonl" }

foreach ($path in @($Output, $PerMatchOutput, $TraceOutput)) {
    if ($path -ne "" -and (Test-Path -LiteralPath $path) -and -not $OverwriteEvidence) {
        throw "Refusing to overwrite evaluation evidence: $path. Choose a unique path or pass -OverwriteEvidence explicitly."
    }
}

Write-Host "Gate eval: $RunDir/$Checkpoint vs MCTS-$MctsSims, N=$MctsGames games, noise OFF, greedy action selection, sudden-death-start=$resolvedSuddenDeathStart."
$evalArgs = @(
    "evaluate", "--run-dir", $RunDir, "--checkpoint", $Checkpoint,
    "--channels", "128", "--blocks", "10",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "$resolvedSuddenDeathStart", "--shrink-interval", "4",
    "--eval-games", "32", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--eval-mcts", "--mcts-eval-games", "$MctsGames",
    "--baseline-mcts-simulations", "$MctsSims", "--baseline-mcts-depth", "16",
    "--mcts-eval-seed-base", "1300001",
    "--output", $Output,
    "--per-match-output", $PerMatchOutput
)
if ($LegacyArenaCrushWinValue -ne "") {
    Write-Host "Legacy checkpoint: forcing --arena-crush-win-value $LegacyArenaCrushWinValue explicitly (--legacy-accept-unverified-semantics)."
    $evalArgs += @("--legacy-accept-unverified-semantics", "--arena-crush-win-value", $LegacyArenaCrushWinValue)
}
if ($TraceOutput -ne "") { $evalArgs += @("--trace-output", $TraceOutput) }
if ($OverwriteEvidence) { $evalArgs += "--overwrite-evidence" }
& $NativeExe @evalArgs
Write-Host "`nRead the 'mcts' win-cause + WAIT% lines above (or $Output; per-match rows in $PerMatchOutput). Compare against the KL-96/KL-108 decision tree before drawing any conclusion."
