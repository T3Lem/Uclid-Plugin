# Run after: gh auth login
# Creates public repo yodem/uclid-plugin and pushes (same style as tal-protfolio).
$ErrorActionPreference = "Stop"
$env:Path = "C:\Program Files\Git\cmd;C:\Program Files\GitHub CLI;" + $env:Path

$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot

gh auth status | Out-Null

$repoName = "uclid-plugin"
$user = gh api user -q .login
$fullName = "$user/$repoName"

Write-Host "Creating GitHub repo: $fullName"

git branch -M main 2>$null

if (-not (git remote get-url origin 2>$null)) {
    gh repo create $fullName --public --description "Uclid — Euclidean rhythm volume gate (JUCE VST3)" --source=. --remote=origin --push
} else {
    git push -u origin main
}

Write-Host ""
Write-Host "Done: https://github.com/$fullName"
