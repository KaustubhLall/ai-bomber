# KL-105 Phase 3 paired evaluation (doc 13 section 3): the full "after" battery for one arm at
# its final iteration - tactical gates, SD-on diagnostic (trace + per-match, 32 MCTS-256
# matches on the standing 1300001 diagnostic block), and the SD-off control. Run once per arm,
# sequentially (single-native-GPU-process rule). Outputs are write-once evidence; the aggregate
# JSONs' resolved_semantics now carry replay_cause_balance_cap, so treatment/control identity
# is readable straight off each evidence file.
#
# SD-off passes --sudden-death-start 0 as an explicit evaluate-time override of the arm's
# manifest (expect the loud SEMANTIC FORK eval-override line - same pattern as the historical
# lever2 SD120-uniform run; deliberate, not a bug).
#
# Usage: .\tools\launch\kl105-paired-eval.ps1 -Arm treatment
#        .\tools\launch\kl105-paired-eval.ps1 -Arm control
param(
    [Parameter(Mandatory = $true)][ValidateSet("treatment", "control")][string]$Arm,
    [int]$Iteration = 154,
    # The arms end at --iterations 154 with latest.pt as the only iteration-154 artifact
    # (iteration_XXXXXX.pt snapshots land on the snapshot-interval grid: 140, 150). latest.pt
    # IS the 154 checkpoint; every evidence file this script produces embeds
    # checkpoint_iteration, which the post-run verification checks against -Iteration - a
    # wrong checkpoint fails loudly there rather than silently evaluating the wrong weights.
    [string]$Checkpoint = "latest.pt",
    [string]$OutDir = "results/kl105-arm-eval-2026-07-12"
)
. "$PSScriptRoot\_env.ps1"
Use-Torch

$runDir = "results/kl105-arm-$Arm-from-control03-130"
$checkpoint = $Checkpoint
if (-not (Test-Path (Join-Path $runDir $checkpoint))) {
    throw "$runDir/$checkpoint not found - has the arm finished training?"
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
$tag = "$Arm-iter$Iteration"

$commonEval = @(
    "--run-dir", $runDir, "--checkpoint", $checkpoint,
    "--channels", "128", "--blocks", "10", "--width", "13", "--height", "11",
    "--max-steps", "200", "--crate-density", "50", "--flame-duration", "2",
    "--shrink-interval", "4",
    "--eval-games", "2", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--eval-mcts", "--mcts-eval-games", "32", "--mcts-eval-seed-base", "1300001",
    "--baseline-mcts-simulations", "256", "--baseline-mcts-depth", "16",
    "--no-progress"
)

# Parameter deliberately NOT named $args: that is PowerShell's AUTOMATIC unbound-arguments
# variable inside every function, and naming a parameter after it makes `@args` splat the
# (empty) automatic array instead - the exe then runs with NO arguments, prints help, and
# exits 0, which sails straight through the exit-code check. Exactly this happened on this
# script's first run; the per-step output-file existence check below is the belt-and-braces
# guard against any future silently-successful no-op of the same shape.
function Invoke-Step($label, $exeArgs, $expectedOutput) {
    Write-Host "=== [$([DateTime]::Now.ToString('HH:mm:ss'))] $label ==="
    & $NativeExe @exeArgs
    if ($LASTEXITCODE -ne 0) { throw "$label FAILED with exit code $LASTEXITCODE" }
    if ($expectedOutput -and -not (Test-Path $expectedOutput)) {
        throw "$label exited 0 but produced no $expectedOutput - the invocation was a no-op"
    }
}

Invoke-Step "$tag gates (96 sims)" @(
    "gates", "--run-dir", $runDir, "--checkpoint", $checkpoint,
    "--channels", "128", "--blocks", "10", "--width", "13", "--height", "11",
    "--max-steps", "200", "--crate-density", "50", "--flame-duration", "2",
    "--sudden-death-start", "120", "--shrink-interval", "4",
    "--eval-simulations", "96", "--no-progress",
    "--output", "$OutDir/$tag-gates.json") "$OutDir/$tag-gates.json"

Invoke-Step "$tag SD-on diagnostic (trace + per-match)" (
    @("evaluate") + $commonEval + @(
    "--sudden-death-start", "120",
    "--trace-output", "$OutDir/$tag-SDon-trace.jsonl",
    "--per-match-output", "$OutDir/$tag-SDon-permatch.jsonl",
    "--output", "$OutDir/$tag-SDon-agg.json")) "$OutDir/$tag-SDon-agg.json"

Invoke-Step "$tag SD-off control" (
    @("evaluate") + $commonEval + @(
    "--sudden-death-start", "0",
    "--trace-output", "$OutDir/$tag-SDoff-trace.jsonl",
    "--per-match-output", "$OutDir/$tag-SDoff-permatch.jsonl",
    "--output", "$OutDir/$tag-SDoff-agg.json")) "$OutDir/$tag-SDoff-agg.json"

Write-Host "=== $tag paired-eval battery complete -> $OutDir ==="
