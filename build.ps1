# Usage: ./build.ps1 -Config Release -Clean -Tests
param (
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$Clean,
    [switch]$Tests
)

$Preset = "Qt-$Config"
$BuildDir = "$PSScriptRoot/out/build/$Preset"

Write-Host "=== Engine Build: $Config ===" -ForegroundColor Cyan

# 1. Find VS Path using vswhere
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $vsPath) {
    Write-Error "Visual Studio C++ tools not found."
    exit 1
}

# 2. Import VS Environment Variables into PowerShell
# This block runs the batch file and imports the resulting variables back into the PS session
$devCmd = "$vsPath\Common7\Tools\VsDevCmd.bat"
$tempFile = [IO.Path]::GetTempFileName()
cmd /c " `"$devCmd`" -arch=amd64 -no_logo && set > `"$tempFile`" "
Get-Content $tempFile | Foreach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:\$($matches[1])" -Value $matches[2]
    }
}
Remove-Item $tempFile

# 3. Handle specific project variables (ensure they exist for CMakePresets)
if (-not $env:QTDIR) {
    # Attempt to use the auto-detection logic from your CMakeLists.txt
    $qtPaths = Get-ChildItem "C:/Qt/6.*" | Sort-Object Name -Descending
    if ($qtPaths) {
        $env:QTDIR = "$($qtPaths[0].FullName)/msvc2022_64"
        Write-Host "Auto-detected QTDIR: $env:QTDIR" -ForegroundColor Yellow
    }
}

# 4. Execute Build
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "--- Cleaning ---"
    Remove-Item -Recurse -Force $BuildDir
}

Write-Host "--- Configuring ---"
$extraArgs = if ($Tests) { "-DENGINE_BUILD_TESTS=ON" } else { "" }
cmake --preset $Preset $extraArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "--- Building ---"
    cmake --build $BuildDir
}

if ($Tests -and $LASTEXITCODE -eq 0) {
    Write-Host "--- Testing ---"
    ctest --preset $Preset
}