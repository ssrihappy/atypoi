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

GitHub Actions는 빌드·검사를 수행하고 개발 설치 파일과 소스 ZIP을 아티팩트로 보관합니다. 릴리스 게시와 서명은 자동 실행하지 않습니다. 프로젝트 고유의 MIT 라이선스와 모든 제3자 고지를 함께 배포하세요.
