# Export only the reviewable project, without local tools, caches or user data.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'audit-publication.ps1')
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$header = [IO.File]::ReadAllText((Join-Path $root 'src\win\platform.h'))
$match = [regex]::Match($header,'version\[\]=L"([0-9A-Za-z.\-]+)"')
if (-not $match.Success) { throw 'Cannot read project version' }
$version = $match.Groups[1].Value
New-Item -ItemType Directory -Force (Join-Path $root 'dist') | Out-Null
$target = Join-Path $root "dist\Atypoi-$version-source.zip"
$items = @('src','resources','scripts','tests','data','third_party','docs','assets','.github','.gitignore','LICENSE','README.md','CONTRIBUTING.md','PRD.md','THIRD_PARTY_NOTICES.md')
$files = @($items | ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $root $_) -File -Recurse -Force } | Sort-Object FullName)
$snapshot=Join-Path $root 'publish\atypoi'
$snapshotFull=[IO.Path]::GetFullPath($snapshot)
if(-not $snapshotFull.StartsWith(([IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Export directory must stay inside the project'}
if(Test-Path -LiteralPath (Join-Path $snapshot '.git')){throw 'The export folder is now a Git checkout. Move it before exporting again.'}
New-Item -ItemType Directory -Force $snapshot | Out-Null
$expected=@($files | ForEach-Object {$_.FullName.Substring($root.Length+1)})
$existing=@(Get-ChildItem -LiteralPath $snapshot -Recurse -Force)
if($existing | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Export directory cannot contain reparse points'}
foreach($file in @($existing | Where-Object {-not $_.PSIsContainer})){
    if($file.FullName.Substring($snapshotFull.Length+1) -notin $expected){throw 'Export directory contains unrelated files. Move them before exporting again.'}
}
foreach($file in $files){
    $destination=Join-Path $snapshot ($file.FullName.Substring($root.Length+1))
    New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
}
$stream = [IO.File]::Open($target,[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite)
$zip = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false)
try {
    foreach($file in $files) {
        $entry = $file.FullName.Substring($root.Length+1).Replace('\','/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$entry,[IO.Compression.CompressionLevel]::Optimal)
    }
} finally { $zip.Dispose();$stream.Dispose() }
$check=[IO.Compression.ZipFile]::OpenRead($target)
try {
    if($check.Entries.Count -ne $files.Count) { throw 'Source archive entry count mismatch' }
    foreach($entry in $check.Entries){if($entry.FullName -match '^(build|dist|\.tools)/'){throw 'Unexpected local output in source archive'}}
} finally {$check.Dispose()}
Get-ChildItem (Join-Path $root 'dist\Atypoi*') -File | Get-FileHash -Algorithm SHA256 | Select-Object @{N='File';E={Split-Path -Leaf $_.Path}},Hash | ConvertTo-Json | Set-Content (Join-Path $root 'dist\SHA256.json') -Encoding UTF8
Write-Host "Source archive ready: $target ($($files.Count) files)"
Write-Host 'Upload folder ready: publish/atypoi (no Git history or local build data)'
