# Auto-restarting wrapper for the native AlphaZero trainer. Two unexplained abrupt kills
# (no in-app error trace - external termination, not a self-reported crash) during tonight's
# v6-league run motivated this: relaunches on any non-zero exit, always RESUMING (never
# --fresh, so a flaky kill never risks wiping progress) from the run dir's own latest.pt.
# Stops cleanly the moment the trainer itself exits 0 (reached its --iterations target or was
# stopped normally). Checkpoints save after every completed iteration, so a kill loses at most
# the single in-flight iteration.
#
# KL-101 Part D: every launch/restart/crash/give-up event is now also appended durably to
# <run-dir>/watchdog-attempts.jsonl, not just printed to the live console window - an
# unattended overnight run needs this inspectable after the fact even if the window was closed,
# scrolled past, or the whole machine was left unattended. The trainer itself now also tees its
# own stdout to <run-dir>/train-console.log (see ConsoleTee in trainer.cpp), so the two logs
# together cover both "did the process keep dying" and "what did it print before each death."
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

foreach ($unsafeFlag in @("--fresh", "--fork-from")) {
    if ([array]::IndexOf($TrainerArgs, $unsafeFlag) -ge 0) {
        throw "watchdog-train.ps1 refuses $unsafeFlag. Bootstrap/fork exactly once outside the watchdog; retries must be resume-only."
    }
}

$runDirIndex = [array]::IndexOf($TrainerArgs, "--run-dir")
$runDir = if ($runDirIndex -ge 0 -and $runDirIndex + 1 -lt $TrainerArgs.Count) {
    $TrainerArgs[$runDirIndex + 1]
} else {
    throw "watchdog-train.ps1 requires an explicit --run-dir value; refusing to log/retry against the current directory."
}
if (-not (Test-Path $runDir)) { New-Item -ItemType Directory -Path $runDir -Force | Out-Null }
$attemptLog = Join-Path $runDir "watchdog-attempts.jsonl"

function Write-AttemptLog {
    param([hashtable]$Record)
    ($Record | ConvertTo-Json -Compress) | Out-File -FilePath $attemptLog -Append -Encoding utf8
}

$restarts = 0
while ($true) {
    Write-Host "[watchdog] Launching trainer (attempt $($restarts + 1))..."
    Write-AttemptLog @{ event = "launch"; attempt = ($restarts + 1)
        timestamp_utc = (Get-Date -Format "o"); watchdog_pid = $PID }
    & $NativeExe train @TrainerArgs
    $code = $LASTEXITCODE

    if ($code -eq 0) {
        Write-Host "[watchdog] Trainer exited cleanly (code 0) - reached target or stopped normally. Done."
        Write-AttemptLog @{ event = "exit_clean"; attempt = ($restarts + 1)
            timestamp_utc = (Get-Date -Format "o") }
        break
    }

    $restarts++
    Write-Host "[watchdog] Trainer exited with code $code (crash/kill #$restarts of $MaxRestarts). Checkpoint is safe - resuming, not fresh."
    Write-AttemptLog @{ event = "crash"; attempt = $restarts; exit_code = $code
        timestamp_utc = (Get-Date -Format "o") }
    if ($restarts -ge $MaxRestarts) {
        Write-Host "[watchdog] Max restarts ($MaxRestarts) reached - giving up. Investigate manually before relaunching."
        Write-AttemptLog @{ event = "giving_up"; max_restarts = $MaxRestarts
            timestamp_utc = (Get-Date -Format "o") }
        exit 1
    }
    Write-Host "[watchdog] Restarting in ${RestartDelaySeconds}s..."
    Start-Sleep -Seconds $RestartDelaySeconds
}
