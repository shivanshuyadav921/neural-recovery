# NeuroParse Phase 18 - PowerShell Reproducibility Entrypoint
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $Root

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  NeuroParse Phase 18 — PowerShell Reproducibility Orchestrator       " -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

python scripts/reproduce.py $args
if ($LASTEXITCODE -ne 0) {
    Write-Error "Reproducibility verification failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
