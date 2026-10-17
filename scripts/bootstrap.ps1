$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsDirectory = Join-Path $projectRoot '.tools'
New-Item -ItemType Directory -Force $toolsDirectory | Out-Null
$archive = Join-Path $toolsDirectory 'llvm-mingw.zip'
$expected = '317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a'
if (-not (Test-Path -LiteralPath $archive)) {
    & curl.exe -L --fail --silent --show-error 'https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-x86_64.zip' -o $archive
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain download failed' }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Toolchain checksum mismatch' }
if (-not (Test-Path -LiteralPath (Join-Path $toolsDirectory 'llvm-mingw-20261006-ucrt-x86_64\bin\clang++.exe'))) {
    & tar.exe -xf $archive -C $toolsDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain extraction failed' }
}
Write-Host 'Portable toolchain ready. System PATH and global developer tools were not changed.'
