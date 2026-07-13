# RETRACTED 2026-07-11: this script evaluates the v5 iter-210 checkpoint, whose
# "agent-ladder superhuman" claim was retracted (95-97% of its wins were arena-crush
# deaths, not bomb-kills - see docs/SUPERHUMAN_ALPHAZERO.md and Linear KL-100). Kept
# for historical inspection of that checkpoint only - do NOT read a strong score from
# this script as current evidence of anything. The active work is v6-league/crush01/
# control03-from102 (see docs/experiment-memory/07-grokking-campaign.md Part 2 and
# 10-autonomous-brick-execution.md); v6-league-gate-eval.ps1 is the current equivalent
# for those checkpoints.
#
# Run the native evaluate ladder on the champion (v5 iter-210): W-D-L vs random, heuristic,
# and (optional) MCTS, with Wilson lower bounds. Console — run with -NoExit to keep it open.
#
# NOTE: defaults use the SELECTION seed block (900001 / MCTS 1300001), which is fine for a
# quick "how strong is the champion" check. Do NOT point --SeedBase at a final holdout block
# (2400001/2410001 = A3, 2500001/2510001 = B3) — starting an eval there retires it for proof.
param(
    [int]$Games = 32,
    [int]$Simulations = 96,
    [switch]$Mcts,
    [int]$MctsGames = 16,
    [int]$MctsSims = 512,
    [string]$SeedBase = ""   # empty = built-in selection default (900001)
)
. "$PSScriptRoot\_env.ps1"
Use-Torch
$ckpt = Resolve-Champion
Write-Host "RETRACTED CHECKPOINT (v5 iter-210) - see docs/SUPERHUMAN_ALPHAZERO.md and KL-100. Historical inspection only." -ForegroundColor Yellow
Write-Host "Loading champion checkpoint ($ckpt, ~870MB — first load can take ~20-30s)..."
Write-Host "This window will show a live progress bar per phase; it is NOT hung while that bar is moving."
if ($Mcts) {
    Write-Host "MCTS-$MctsSims baseline is the slow phase (~tens of seconds/game) - budget several minutes for $MctsGames games."
}
$evalArgs = @(
    "evaluate", "--run-dir", $ChampionRunDir, "--checkpoint", $ckpt,
    "--channels", "128", "--blocks", "10",
    "--eval-games", $Games, "--eval-simulations", $Simulations
)
if ($SeedBase -ne "") { $evalArgs += @("--eval-seed-base", $SeedBase) }
if ($Mcts) {
    $evalArgs += @("--eval-mcts", "--mcts-eval-games", $MctsGames,
                   "--baseline-mcts-simulations", $MctsSims, "--baseline-mcts-depth", "16")
}
& $NativeExe @evalArgs
