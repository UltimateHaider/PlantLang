# PlantLang — Windows (MSYS2) build wrapper.
#
# The toolchain and test runners are POSIX-oriented, so builds must run
# inside MSYS2. This script locates an MSYS2 installation, maps the repo
# path to its POSIX form, and forwards the requested make targets.
#
# Usage (from PowerShell):
#   .\scripts\build-windows.ps1              # make all
#   .\scripts\build-windows.ps1 self         # make self
#   .\scripts\build-windows.ps1 test         # make test
#   .\scripts\build-windows.ps1 all self test
#
# Override the MSYS2 root when it is not installed at C:\msys64:
#   .\scripts\build-windows.ps1 -MsysRoot D:\msys64 test

param(
    [string]$MsysRoot = "C:\msys64",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Targets = @("all")
)

$ErrorActionPreference = "Stop"

$bash = Join-Path $MsysRoot "usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    throw "MSYS2 bash not found at '$bash'. Install MSYS2 (https://www.msys2.org/) or pass -MsysRoot <path>."
}

$repo = Split-Path -Parent $PSScriptRoot
# Convert D:\PlantLang\PlantLang -> /d/PlantLang/PlantLang for MSYS2.
$posix = "/" + $repo.Substring(0, 1).ToLower() + $repo.Substring(2).Replace("\", "/")

$makeTargets = ($Targets -join " ")
Write-Host "MSYS2: $MsysRoot"
Write-Host "Repo : $posix"
Write-Host "Make : $makeTargets"

& $bash -lc "cd '$posix' && make $makeTargets"
exit $LASTEXITCODE
