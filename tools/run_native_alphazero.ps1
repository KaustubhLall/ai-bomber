param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TrainerArgs
)

$root = Split-Path -Parent $PSScriptRoot
$torchLib = Join-Path $root ".venv-gpu\Lib\site-packages\torch\lib"
$executable = Join-Path $root "build-native-gpu\src\Release\bomber_alphazero_native.exe"

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Native trainer not built: $executable"
}
if (-not (Test-Path -LiteralPath $torchLib)) {
    throw "LibTorch runtime not found: $torchLib"
}

$env:PATH = "$torchLib;$env:PATH"
& $executable @TrainerArgs
exit $LASTEXITCODE
