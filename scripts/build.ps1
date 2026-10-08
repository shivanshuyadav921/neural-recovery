param(
  [string]$BuildType = "Release"
)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")

python (Join-Path $PSScriptRoot "build.py")
