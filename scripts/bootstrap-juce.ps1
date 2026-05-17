# Optional: download JUCE into external/JUCE (offline builds without FetchContent).
$ErrorActionPreference = "Stop"
$dest = Join-Path $PSScriptRoot "..\external\JUCE"
if (Test-Path (Join-Path $dest "CMakeLists.txt")) {
    Write-Host "JUCE already present at $dest"
    exit 0
}
$zip = Join-Path $PSScriptRoot "..\external\juce-8.0.4.zip"
New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
curl.exe -L "https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip" -o $zip
Expand-Archive -Path $zip -DestinationPath (Split-Path $dest) -Force
Rename-Item (Join-Path (Split-Path $dest) "JUCE-8.0.4") "JUCE"
Remove-Item $zip
Write-Host "JUCE installed to $dest"
