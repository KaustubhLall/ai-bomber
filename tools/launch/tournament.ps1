# Headless round-robin tournament among scripted/search agents (role-balanced, sudden-death).
# Prints a W-D-L matrix + score-ranked standings. Console — run with -NoExit to keep it open.
param(
    [string]$Agents = "mcts,heuristic,greedy,alpha-beta",
    [int]$Episodes = 12,
    [int]$Seed = 20260709
)
. "$PSScriptRoot\_env.ps1"
Write-Host "Starting round-robin: $Agents ($Episodes games/matchup)..."
Write-Host "The binary now prints a line per matchup as it plays (was previously silent until the end)."
& (Get-Headless) --mode battle --tournament $Agents --episodes $Episodes `
    --sudden-death-start 120 --seed $Seed
