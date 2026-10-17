# GitHub 공개 준비

대상 저장소: <https://github.com/ssrihappy/atypoi>

## 공개용 소스 만들기

```powershell
powershell -ExecutionPolicy Bypass -File scripts/audit-publication.ps1 -CheckBinaries
powershell -ExecutionPolicy Bypass -File scripts/export-source.ps1
```

소스 ZIP과 `publish/atypoi` 폴더를 생성합니다. 공개 폴더는 허용한 소스 파일만 복사한 새 스냅샷이며 `.git` 이력이 없습니다. `.tools`, `build`, `dist`, 브라우저 프로필·로컬 검사 기록·인증서·환경 파일은 들어가지 않습니다. 업로드할 때는 이 폴더나 소스 ZIP을 사용하세요.

## 빈 저장소에 처음 올리기

프로젝트 루트에서 내보내기를 실행한 뒤 다음 명령을 사용합니다. 커밋 작성자 이름과 이메일은 Git에 설정한 값으로 기록되므로 GitHub의 비공개 noreply 이메일 설정을 선택할 수 있습니다.

```powershell
Set-Location publish/atypoi
git init -b main
git add .
git diff --cached --stat
git commit -m "Initial open-source release"
git remote add origin https://github.com/ssrihappy/atypoi.git
git push -u origin main
```

이미 커밋이 있는 원격 저장소라면 기존 저장소를 clone한 후 공개 폴더의 파일을 복사해 별도 브랜치와 PR로 반영하세요. 기존 이력을 강제로 덮어쓸 필요는 없습니다. 이 문서의 명령을 실행하기 전까지 원격 저장소에는 게시되지 않습니다.

## 릴리스 파일

설치 파일은 소스 커밋에 넣지 않고 GitHub Releases에 첨부합니다.

- `dist/Atypoi.Setup.exe`
- 해당 버전의 소스 ZIP
- `dist/SHA256.json`, `dist/ModelHashes.json`

개발 빌드에는 서명이 없다는 사실과 지원 플랫폼 Windows x64를 릴리스 설명에 적어 주세요. 정식 서명 빌드의 인증서·타임스탬프·업데이트 설정은 `scripts/package.ps1`과 `src/win/update.h`를 참고하세요. 인증서와 개인키는 저장소에 넣지 않습니다.

## 문서와 이미지

저장소 첫 화면은 `README.md`, 상세 안내는 `docs/USAGE.md`입니다. 스크린샷과 타이핑 GIF는 `docs/images`의 상대 경로로 연결되어 GitHub에서도 표시됩니다. 소셜 미리보기 이미지는 `assets/brand/social-preview.png`를 저장소 설정에 사용할 수 있습니다.

GitHub Actions는 일반 커밋과 PR에서 빌드·검사를 수행하고 설치 파일과 소스 ZIP을 보관합니다. 소스 버전과 일치하는 안정 버전 태그를 push하면 검사에 통과한 동일 아티팩트를 공개 릴리스로 게시합니다. 프로젝트 고유의 MIT 라이선스와 모든 제3자 고지를 함께 배포하세요.

## 다음 버전 자동 배포

1. `src/win/platform.h`, `resources/app.manifest`, `resources/version.rc`와 문서의 버전을 올립니다. 예: `0.1.7`.
2. 변경을 검토하고 `main`에 push합니다.
3. 해당 커밋에 태그를 붙여 push합니다.

```powershell
git tag v0.1.7
git push origin v0.1.7
```

태그용 GitHub Actions의 빌드·테스트·패키징이 모두 성공하면 `scripts/publish-release.ps1`가 설치 파일, 소스 ZIP, 해시 목록, `update.txt`를 초안 릴리스에 함께 업로드하고 최신 릴리스로 공개합니다. 소스 버전과 태그가 다르거나 기존 릴리스가 있으면 중단합니다. 기존 파일을 덮어쓰지 말고 새 버전을 만드세요. GitHub Actions의 `GITHUB_TOKEN`만 사용하므로 개인 액세스 토큰을 저장소에 추가할 필요가 없습니다.

기본 업데이트 주소:

`https://github.com/ssrihappy/atypoi/releases/latest/download/update.txt`

`update.txt`에는 버전, 해당 태그의 고정 설치 파일 URL, SHA256이 들어갑니다. 일반 코드 push와 사전 릴리스는 사용자 업데이트 대상이 아닙니다. 공개 릴리스 게시 후 앱이 다음 확인 시점에 감지합니다. 지금 확인하려면 설정의 **업데이트**를 누르세요.

현재 서명 없는 공개 빌드는 공식 저장소의 정확한 버전별 설치 주소와 HTTPS·SHA256을 확인하고 설치 전 사용자의 승인을 받습니다. 이는 게시자 서명 검증과 다릅니다. 서명된 앱은 같은 게시자 서명까지 검사하며, 사용자 지정 업데이트 주소는 신뢰된 서명이 있어야 사용할 수 있습니다. 인증서와 개인키는 저장소에 넣지 마세요.

## Code signing policy

[서명 정책과 신청 상태](CODE-SIGNING.md)를 참고하세요. SignPath Foundation 무료 서명 신청을 접수했고 심사 대기 중이며 아직 인증서나 서명 서비스 승인을 받지 않았습니다. 현재 공개 v0.1.6은 서명되지 않았습니다.

`scripts/verify-signatures.ps1`는 앱·입력기·설치 파일의 제품 정보와 서명을 검사합니다. 신뢰되지 않는 서명, 일부 파일만 서명한 패키지, 서로 다른 게시자 인증서, 타임스탬프 누락은 거부합니다. `-RequireTrustedSignatures`를 주면 서명 없는 패키지도 거부합니다.

승인 후에는 앱과 입력기에 먼저 서명하고, 그 파일들을 포함해 설치 프로그램을 만든 뒤 설치 프로그램에 서명해야 합니다. 서명이 끝난 파일로 해시와 업데이트 정보를 생성하세요. GitHub Actions 서명 연동과 승인 절차를 검증한 뒤 저장소 변수 `REQUIRE_SIGNED_RELEASE=true`를 설정하면 공개 단계에서 서명 없는 파일을 차단할 수 있습니다. 이 변수를 켜는 것만으로 서명이 생성되지는 않습니다.
