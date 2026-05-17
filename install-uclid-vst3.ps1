# Copies final VST3 build to FL user plugin folder (no admin required).
$ErrorActionPreference = "Stop"

$src = Join-Path $PSScriptRoot "final\VST3\Uclid.vst3"
$dstRoot = Join-Path $env:USERPROFILE "Documents\Image-Line\FL Studio\Plugins\VST3"
$dst = Join-Path $dstRoot "Uclid.vst3"

if (-not (Test-Path $src)) {
    Write-Error "Final build not found. Run: cmake --build build --config Release --target Uclid_VST3"
}

New-Item -ItemType Directory -Force -Path $dstRoot | Out-Null
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
Copy-Item -Recurse -Force $src $dst

Write-Host "Installed to: $dstRoot"
Write-Host "Or use final build directly: $(Join-Path $PSScriptRoot 'final\VST3')"
