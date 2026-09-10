# Host-side unit tests (Windows). Needs a C++17 compiler: set CXX to g++ or
# clang++, or have zig in PATH. Usage: powershell -File test/run.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$cxx = $env:CXX
if (-not $cxx) {
  foreach ($candidate in @('g++', 'clang++', 'zig')) {
    if (Get-Command $candidate -ErrorAction SilentlyContinue) { $cxx = $candidate; break }
  }
}
if (-not $cxx) {
  Write-Error "No C++ compiler found. Install g++/clang++ or zig, or set `$env:CXX."
  exit 1
}

$out = Join-Path $env:TEMP ("host_tests_" + [guid]::NewGuid().ToString('N') + ".exe")
$sources = @(Get-ChildItem test -Filter *.cpp | ForEach-Object { $_.FullName })

$cxxName = [System.IO.Path]::GetFileNameWithoutExtension($cxx)
if ($cxxName -eq 'zig') {
  & $cxx c++ -w -std=gnu++17 -Itest/shim -I. @sources -o $out
} else {
  & $cxx -std=gnu++17 -Wall -Wextra -Itest/shim -I. @sources -o $out
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $out
exit $LASTEXITCODE
