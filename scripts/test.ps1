$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")

# Ensure build is up to date
python (Join-Path $PSScriptRoot "build.py")

# Run C++ Unit Tests
$UnitTestsExe = Join-Path $Root "build\neuroparse_unit_tests.exe"
if (Test-Path $UnitTestsExe) {
  & $UnitTestsExe
}

# Run Python Tests
python -m pytest (Join-Path $Root "tests\python") -q
