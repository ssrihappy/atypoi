param([Parameter(Mandatory=$true)][string]$Tag)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$source = [IO.File]::ReadAllText((Join-Path $projectRoot 'src\win\platform.h'))
$version = [regex]::Match($source,'version\[\]=L"([^"]+)"').Groups[1].Value
if ($version -notmatch '^\d+\.\d+\.\d+$' -or $Tag -cne "v$version") { throw 'Release tag must match the stable source version.' }
$signature = & "$PSScriptRoot\verify-signatures.ps1" -RequireTrustedSignatures:($env:ATYPOI_REQUIRE_SIGNED_RELEASE -eq 'true')
$existing = & gh release list --repo ssrihappy/atypoi --limit 100 --json tagName
if ($LASTEXITCODE -ne 0) { throw 'Could not check existing releases.' }
if (($existing | ConvertFrom-Json).tagName -ccontains $Tag) { throw 'Release already exists; published assets are immutable. Use a new version.' }
& "$PSScriptRoot\write-update-feed.ps1" -Version $version -DownloadUrl "https://github.com/ssrihappy/atypoi/releases/download/$Tag/Atypoi.Setup.exe"
$files = @('Atypoi.exe','Atypoi.IME.dll','Atypoi.Setup.exe',"Atypoi-$version-source.zip",'update.txt')
$files | ForEach-Object {
    $path = Join-Path $projectRoot "dist\$_"
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing release artifact: $_" }
    [pscustomobject]@{File=$_;Hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
} | ConvertTo-Json | Set-Content -LiteralPath dist\SHA256.json -Encoding UTF8
$notes = @"
Windows x64 · Atypoi $version

- GitHub Releases와 연결된 자동 업데이트: 시작 10초 후와 실행 중 24시간마다 확인하고 다운로드합니다. 알림을 눌러 설치를 승인하세요.
- 트레이 메뉴의 자동 업데이트 확인을 끄거나 설정의 업데이트 버튼으로 수동 확인할 수 있습니다.
- GitHub 링크 배경을 설정 창과 자연스럽게 연결했습니다.

$(if($signature.Signed){'앱·입력기·설치 파일의 신뢰된 게시자 서명과 타임스탬프를 검증했습니다.'}else{'설치 파일은 게시자 서명이 없습니다.'}) 앱은 공식 버전별 GitHub 주소와 SHA256을 확인하며 설치 전 동의를 받습니다. 업데이트 후 입력하던 앱을 완전히 종료하고 다시 실행하세요.
이전 0.1.5-dev 이하는 자동 업데이트 주소가 없으므로 이번 설치 파일을 한 번 직접 실행해야 합니다.

[Code signing policy](https://github.com/ssrihappy/atypoi/blob/main/docs/CODE-SIGNING.md)
"@
New-Item -ItemType Directory -Force build | Out-Null
[IO.File]::WriteAllText((Join-Path $projectRoot 'build\release-notes.md'),$notes,[Text.UTF8Encoding]::new($false))
$assets = @('dist/Atypoi.Setup.exe',"dist/Atypoi-$version-source.zip",'dist/SHA256.json','dist/ModelHashes.json','dist/update.txt')
& gh release create $Tag @assets --repo ssrihappy/atypoi --verify-tag --draft --title "Atypoi $version · GitHub 자동 업데이트" --notes-file build/release-notes.md
if ($LASTEXITCODE -ne 0) { throw 'Draft release upload failed.' }
& gh release edit $Tag --repo ssrihappy/atypoi --draft=false --latest
if ($LASTEXITCODE -ne 0) { throw 'Release publication failed; draft retained for inspection.' }
