# Run as Administrator — installs from final build output.
$ErrorActionPreference = "Stop"

$src = Join-Path $PSScriptRoot "final\VST3\Uclid.vst3"
$dst = "C:\Program Files\Common Files\VST3\Uclid.vst3"

if (-not (Test-Path $src)) {
    Write-Error "Final build not found. Run: cmake --build build --config Release --target Uclid_VST3"
}

$dll = Join-Path $src "Contents\x86_64-win\Uclid.vst3"
if (-not (Test-Path $dll)) {
    Write-Error "Invalid bundle: $src"
}

Write-Host "Source (final): $src"
Write-Host "Target:         $dst"

if (Test-Path $dst) {
    Remove-Item -LiteralPath $dst -Recurse -Force
}

Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force
Write-Host "Installed OK. Rescan plugins in FL Studio."
