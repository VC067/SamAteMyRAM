$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$DevKit = Join-Path $Root "tools\w64devkit\bin"

if (Test-Path (Join-Path $DevKit "g++.exe")) {
    $env:PATH = "$DevKit;$env:PATH"
}

$BuildDir = Join-Path $Root "build"
cmake -S $Root -B $BuildDir -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Built = Join-Path $BuildDir "Altman.exe"
$Dest = Join-Path $Root "Altman.exe"
Copy-Item $Built $Dest -Force
Write-Host "Built $Dest"
