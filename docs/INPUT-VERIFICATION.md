# 입력 검증

검증 대상은 Windows x64 개발 빌드입니다. 코어·TSF 컨텍스트 검사와 실제 데스크톱 키 입력 검사는 서로 구분합니다.

## 자동 검사

`scripts/build.ps1`은 **19,452개 검사**를 실행합니다. 두벌식 한글 11,172개 음절, 무작위 조합, 혼합 조사, 후보 전환, 자모 삭제, 보호 정책, 실제 DLL의 COM·TSF 편집 세션을 검사합니다. 키 구독은 테스트용 어댑터를 사용하며 제품의 공유 메모리·사용자 설정과 분리합니다.

## 실제 입력 검사

전용 RichEdit와 Chrome의 로컬 테스트 페이지에 Windows 가상 키·스캔 코드를 보냅니다. 입력란을 실제 클릭하고 결과를 읽으며, 기대 문자열을 붙여넣지 않습니다. Chrome 검사는 별도의 임시 프로필을 사용합니다. 입력 텍스트가 포함된 로컬 결과 파일은 이 가상 샘플 검사에서만 생성되며 공개 소스에 포함하지 않습니다.

| 시나리오 | 기대 결과 |
| --- | --- |
| 기본 혼합 문장 | sunlight이 선명하게 leaf를 핥고 있었다 |
| 쌍시옷 받침 | 있었 → 있었ㄷ → 있었다 |
| 받침 Backspace | 있 → 이 → ㅇ |
| 영어 뒤의 한글 | hello 했 |
| 쌍자음·겹받침 | 썼다 봤다 갔다 핥고 |
| 영어 대소문자 | the The got GoT TOP API sunlight leaf 유지 |
| F9 | 조합 중 원문 후보로 전환 |
| Enter | 첫 Enter 확정, 다음 Enter 전달 |
| 혼합 조사 | github에 API를 |
| 포커스 전환·선택 교체 | 입력란별 조합 분리와 선택 영역 교체 |
| 이메일·암호 | 가상 영문 샘플 원문 유지 |
| 일시정지·재개 | 원문 전달·자동 판단 복구 |

위 항목을 **22개 검사**로 확인합니다. 검증 시 설치 DLL과 브라우저가 로드한 DLL의 SHA-256도 비교합니다. 실행 중인 앱은 이전 DLL을 유지할 수 있으므로 업데이트 후 다시 열어야 합니다.

## 재현

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 아래 명령은 설치된 Atypoi를 실행하고 선택한 상태에서 수행합니다.
powershell -ExecutionPolicy Bypass -File scripts/test-desktop-input.ps1
powershell -ExecutionPolicy Bypass -File scripts/test-browser-input.ps1 -Launch
powershell -ExecutionPolicy Bypass -File scripts/verify-installed-input.ps1
```

검사 중에는 전용 창의 키보드·마우스를 자동으로 조작합니다. 브라우저 재검사 전 기존 전용 테스트 브라우저를 닫으세요. 마지막 스크립트는 모드와 후보 단축키를 자동·F9로 설정합니다.

GUI 미리보기·페이지 전환·GitHub 링크 존재·단일 실행·트레이 복원·닫기/종료·데모의 자동 시작 보존은 `scripts/smoke-ui.ps1`로 확인합니다. 실행 중인 트레이를 종료하고 수행해야 합니다.

## 화면 자료 생성

```powershell
powershell -ExecutionPolicy Bypass -File scripts/record-demo.ps1
```

이 명령은 자체 입력 테스트 창만 캡처하고 `tests/input-sample.json`의 가상 예문을 입력합니다. 결과가 일치할 때만 GIF를 생성합니다. 프레임·검사 결과는 `build`에, 공개할 GIF·화면은 `docs/images`에 저장합니다. 화면에는 사용자 문서·계정·데스크톱이 들어가지 않습니다.

## 검증 한계

모든 웹 편집기, Edge·Word·메신저, x86·ARM64·관리자 앱, 재로그인·제거·재부팅·장시간·다중 DPI·스크린 리더의 종합 검증은 남아 있습니다. 독립 데이터에 대한 판별 정확도 수치도 아직 측정하지 않았습니다. 로컬 페이지의 성공을 모든 앱·사이트의 지원으로 일반화하지 않습니다.
