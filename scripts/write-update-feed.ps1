param([Parameter(Mandatory=$true)][string]$Version,[Parameter(Mandatory=$true)][string]$DownloadUrl)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Use a stable version such as 0.1.1.' }
if ($DownloadUrl -notmatch '^https://[^\s"<>]+$') { throw 'Use an HTTPS download URL.' }
$installer = Join-Path $projectRoot 'dist\Atypoi.Setup.exe'
if ((Get-AuthenticodeSignature -LiteralPath $installer).Status -ne 'Valid') { throw 'Only publish an installer signed with a trusted publisher certificate.' }
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = "version=$Version`nurl=$DownloadUrl`nsha256=$hash`n"
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'dist\update.txt'),$manifest,[System.Text.UTF8Encoding]::new($false))
Write-Host 'dist\update.txt created. Upload it to the HTTPS UpdateFeed URL used when building the installed app.'
