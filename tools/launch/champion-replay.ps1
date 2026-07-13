# RETRACTED 2026-07-11: plays back the v5 iter-210 checkpoint, whose "agent-ladder
# superhuman" claim was retracted (95-97% of its wins were arena-crush deaths, not
# bomb-kills - see docs/SUPERHUMAN_ALPHAZERO.md and Linear KL-100). What you'll see is
# mostly the arena closing in around a passive agent, not tactical play - kept for
# historical/diagnostic viewing of exactly that failure mode, not as a demo of strength.
#
# Watch the trained champion (v5 iter-210) play. The neural net lives in the LibTorch
# trainer, so this exports a v4 replay via `evaluate --replay-out`, then plays it in the
# polished viewer. First run loads the ~868 MB checkpoint (~30 s); the replay is cached. GUI.
param(
    [ValidateSet("heuristic", "mcts")][string]$Opponent = "heuristic",
    [int]$Seed = 777,
    [int]$Simulations = 96,
    [switch]$Regen
)
. "$PSScriptRoot\_env.ps1"
Ensure-ReplayDir
$replay = Join-Path $ReplayDir "champion-best210-vs-$Opponent.bin"
if ($Regen -or -not (Test-Path -LiteralPath $replay)) {
    Use-Torch
    $ckpt = Resolve-Champion
    Write-Host "Generating champion ($ckpt) vs $Opponent replay (seed $Seed, $Simulations sims) ..."
    $mctsFlag = @()
    if ($Opponent -eq "mcts") { $mctsFlag = @("--eval-mcts") }
    & $NativeExe evaluate --run-dir $ChampionRunDir --checkpoint $ckpt `
        --channels 128 --blocks 10 `
        --eval-seed-base $Seed --eval-games 1 --eval-simulations $Simulations `
        @mctsFlag --replay-out $replay --no-progress
    if ($LASTEXITCODE -ne 0) { throw "champion replay export failed ($LASTEXITCODE)" }
}
& (Get-Viz) --replay $replay --fps 3 --render-fps 60
