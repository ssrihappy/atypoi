param([Parameter(Mandatory=$true)][string]$Version,[Parameter(Mandatory=$true)][string]$DownloadUrl)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Use a stable version such as 0.1.1.' }
if ($DownloadUrl -notmatch '^https://[^\s"<>]+$') { throw 'Use an HTTPS download URL.' }
$installer = Join-Path $projectRoot 'dist\Atypoi.Setup.exe'
if ($DownloadUrl -cne "https://github.com/ssrihappy/atypoi/releases/download/v$Version/Atypoi.Setup.exe") { throw 'Use the official versioned GitHub release installer URL.' }
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = "version=$Version`nurl=$DownloadUrl`nsha256=$hash`n"
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'dist\update.txt'),$manifest,[System.Text.UTF8Encoding]::new($false))
Write-Host 'dist\update.txt created. Publish it with the matching installer on the latest GitHub release.'
