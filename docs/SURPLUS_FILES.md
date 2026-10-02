# 잉여 파일 목록 (2026-09-25)

> **2026-09-30 정리 결과:** 1번만 처리했다. `새 텍스트 문서.txt`(옛 ge_python 오류 메모, git 이력에 남음)와 빌드 로그 7개, 빈 `editor/qml/`는 삭제했다. `새 텍스트 문서 (2).txt`는 확인해 보니 VFX Lite 타당성 검토 보고서(git 미추적, 삭제 시 복구 불가)라서 `docs/archive/VFX_LITE_FEASIBILITY_REVIEW.txt`로 옮겨 보존했다. 2번의 `engine/asset/`은 삭제하지 않고 **텍스처 기능으로 연결하기로 했다**(사용자 결정: 정적 분석상 미사용이라고 지우면, 에디터 사용자는 기능이 구현되지 않은 것으로 경험한다). 나머지 2번 코드와 3번 문서는 그대로 두었다.

기준: 코드/빌드/문서 어디에서도 참조되지 않거나, 임시 산출물인 파일이다. 아직 아무것도 삭제하지 않았다.

## 1. 임시 파일 / 로그 (삭제 권장)

| 파일 | 근거 |
|---|---|
| `새 텍스트 문서.txt` | 루트의 개인 메모(커밋됨) |
| `새 텍스트 문서 (2).txt` | 루트의 개인 메모(untracked) |
| `engine/build_ge_python_log_release4.txt` ~ `release10.txt` (7개) | 옛 이름(`ge_python`) 빌드 로그, `.gitignore` 대상 |
| `engine/editor/qml/` | 빈 폴더 |

## 2. 참조되지 않는 코드

| 파일 | 근거 |
|---|---|
| `engine/editor/ge_transaction.py` | 어떤 모듈도 import하지 않음 |
| `engine/editor/qt_engine_viewport.py` | 어떤 모듈도 import하지 않음(`viewport.py`의 `EngineViewport`가 실제 사용본) |
| `engine/test_imports.py` | 수동 import 점검 스크립트, 테스트 체계 밖 |
| `engine/job/JobSystemExample.h` | 어떤 파일도 include하지 않음(예제 코드) |
| `engine/core/StringHash.cpp` | 엔진 CMake 소스 목록에 없음(테스트 GLOB으로만 컴파일됨) |
| `engine/core/memory/PoolAllocator.cpp/.h` | 엔진 코드에서 미사용, 테스트에서만 사용 |
| `engine/core/memory/StackAllocator.cpp/.h` | 엔진 코드에서 미사용, 테스트에서만 사용 |
| `engine/asset/` (AssetManager, AssetRegistry, AssetCache, TextureImporter 등) | 빌드는 되지만 모듈 밖에서 include하는 곳이 없음(고아 모듈, ROADMAP에도 "고아"로 기록됨) |

## 3. 구버전/중복 문서 (정리 또는 `docs/archive/` 이동 권장)

| 파일 | 근거 |
|---|---|
| `PLAN.md` | 옛 모듈명 `ge_python` 기준(INGAME_UI 계획서 §8.2) |
| `UNUSED_AND_LEGACY_REPORT.md` | 옛 모듈명 사용, 이 문서로 대체됨 |
| `PROBLEM_ANALYSIS_REPORT.md` | 초기 분석 보고서, 이후 ROADMAP과 CLAUDE.md로 흡수됨 |
| `engine/ARCHIVE_INFO.md` | 존재하지 않는 `BUILD_LEGACY_MAIN` 옵션을 기술함 |
| `docs/PYTHON_EDITOR_BUG_ANALYSIS_KO.md` | 2026-08-16 시점 분석, 이후 수정 기록은 ROADMAP에 있음 |
| `docs/archive/legacy/` | 이미 보관 처리된 문서 |

## 4. 확인 필요 (사용 여부가 불분명)

| 파일 | 근거 |
|---|---|
| `engine/assets/models/` | `.gitignore` 대상, 코드에서 경로를 참조하지 않음 |
| `engine/check_build_prerequisites.ps1/.sh` | 문서에서 안내하지 않음 |
| `scripts/run_editor.bat.in`, `run_editor.sh.in` | CMake `configure_file`에서 쓰는지 확인 필요 |
| `.idea/`, `.vscode/` | IDE 설정, `.gitignore` 대상 |
