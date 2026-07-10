# MCTS self-play (sudden-death + persistent flame) — generates a fresh v4 replay if needed,
# then plays it back in the polished viewer. Replaces the old v3 "MCTS Causal Win" replay,
# which the current v4 loader rejects. GUI.
param(
    [int]$Seed = 7,
    [switch]$Regen   # force regeneration even if the replay exists
)
. "$PSScriptRoot\_env.ps1"
Ensure-ReplayDir
$replay = Join-Path $ReplayDir "mcts-selfplay-sudden-death.bin"
if ($Regen -or -not (Test-Path -LiteralPath $replay)) {
    Write-Host "Generating MCTS self-play replay (sudden-death + flame), seed $Seed ..."
    & (Get-Headless) --mode battle --agent mcts --enemy mcts `
        --sudden-death-start 120 --episodes 1 --seed $Seed --replay $replay
    if ($LASTEXITCODE -ne 0) { throw "headless replay generation failed ($LASTEXITCODE)" }
}
& (Get-Viz) --replay $replay --fps 3 --render-fps 60
