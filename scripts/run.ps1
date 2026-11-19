<#
.SYNOPSIS
    Builds CAN Analyzer (release), deploys the Qt runtime next to the exe, and starts it.

.DESCRIPTION
    1. Picks the CMake preset: "local-release" when CMakeUserPresets.json defines it,
       otherwise "release" (which needs the QT_ROOT environment variable).
    2. Loads the MSVC build environment (vcvars64.bat, found with vswhere) into this
       PowerShell process only, unless cl.exe is already available.
    3. Finds cmake.exe and ninja.exe in the Qt installation's Tools folder derived from
       QT_ROOT (<Qt>\Tools\CMake_64, <Qt>\Tools\Ninja), falling back to PATH.
    4. Configures, builds, runs the "deploy" target (windeployqt) and starts the app.

    No machine-specific paths are stored in this script. Nothing outside this process
    is changed (PATH changes are process-local).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -AppArgs '--rate','2000'
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -NoLaunch
#>
[CmdletBinding()]
param(
    [string]   $Preset = '',
    [string[]] $AppArgs = @(),
    [switch]   $NoLaunch
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Write-Step([string]$text) { Write-Host ""; Write-Host "==> $text" -ForegroundColor Cyan }
function Write-Ok([string]$text)   { Write-Host "    $text" -ForegroundColor Green }
function Write-Info([string]$text) { Write-Host "    $text" }
function Fail([string]$text) {
    Write-Host ""
    Write-Host "ERROR: $text" -ForegroundColor Red
    exit 1
}

Set-Location $repo

# --------------------------------------------------------------------- preset
Write-Step "Choosing the CMake preset"
$userPresets = Join-Path $repo 'CMakeUserPresets.json'
$userQtRoot = $null
if (Test-Path $userPresets) {
    $json = Get-Content $userPresets -Raw | ConvertFrom-Json
    $local = $json.configurePresets | Where-Object { $_.name -eq 'local-release' } | Select-Object -First 1
    if ($local -and $local.environment -and $local.environment.QT_ROOT) { $userQtRoot = $local.environment.QT_ROOT }
    if (-not $Preset -and $local) { $Preset = 'local-release' }
}
if (-not $Preset) { $Preset = 'release' }
Write-Ok "Preset: $Preset"

$qtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } elseif ($userQtRoot) { $userQtRoot } else { $null }
if ($Preset -eq 'release' -and -not $env:QT_ROOT) {
    Fail ("The 'release' preset needs the QT_ROOT environment variable (e.g. C:\Qt\6.12.0\msvc2022_64).`n" +
          "       Set it, or copy CMakeUserPresets.json.example to CMakeUserPresets.json and fill it in.")
}
if ($qtRoot) {
    if (-not (Test-Path (Join-Path $qtRoot 'bin\qmake.exe'))) { Fail "QT_ROOT '$qtRoot' does not look like a Qt kit (bin\qmake.exe is missing)." }
    Write-Ok "Qt kit: $qtRoot"
}
# <Qt>\<version>\<kit> -> <Qt>\Tools
$qtTools = if ($qtRoot) { Join-Path (Split-Path -Parent (Split-Path -Parent $qtRoot)) 'Tools' } else { $null }

# ---------------------------------------------------------------- MSVC env
Write-Step "Preparing the MSVC build environment"
if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    Write-Ok "cl.exe is already available in this shell."
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { Fail "Visual Studio Installer (vswhere.exe) not found. Install Visual Studio with the 'Desktop development with C++' workload." }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { Fail "No Visual Studio installation with the C++ x64 tools was found." }
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat not found under $vsPath." }
    Write-Info "Loading $vcvars"
    $envDump = & cmd.exe /s /c "`"$vcvars`" >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0) { Fail "vcvars64.bat failed." }
    foreach ($line in $envDump) {
        $i = $line.IndexOf('=')
        if ($i -gt 0) { Set-Item -Path ("Env:" + $line.Substring(0, $i)) -Value $line.Substring($i + 1) }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { Fail "cl.exe is still not available after vcvars64.bat." }
    Write-Ok "MSVC environment loaded from $vsPath"
}

# ------------------------------------------------------------ cmake, ninja
Write-Step "Locating CMake and Ninja"
# Prefer the tools shipped with the Qt installation: vcvars64.bat also puts Visual
# Studio's own CMake/Ninja on PATH, and mixing CMake versions in one build folder is fragile.
function Find-Tool([string]$exe, [string]$preferred) {
    if ($preferred -and (Test-Path $preferred)) { return $preferred }
    $cmd = Get-Command $exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}
$cmake = Find-Tool 'cmake.exe' $(if ($qtTools) { Join-Path $qtTools 'CMake_64\bin\cmake.exe' })
if (-not $cmake) { Fail "cmake.exe not found on PATH or in the Qt Tools folder." }
Write-Ok "CMake: $cmake"
$ninja = Find-Tool 'ninja.exe' $(if ($qtTools) { Join-Path $qtTools 'Ninja\ninja.exe' })
if (-not $ninja) { Fail "ninja.exe not found on PATH or in the Qt Tools folder." }
$env:PATH = (Split-Path -Parent $ninja) + ';' + $env:PATH   # this process only
Write-Ok "Ninja: $ninja"

# ------------------------------------------------------------------- build
Write-Step "Configuring ($Preset)"
& $cmake --preset $Preset | Out-Host
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed (see the output above)." }

Write-Step "Building ($Preset)"
& $cmake --build --preset $Preset | Out-Host
if ($LASTEXITCODE -ne 0) { Fail "Build failed (see the compiler output above)." }
Write-Ok "Build succeeded."

Write-Step "Deploying the Qt runtime next to the executable (windeployqt)"
& $cmake --build --preset $Preset --target deploy | Out-Host
if ($LASTEXITCODE -ne 0) { Fail "Deploy step failed (see the output above)." }

$exe = Join-Path $repo "build\$Preset\can_analyzer.exe"
if (-not (Test-Path $exe)) { Fail "Expected executable not found: $exe" }
Write-Ok "Ready: $exe"

# --------------------------------------------------------------------- run
if ($NoLaunch) {
    Write-Step "Done (not started because of -NoLaunch)"
    Write-Info "Start it any time by double-clicking: $exe"
    exit 0
}
Write-Step "Starting CAN Analyzer"
$startArgs = @{ FilePath = $exe; WorkingDirectory = $repo; PassThru = $true }
if ($AppArgs.Count -gt 0) { $startArgs.ArgumentList = $AppArgs }
$proc = Start-Process @startArgs
Write-Ok "Started (process id $($proc.Id)). Press F5 in the app to start the simulator."
