param([switch]$CheckBinaries)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$items=@('src','resources','scripts','tests','data','third_party','docs','assets','.github','.gitignore','LICENSE','README.md','CONTRIBUTING.md','PRD.md','THIRD_PARTY_NOTICES.md')
$files=@($items | ForEach-Object {Get-ChildItem -LiteralPath (Join-Path $root $_) -File -Recurse -Force})
$rules=@{
    'personal-profile-path'='(?i)(?:[A-Z]:[\\/]+Users[\\/]+[^\s"<>\x00]+|/(?:Users|home)/[^\s/"<>\x00]+)'
    'github-token'='(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})'
    'cloud-access-key'='(?:AKIA|ASIA)[A-Z0-9]{16}'
    'private-key'='-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'
    'service-key'='sk-(?:proj-)?[A-Za-z0-9_-]{32,}'
}
$issues=[Collections.Generic.List[object]]::new()
function Scan([string]$name,[string]$content,[bool]$binary=$false) {
    # Portable LLVM-MinGW runtime strings refer to its public CI build tree.
    if($binary -or $name -eq 'scripts/audit-publication.ps1'){$content=$content.Replace('/home/runner/work/llvm-mingw/','/toolchain/')}
    foreach($category in $rules.Keys){if([regex]::IsMatch($content,$rules[$category])){$issues.Add([pscustomobject]@{file=$name;category=$category})}}
}
foreach($file in $files){
    if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Publication source contains a reparse point'}
    $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
    if($file.Extension -notin '.png','.ico','.gif'){Scan $relative ([IO.File]::ReadAllText($file.FullName))}
}
$binaries=0
if($CheckBinaries){foreach($name in @('Atypoi.exe','Atypoi.IME.dll','Atypoi.Setup.exe')){
    $path=Join-Path $root ('dist\'+$name)
    if(-not (Test-Path -LiteralPath $path)){throw "Build artifact missing: $name"}
    $bytes=[IO.File]::ReadAllBytes($path)
    Scan ('dist/'+$name) ([Text.Encoding]::ASCII.GetString($bytes)) $true
    Scan ('dist/'+$name) ([Text.Encoding]::Unicode.GetString($bytes)) $true
    $binaries++
}}
if($issues.Count){$issues | Format-Table -AutoSize;throw 'Publication audit found paths or possible secrets; matched values are not printed'}
Write-Host "Publication audit passed: $($files.Count) source files, $binaries binaries; no matching personal paths or secret formats."
