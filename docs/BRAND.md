# Atypoi brand

**Atypoi = AI + typo.** 한영 전환 때문에 끊기던 입력 흐름을 자연스럽게 이어주는 Windows 입력기입니다. 현재 제품의 판단은 로컬 사전·통계 모델을 사용합니다. 클라우드 생성형 AI나 외부 API 연결을 의미하지 않습니다.

## 로고

소문자 `a`의 둥근 획은 입력의 흐름을, 오른쪽 세로획은 입력 커서를 나타냅니다. 커서 위의 점을 함께 읽으면 `ai`가 됩니다. 소문자 워드마크 `atypoi`는 친근하고 차분한 인상을 유지합니다.

| 파일 | 용도 |
| --- | --- |
| `assets/brand/icon.svg` | 확대 가능한 심벌 원본 |
| `assets/brand/wordmark.svg` | 밝은 배경의 로고와 이름 |
| `assets/brand/icon.png` | 256px 앱 이미지 |
| `assets/brand/social-preview.svg` / `.png` | GitHub 소셜 미리보기, 1280×640 |
| `resources/atypoi.ico` | 16·20·24·32·40·48·64·128·256px Windows 아이콘 |
| `docs/images/*.png` | 실제 Windows 앱 화면 |

심벌 주변에는 폭의 1/4 이상 여백을 두고 비율을 유지합니다. 작은 아이콘은 심벌만 사용합니다. 상태는 트레이의 작은 배지와 한국어 툴팁으로 전달합니다.

## 색과 글꼴

| 역할 | 값 |
| --- | --- |
| 주요 동작 | Violet `#6750E4` |
| 커서 포인트 | Mint `#BAF2DF` |
| 본문 | Ink `#1D2131` |
| 보조 글자 | Slate `#686F82` |
| 창 배경 | Mist `#F6F7FB` |
| 카드 | White `#FFFFFF` |

영문 워드마크와 큰 제목은 Segoe UI, 한국어 본문은 맑은 고딕을 사용합니다. 모두 설치된 Windows 시스템 글꼴을 참조하며 글꼴 파일을 재배포하지 않습니다. 본문 14px, 섹션 제목 16px, 워드마크 34px를 기준으로 DPI에 맞춰 확대합니다.

둥근 카드, 12~24px 간격, 하나의 보라색 주요 버튼을 기본으로 합니다. 키보드 포커스를 표시하고, Windows 고대비 모드에서는 시스템 색상을 사용합니다. 일반 설정은 바로 적용되며 앱별 규칙만 저장 버튼으로 적용합니다.

## 생성

```powershell
powershell -ExecutionPolicy Bypass -File scripts/branding.ps1
```

System.Drawing으로 벡터와 동일한 기하를 렌더링하므로 외부 이미지 서비스나 빌드 시 네트워크 호출이 필요하지 않습니다. 생성한 브랜드 자산은 Atypoi의 MIT 라이선스로 배포합니다.
