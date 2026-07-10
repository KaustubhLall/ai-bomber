# Auto-restarting wrapper for the native AlphaZero trainer. Two unexplained abrupt kills
# (no in-app error trace - external termination, not a self-reported crash) during tonight's
# v6-league run motivated this: relaunches on any non-zero exit, always RESUMING (never
# --fresh, so a flaky kill never risks wiping progress) from the run dir's own latest.pt.
# Stops cleanly the moment the trainer itself exits 0 (reached its --iterations target or was
# stopped normally). Checkpoints save after every completed iteration, so a kill loses at most
# the single in-flight iteration.
#
# Usage: pass the FULL trainer argument list (everything you'd normally give
# tools/run_native_alphazero.ps1, including --run-dir/--iterations) as one explicit array to
# -TrainerArgs. A single array parameter sidesteps PowerShell's ambiguous parsing of
# double-dash flags mixed with named params/ValueFromRemainingArguments (the first version of
# this script hit exactly that: "--width" failed to bind to -Iterations's [int] type).
#
# Example:
#   .\tools\launch\watchdog-train.ps1 -TrainerArgs @(
#       "--run-dir", "results/my-run", "--iterations", "200", "--channels", "128", "--blocks", "10")
param(
    [Parameter(Mandatory = $true)][string[]]$TrainerArgs,
    [int]$MaxRestarts = 20,
    [int]$RestartDelaySeconds = 10
)
. "$PSScriptRoot\_env.ps1"
Use-Torch

$restarts = 0
while ($true) {
    Write-Host "[watchdog] Launching trainer (attempt $($restarts + 1))..."
    & $NativeExe train @TrainerArgs
    $code = $LASTEXITCODE

    if ($code -eq 0) {
        Write-Host "[watchdog] Trainer exited cleanly (code 0) - reached target or stopped normally. Done."
        break
    }

    $restarts++
    Write-Host "[watchdog] Trainer exited with code $code (crash/kill #$restarts of $MaxRestarts). Checkpoint is safe - resuming, not fresh."
    if ($restarts -ge $MaxRestarts) {
        Write-Host "[watchdog] Max restarts ($MaxRestarts) reached - giving up. Investigate manually before relaunching."
        exit 1
    }
    Write-Host "[watchdog] Restarting in ${RestartDelaySeconds}s..."
    Start-Sleep -Seconds $RestartDelaySeconds
}
