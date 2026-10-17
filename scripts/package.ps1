param([string]$Toolchain = '', [string]$CertificateThumbprint = '', [string]$SignTool = 'signtool.exe', [string]$TimestampUrl = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
if (-not $Toolchain) { $Toolchain = Join-Path $projectRoot '.tools\llvm-mingw-20261006-ucrt-x86_64' }
$compiler = Join-Path $Toolchain 'bin\x86_64-w64-mingw32-clang++.exe'
$resourceCompiler = Join-Path $Toolchain 'bin\x86_64-w64-mingw32-windres.exe'
function Sign-Artifact([string]$Path) {
    $signArgs = @('sign','/sha1',$CertificateThumbprint,'/fd','SHA256')
    if ($TimestampUrl) { $signArgs += @('/tr',$TimestampUrl,'/td','SHA256') }
    & $SignTool @signArgs $Path
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $Path" }
    & $SignTool verify /pa $Path
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $Path" }
}
if ($CertificateThumbprint) { Sign-Artifact 'dist\Atypoi.exe'; Sign-Artifact 'dist\Atypoi.IME.dll' }
$noticeFiles = @('LICENSE','THIRD_PARTY_NOTICES.md','third_party\seamtype\LICENSE','third_party\seamtype\scowl-Copyright','third_party\seamtype\tatoeba.txt','third_party\LLVM-MinGW-LICENSE.txt','third_party\mingw\COPYING.MinGW-w64.txt','third_party\mingw\COPYING.MinGW-w64-runtime.txt','third_party\mingw\COPYING.winpthreads.txt')
$notice = ($noticeFiles | ForEach-Object { [System.IO.File]::ReadAllText((Join-Path $projectRoot $_)) }) -join "`r`n`r`n"
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'build\ThirdPartyNotices.txt'),$notice,[System.Text.UTF8Encoding]::new($false))
Push-Location resources
try { & $resourceCompiler setup.rc -O coff -o '..\build\setup.o'; if ($LASTEXITCODE -ne 0) { throw 'Setup resource build failed' } } finally { Pop-Location }
$flags = @('-std=c++20','-O2','-Wall','-Wextra','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-static','-municode','-mwindows')
$flags += @(('-ffile-prefix-map='+$projectRoot+'=.'), ('-ffile-prefix-map='+$projectRoot.Replace('\','/')+'=.'))
if ($CertificateThumbprint) { $flags += '-DATYPOI_RELEASE' }
& $compiler @flags src/core/engine.cpp src/setup/main.cpp build/setup.o -lole32 -loleaut32 -luuid -ladvapi32 -lshell32 -luser32 -lgdi32 -lcomctl32 -lbcrypt -lwintrust -lcrypt32 '-Wl,--no-insert-timestamp' -o dist/Atypoi.Setup.exe
if ($LASTEXITCODE -ne 0) { throw 'Setup build failed' }
if ($CertificateThumbprint) { Sign-Artifact 'dist\Atypoi.Setup.exe' }
$verify = Start-Process -FilePath (Join-Path $projectRoot 'dist\Atypoi.Setup.exe') -ArgumentList '--verify-payload' -WindowStyle Hidden -PassThru -Wait
if ($verify.ExitCode -ne 0) { throw 'Installer embedded payload validation failed' }
Get-ChildItem dist\Atypoi* -File | Get-FileHash -Algorithm SHA256 | Select-Object @{N='File';E={Split-Path -Leaf $_.Path}},Hash | ConvertTo-Json | Set-Content -LiteralPath dist\SHA256.json -Encoding UTF8
Get-ChildItem data\*.txt -File | Get-FileHash -Algorithm SHA256 | Select-Object @{N='File';E={Split-Path -Leaf $_.Path}},Hash | ConvertTo-Json | Set-Content -LiteralPath dist\ModelHashes.json -Encoding UTF8
Write-Host 'Installer ready: dist\Atypoi.Setup.exe'
Copy-Item -LiteralPath README.md,LICENSE,CONTRIBUTING.md,THIRD_PARTY_NOTICES.md -Destination dist -Force
Copy-Item -LiteralPath assets -Destination dist -Recurse -Force
Copy-Item -LiteralPath third_party -Destination dist -Recurse -Force
if (Test-Path -LiteralPath docs\IMPLEMENTATION.md) { Copy-Item -LiteralPath docs\IMPLEMENTATION.md -Destination dist -Force }
Copy-Item -LiteralPath docs -Destination dist -Recurse -Force
Copy-Item -LiteralPath PRD.md -Destination dist -Force
Copy-Item -LiteralPath docs\INPUT-VERIFICATION.md -Destination dist -Force
if (-not $CertificateThumbprint) { Write-Host 'Development installer is unsigned. Supply a publisher certificate for release.' }
