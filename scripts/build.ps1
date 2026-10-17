param([string]$Toolchain = '', [string]$UpdateFeed = '', [switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
if (-not $Toolchain) { $Toolchain = Join-Path $projectRoot '.tools\llvm-mingw-20261006-ucrt-x86_64' }
$compiler = Join-Path $Toolchain 'bin\x86_64-w64-mingw32-clang++.exe'
$resourceCompiler = Join-Path $Toolchain 'bin\x86_64-w64-mingw32-windres.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Run scripts/bootstrap.ps1 first, or supply -Toolchain.' }
New-Item -ItemType Directory -Force build,dist | Out-Null
Push-Location resources
try {
    & $resourceCompiler models.rc -O coff -o '..\build\models.o'
    if ($LASTEXITCODE -ne 0) { throw 'Model resource compilation failed' }
    & $resourceCompiler app.rc -O coff -o '..\build\app.o'
    if ($LASTEXITCODE -ne 0) { throw 'Application resource compilation failed' }
    & $resourceCompiler ime.rc -O coff -o '..\build\ime.o'
    if ($LASTEXITCODE -ne 0) { throw 'IME resource compilation failed' }
} finally { Pop-Location }
$common = @('-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-static','-Isrc')
$common += @(('-ffile-prefix-map='+$projectRoot+'=.'), ('-ffile-prefix-map='+$projectRoot.Replace('\','/')+'=.'))
if ($UpdateFeed) {
    if ($UpdateFeed -notmatch '^https://[^\s"<>]+$') { throw 'UpdateFeed must be an HTTPS URL.' }
    $common += ('-DATYPOI_UPDATE_FEED=L"' + $UpdateFeed + '"')
}
$libs = @('-lole32','-loleaut32','-luuid','-ladvapi32','-lshell32','-luser32','-lgdi32','-lcomctl32','-lcomdlg32','-limm32','-lwinhttp','-lwintrust','-lcrypt32','-lbcrypt')
& $compiler @common -shared src/core/engine.cpp src/ime/ime.cpp src/ime/exports.def build/ime.o @libs '-Wl,--no-insert-timestamp' -o dist/Atypoi.IME.dll
if ($LASTEXITCODE -ne 0) { throw 'IME build failed' }
& $compiler @common -municode -mwindows src/core/engine.cpp src/tray/main.cpp build/models.o build/app.o @libs '-Wl,--no-insert-timestamp' -o dist/Atypoi.exe
if ($LASTEXITCODE -ne 0) { throw 'Tray build failed' }
& $compiler @common -DATYPOI_TESTING -shared src/core/engine.cpp src/ime/ime.cpp src/ime/exports.def build/ime.o @libs -o build/Atypoi.TestIME.dll
if ($LASTEXITCODE -ne 0) { throw 'Isolated test IME build failed' }
& $compiler @common -DATYPOI_TESTING -municode src/core/engine.cpp tests/tests.cpp @libs -o build/Atypoi.Tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Tests build failed' }
& $compiler @common -municode tests/profile-diagnostic.cpp @libs -o build/profile-diagnostic.exe
if ($LASTEXITCODE -ne 0) { throw 'Profile diagnostic build failed' }
if (-not $SkipTests) {
    & '.\build\Atypoi.Tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
}
Copy-Item -LiteralPath README.md,LICENSE,THIRD_PARTY_NOTICES.md -Destination dist
Copy-Item -LiteralPath third_party -Destination dist -Recurse -Force
Write-Host 'Build complete: dist\Atypoi.exe and dist\Atypoi.IME.dll'
