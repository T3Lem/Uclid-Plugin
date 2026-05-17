# Run after: gh auth login
# Pushes to https://github.com/T3Lem/Uclid-Plugin
$ErrorActionPreference = "Stop"
$env:Path = "C:\Program Files\Git\cmd;C:\Program Files\GitHub CLI;" + $env:Path

$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot

gh auth status | Out-Null

$remoteUrl = "https://github.com/T3Lem/Uclid-Plugin.git"

git branch -M main 2>$null

if (git remote get-url origin 2>$null) {
    git remote set-url origin $remoteUrl
} else {
    git remote add origin $remoteUrl
}

git push -u origin main

Write-Host ""
Write-Host "Done: https://github.com/T3Lem/Uclid-Plugin"
