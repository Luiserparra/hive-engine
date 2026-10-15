$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

cmake --preset release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --preset release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$OutDir = Join-Path $RepoRoot "build\release\bench-results"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutJson = Join-Path $OutDir "hive_bench.json"

$Bench = Join-Path $RepoRoot "build\release\bench\hive_bench.exe"
if (-not (Test-Path $Bench)) {
    $Bench = Join-Path $RepoRoot "build\release\bench\hive_bench"
}
if (-not (Test-Path $Bench)) {
    Write-Error "hive_bench not found under build/release/bench/"
    exit 1
}

& $Bench --benchmark_out=$OutJson --benchmark_out_format=json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Demo = Join-Path $RepoRoot "build\release\hive_engine.exe"
if (-not (Test-Path $Demo)) {
    $Demo = Join-Path $RepoRoot "build\release\hive_engine"
}

Write-Host "Demo: $Demo"
Write-Host "Benchmark JSON: $OutJson"
