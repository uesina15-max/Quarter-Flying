# 미사용 파일 및 레거시 흔적 분석 보고서

**최초 작성일**: 2026-07-29
**1차 개정일**: 2026-08-10 (실제 프로젝트 재분석 — 이전 버전의 사실 오류 정정 + C++ Python 바인딩/더미 폴백 구조 섹션 신규 추가)
**2차 개정일**: 2026-08-10 (실행 스크립트 경로 버그 발견 + QML 아카이브 실제 위치 정정 + `죽은코드/` 폴더 신설 반영)
**분석 대상**: Quarter Flying Game Engine 프로젝트 (`d:\Quarter Flying`)
**분석 방법**: 파일 시스템 실측(디렉토리/파일 존재 여부, 용량), 소스 코드 전수 검색(`ge_python`, `TODO/FIXME`, `dummy/fallback`), 실행 스크립트 경로 추적, 문서-코드 교차 검증

> ⚠️ 이 보고서는 2026-07-29판의 여러 항목이 실제 파일 시스템/소스 코드와 대조한 결과 **사실과 다름**이 확인되어 두 차례 개정되었습니다. 정정 내역은 각 항목에 "🔧 정정" 표시로 구분했습니다.

---

## 📋 목차

1. [미사용 파일 및 디렉토리](#1-미사용-파일-및-디렉토리)
2. [레거시 코드 흔적](#2-레거시-코드-흔적)
3. [오래된 주석 및 TODO/FIXME](#3-오래된-주석-및-todofixme)
4. [C++ Python 바인딩 / 더미 폴백 구조](#4-c-python-바인딩--더미-폴백-구조)
5. [죽은코드 폴더 분류 내역 (2026-08-10 신설)](#5-죽은코드-폴더-분류-내역-2026-08-10-신설)
6. [정리 권장 사항](#6-정리-권장-사항)
7. [유지 보수 필요 항목](#7-유지-보수-필요-항목)
8. [요약](#8-요약)

---

## 1. 미사용 파일 및 디렉토리

### 1.1 아카이브된 파일

**🔧 정정 (2차 개정)**: 이전 판은 두 아카이브가 모두 "실제 파일이 존재하지 않음(정리 완료)"이라고 기술했으나, **QML 아카이브는 삭제된 적이 없었습니다.** 다른 경로에 그대로 남아 있었습니다.

#### 아카이브 1: QML 실험 — ❌ 이전 기술 오류

- **`engine/ARCHIVE_INFO.md`의 기술**: 이전 위치 `engine/editor/qml_archive/`, 현재 상태 "✅ 정리 완료(삭제됨)"
- **실제 상태**: 삭제된 것이 아니라 **`scripts/archive/qml_archive/`로 이동되어 9개 파일이 그대로 존재**하고 있었습니다.
  - `Main.qml`, `Theme.qml`, `SceneTreeItem.qml`, `ViewportPanel.qml`, `InspectorPanel.qml`
  - `qml_launcher.py`, `qml_engine_integration.py`
  - `QML_MODERNIZATION_PLAN.md`, `QML_MODERNIZATION_REPORT.md`
- **현재 조치**: 2026-08-10 `죽은코드/scripts/archive/qml_archive/`로 이동 분류 (5장 참고). 삭제하지 않았습니다.
- **후속 필요**: `engine/ARCHIVE_INFO.md`의 "정리 완료" 기술이 사실과 다르므로 수정 필요.

#### 아카이브 2: 레거시 GLFW 메인 — ✅ 기술 내용 맞음

- **이전 위치**: `engine/app/archive2/`
- **실제 상태**: `engine/app/` 디렉토리 자체가 존재하지 않음을 실측 확인. 이 항목은 문서 기술이 정확합니다.

**🔧 정정 (1차 개정, 유지)**: `engine/ARCHIVE_INFO.md`는 "Git 히스토리에서 `git checkout HEAD~N -- ...`로 복구"를 안내하지만, **이 프로젝트는 Git 저장소로 초기화되어 있지 않습니다** (`.git` 없음, `.gitignore`만 존재). 문서가 전제하는 Git 히스토리 복구는 불가능하며, GLFW 아카이브(`engine/app/`)는 실질적으로 완전히 소실된 상태입니다.

### 1.2 빌드 아티팩트

**상태**: ⚠️ 정리 필요 — 실측 결과 이전 보고서 수치와 차이 있음

#### 빌드 디렉토리 실측 결과

| 디렉토리 | 파일 수 | 용량 | 실제 상태 |
|---|---|---|---|
| `build/` (루트) | 6,731개 | 약 80.8MB | ✅ 활성 빌드 — `ge_python.vcxproj`, `run_editor.bat` 등 산출물 포함 |
| `engine/build/` | 96개 | 약 0.4MB | ⚠️ 사실상 빈 껍데기 — CMake 1차 구성만 되고 실제 컴파일 산출물 없음 |
| `engine/build_debug/` | 6,944개 | 약 389.7MB | ✅ 활성 Debug 빌드 — 실제 컴파일/링크 산출물 다수 포함 |

**🔧 정정**: 이전 보고서와 `engine/ARCHIVE_INFO.md`는 `engine/build/`, `engine/build_debug/`를 묶어 "중복이므로 삭제 권장"이라고 단정했으나, 실측 결과 두 디렉토리의 성격이 다릅니다.
- `engine/build/`는 사실상 미사용(거의 빈 CMake 캐시)이므로 삭제해도 무방합니다.
- `engine/build_debug/`는 390MB 규모의 실제 Debug 빌드 산출물로, 사용 중인 빌드일 가능성이 높습니다. 삭제 전 반드시 "현재 이 빌드 디렉토리를 IDE/CI가 참조 중인지" 확인이 필요합니다. 무조건 `rm -rf`를 권고하는 것은 위험합니다.
- 빌드 디렉토리가 3곳으로 분산된 근본 원인은 CMake 프리셋/빌드 스크립트가 출력 경로를 일관되게 지정하지 않기 때문입니다. `CMakePresets.json`으로 `Debug`/`Release` 출력 경로를 표준화하는 것이 재발 방지책입니다.

#### 대형 테스트 결과 파일

**🔧 정정**: `engine/baseline_test_results.txt` (7.8MB)는 **더 이상 존재하지 않습니다.** 전체 재검색 결과 파일을 찾을 수 없었으며, `engine/ARCHIVE_INFO.md`의 "2026-07-30: 대형 테스트 결과 파일 제거 완료" 기록과 일치합니다. 이전 보고서의 "즉시 수행: 대형 테스트 결과 파일 제거" 권고는 **이미 완료되어 더 이상 유효하지 않습니다.**

#### 기타 잔재 파일
- `engine/build_out.txt`, `engine/build_out2.txt` — 빌드 로그를 리다이렉트한 것으로 보이는 텍스트 파일.
- 루트 `새 텍스트 문서.txt` — Windows 기본 파일명 그대로 방치된 임시 메모 파일.

### 1.3 중복 의존성

**상태**: ⚠️ 실측 확인됨

- `build/deps/` — 루트 빌드의 FetchContent 의존성 (pybind11, nlohmann-json, fmt, glm, glew, glfw, stb, tinyobjloader, googletest, rapidcheck 등)
- `engine/build_debug/deps/` — 동일한 의존성 세트가 Debug 빌드용으로 다시 다운로드/빌드됨

두 빌드 디렉토리가 별도 트리이므로 CMake FetchContent가 의존성을 중복 다운로드/컴파일하고 있습니다. `FETCHCONTENT_BASE_DIR`을 두 빌드가 공유하는 상위 경로로 지정하거나 vcpkg/Conan 같은 공용 패키지 캐시로 전환하면 중복을 없앨 수 있습니다.

### 1.4 실행 스크립트(런처) — 🐛 경로 버그가 있는 죽은 코드 (2차 개정 신규)

이번 개정에서 **사용자가 실제로 겪은 실행 오류의 원인**을 특정했습니다. 이전 판에는 이 항목 자체가 없었습니다.

#### 보고된 오류

```
python: can't open file 'D:\Quarter Flying\scripts\editor\main.py': [Errno 2] No such file or directory
Error running editor
계속하려면 아무 키나 누르십시오 . . .
```

#### 원인: `scripts/start_editor.bat`의 상대 경로 버그

문제의 스크립트 전문(이동 전 `scripts/start_editor.bat`):

```bat
@echo off
cd /d "%~dp0\engine"     ← 버그 지점
python editor/main.py
if errorlevel 1 (
    echo Error running editor
    pause
)
```

- `%~dp0`는 **배치 파일 자신이 위치한 디렉토리**, 즉 `D:\Quarter Flying\scripts\`로 확장됩니다.
- 따라서 2번째 줄은 존재하지 않는 `D:\Quarter Flying\scripts\engine\`로 이동을 시도합니다.
- 대상 경로가 없으므로 `cd`가 실패하지만 스크립트는 중단되지 않고 계속 진행되며, 작업 디렉토리는 `scripts\`에 남습니다.
- 결과적으로 3번째 줄이 `scripts\editor\main.py`를 찾다가 실패합니다. 실제 파일은 `engine\editor\main.py`에 있습니다.
- `scripts/start_editor.sh`도 `cd "$(dirname "$0")/engine"`으로 **동일한 구조의 버그**를 가지고 있습니다.

올바른 경로는 `%~dp0..\engine`(한 단계 상위로 올라간 뒤 `engine`)이어야 합니다.

#### 이 스크립트가 "고칠 대상"이 아니라 "죽은 코드"인 이유

이 프로젝트는 CMake 빌드 과정에서 **정식 런처를 자동 생성**하도록 이미 구성되어 있습니다.

- 템플릿: `engine/scripts/run_editor.bat.in`, `engine/scripts/run_editor.sh.in` — **이 두 파일은 현역이므로 건드리지 않았습니다.**
- 생성 로직: [engine/CMakeLists.txt:184-205](engine/CMakeLists.txt#L184-L205) — `configure_file(...)`로 `@CMAKE_CURRENT_BINARY_DIR@`를 실제 경로로 치환한 뒤 `install(FILES ...)`로 배포.
- 생성 결과 확인: `build/run_editor.bat`에 `cd /d "D:/Quarter Flying/build"`로 **올바르게 생성되어 있음**을 실측 확인.

즉 `scripts/start_editor.*`는 이 CMake 런처 체계가 도입되기 전 손으로 작성한 **중복·구식 런처**이며, 이후 유지보수되지 않아 경로가 깨진 채 방치된 것입니다. 이 프로젝트가 "빌드해서 사용하는 개발자용 원본 소스"라는 성격을 고려하면, 수작업 런처는 애초에 정식 실행 경로가 아닙니다.

> ⚠️ **별개 이슈**: `build/run_editor.bat`는 경로 자체는 올바르지만 아직 실행되지 않습니다. `cmake --install`(설치 단계)까지 실행해야 생성되는 `build/editor/main.py`가 현재 존재하지 않기 때문입니다(`install(DIRECTORY editor/ ...)` 미실행). 이는 런처 버그와 무관한 빌드/설치 절차 문제이며, 이번 죽은 코드 분류 작업 범위 밖이라 손대지 않았습니다.

#### `scripts/restore_legacy.bat`, `.sh` — 실행 자체가 불가능한 스크립트

내용은 `git checkout HEAD -- app/main.cpp app/DemoScene.cpp`로 레거시 GLFW 메인을 복구하는 것입니다. 아래 **세 가지가 모두** 성립하지 않아 실행이 불가능합니다.

| 전제 조건 | 실제 상태 |
|---|---|
| `engine/app/` 디렉토리 존재 | ❌ 존재하지 않음 |
| Git 저장소 | ❌ `.git` 없음 — `git checkout` 실행 불가 |
| `cmake -DBUILD_LEGACY_MAIN=ON` 옵션 | ❌ `engine/CMakeLists.txt`에서 제거됨 |

#### `scripts/BACKUP_INFO.md`

위 `restore_legacy.*`의 사용 설명서입니다. 동일하게 존재하지 않는 `engine/app/`, 사용하지 않는 Git, 제거된 `BUILD_LEGACY_MAIN` 옵션을 전제로 작성되어 있어 함께 분류했습니다.

#### 올바른 실행 방법

```powershell
cd "D:\Quarter Flying\engine"
python editor/main.py
```

(CMake로 빌드 + `cmake --install`까지 마쳤다면 `build/run_editor.bat`도 사용 가능합니다.)

---

## 2. 레거시 코드 흔적

### 2.1 소스 코드 내 레거시 주석

**상태**: ✅ 대체로 정리됨
**확인 위치**: `engine/core`, `engine/renderer`, `engine/platform`, `engine/ecs`, `engine/job` 등 엔진 소스 코드

프로젝트 소스 코드 자체에서는 "legacy" 관련 주석이 남아있지 않음을 재확인했습니다. 다만 GLFW 기반 레거시 진입점의 흔적은 `engine/ARCHIVE_INFO.md` 문서와, 2차 개정에서 발견한 `restore_legacy.*` 스크립트(현재 `죽은코드/`로 이동)에 남아 있었습니다.

### 2.2 CMakeLists.txt 레거시 참조

**상태**: ✅ 정리됨
**위치**: `engine/CMakeLists.txt`

- GLFW 의존성 제거 완료, `ge_python`(Python 바인딩)과 Python 에디터가 유일한 실행 경로로 설치됨(`install(TARGETS ge_python ...)`, `install(DIRECTORY editor/ ...)`).
- `option(BUILD_LEGACY_MAIN ...)` 형태의 레거시 빌드 옵션은 현재 존재하지 않음(제거 완료). 단, 이 옵션을 사용하라고 안내하는 스크립트/문서가 `scripts/`에 남아 있었던 점은 1.4절 참고.

---

## 3. 오래된 주석 및 TODO/FIXME

### 3.1 프로젝트 소스 코드

**🔧 정정**: 이전 보고서는 "프로젝트 소스 코드에서 TODO/FIXME 주석 없음"이라고 기술했으나, 재검색 결과 **실제로는 1건 존재**합니다.

| 파일 | 위치 | 내용 |
|---|---|---|
| [engine/renderer/RenderGraph.h](engine/renderer/RenderGraph.h#L100) | 100줄 | `// TODO: Add barrier type and state information when GPU abstraction is implemented` |

이 외에 `engine/core`, `engine/platform`, `engine/ecs`, `engine/job` 등 나머지 소스 디렉토리에서는 TODO/FIXME/XXX/HACK이 발견되지 않았습니다. "없음"이 아니라 "1건, GPU 추상화 계층 관련 설계 부채"로 정정합니다.

### 3.2 외부 의존성 라이브러리

**상태**: ⚠️ 외부 라이브러리에 TODO/FIXME 다수 존재 (정상적인 현상)
**위치**: `build/deps/`, `engine/build_debug/deps/` 등 FetchContent로 받아온 서드파티 소스 트리

GLM, nlohmann-json, googletest, rapidcheck, pybind11 등 벤더링된 외부 라이브러리 내부의 TODO/FIXME는 해당 라이브러리 자체의 이슈이며, 이 프로젝트가 손댈 대상이 아닙니다.

---

## 4. C++ Python 바인딩 / 더미 폴백 구조

이 섹션은 최초 판(2026-07-29)에는 전혀 다루어지지 않았던 부분입니다. 실제 코드를 전수 조사한 결과, 이 구조는 "정리하면 그만인 죽은 레거시 코드"가 아니라 **현재도 에디터의 유일한 실행 경로를 지탱하는 활성 아키텍처**이며, 동시에 자체적인 기술 부채(중복·불일치)를 안고 있습니다.

### 4.1 구조 개요

- C++ 엔진은 pybind11로 `ge_python`이라는 확장 모듈로 빌드됩니다.
  - 등록 지점: [engine/bindings/PythonModule.cpp](engine/bindings/PythonModule.cpp) — `RegisterMathBindings`, `RegisterECSBindings`, `RegisterEngineBindings`, `RegisterEditorBindings` 4개 바인딩을 순서대로 등록.
  - 빌드 대상 정의: [engine/CMakeLists.txt:134](engine/CMakeLists.txt#L134) `pybind11_add_module(ge_python ...)`, 링크: [engine/CMakeLists.txt:141](engine/CMakeLists.txt#L141) `target_link_libraries(ge_python PRIVATE ge_engine)`.
  - 설치 경로: [engine/CMakeLists.txt:168-171](engine/CMakeLists.txt#L168-L171) — `install(TARGETS ge_python LIBRARY DESTINATION . RUNTIME DESTINATION .)`.
- Python 에디터([engine/editor](engine/editor))는 `ge_python`이 없어도(=미빌드/미설치 상태) 기동되도록 "더미 모드"를 각 파일마다 자체적으로 구현하고 있습니다.

### 4.2 실측 사실: `ge_python`은 현재 이 작업 공간에서 한 번도 빌드된 적이 없음

`build/`, `engine/build/`, `engine/build_debug/` 등 모든 빌드 산출물 트리를 검색했으나 `ge_python*.pyd`(Windows) 또는 `ge_python*.so`는 **어디에도 존재하지 않습니다.** `build/ge_python.vcxproj`처럼 Visual Studio 프로젝트 파일은 생성되어 있지만, 실제 컴파일된 확장 모듈 바이너리는 없습니다.

즉, 현재 상태에서 에디터를 실행하면 **예외 없이 항상 더미 폴백 경로만 실행됩니다.** 아래 4.3의 "실제 엔진 연동 경로"는 이 저장소 내에서 한 번도 검증되지 않은 코드입니다.

> 참고: 1.4절의 `build/editor/main.py` 부재(설치 단계 미실행)와 이 항목의 `ge_python` 바이너리 부재는 같은 원인 — **빌드/설치 파이프라인이 끝까지 실행된 적이 없음** — 에서 비롯된 것으로 보입니다.

### 4.3 더미 폴백이 파일마다 따로 구현되어 파편화됨

`ge_python` 임포트를 처리하는 방식이 파일마다 제각각입니다. 6개 파일에서 동일한 `try/except ImportError` 패턴이 독립적으로 중복 구현되어 있고, 그중 1개 파일은 가드 자체가 없습니다.

| 파일 | 처리 방식 | 비고 |
|---|---|---|
| [engine/editor/main.py:53-57](engine/editor/main.py#L53-L57) | `try/except ImportError` → `HAS_ENGINE` | `DummyViewport` 클래스([main.py:64-87](engine/editor/main.py#L64-L87))로 UI 대체 |
| [engine/editor/viewport.py:5](engine/editor/viewport.py#L5) | ⚠️ **가드 없음** — 모듈 최상단에서 `import ge_python`을 무조건 실행 | 이 파일 자체는 `ge_python` 없이 임포트 불가능. 안전망은 오직 `main.py`가 `from viewport import EngineViewport`를 try/except로 감싸주는 것([main.py:46-51](engine/editor/main.py#L46-L51))뿐 |
| [engine/editor/qt_engine_viewport.py:17-22](engine/editor/qt_engine_viewport.py#L17-L22) | `try/except ImportError` → `HAS_ENGINE` | 별도의 프로토타입용 뷰포트 위젯(Qt 임베딩 실험) |
| [engine/editor/panels/inspector.py:14-18](engine/editor/panels/inspector.py#L14-L18) | `try/except ImportError` → `HAS_ENGINE` | 함수 내부에서도 `import ge_python`을 반복 수행 (예: 178, 205, 249, 344줄) |
| [engine/editor/panels/scene_hierarchy.py:15-19](engine/editor/panels/scene_hierarchy.py#L15-L19) | `try/except ImportError` → `HAS_ENGINE` | 더미 엔티티 카운터(`_dummy_counter`)로 폴백 상태에서도 UI 조작 가능하게 처리 |
| [engine/editor/demo_scene_integration.py:11-15](engine/editor/demo_scene_integration.py#L11-L15) | `try/except ImportError` → `HAS_ENGINE` | `HAS_ENGINE`을 선언만 하고 이후 실제로 분기에 사용하지 않음(죽은 플래그) |

**문제점 요약**:
1. **가드 누락**: `viewport.py`는 스스로 안전하지 않은 모듈입니다. 지금은 유일한 호출자인 `main.py`가 방어해주고 있어 우연히 문제가 드러나지 않을 뿐, 다른 곳에서 직접 `import viewport`를 하면 즉시 `ImportError`로 죽습니다.
2. **중복 구현**: 동일한 `try: import ge_python; HAS_ENGINE = True / except ImportError: HAS_ENGINE = False` 패턴이 5곳에 복사되어 있어, `ge_python` API가 바뀌면 5곳을 모두 찾아 수정해야 합니다.
3. **죽은 플래그**: `demo_scene_integration.py`의 `HAS_ENGINE`은 선언 후 어디서도 참조되지 않습니다.
4. **부분적으로만 방어됨**: `panels/inspector.py`는 모듈 상단에서 `HAS_ENGINE`을 판정해놓고도, 함수 내부에서 다시 `import ge_python`/`from ge_python import Entity, Vec3`를 무가드로 호출하는 지점이 있습니다([inspector.py:205](engine/editor/panels/inspector.py#L205), [inspector.py:249](engine/editor/panels/inspector.py#L249)).

### 4.4 이미 존재하는 계획 문서와 실제 이행 상태의 괴리

이 파편화 문제는 이미 [docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md](docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md)에 Phase 1~6으로 계획되어 있습니다. 계획서와 실제 코드를 대조한 결과:

| Phase | 계획 내용 | 실제 이행 상태 |
|---|---|---|
| Phase 1: 기본 안정화 | `ge_python` 부재 시에도 에디터 로드 | 🟡 부분 완료 — `main.py`는 안전, `viewport.py`는 여전히 무가드 |
| Phase 2: 공통 래퍼 계층 (`engine_binding.py` 등) | `load_ge_python()`/`get_engine_runtime()`/`has_engine_runtime()` 헬퍼 도입 | 🔴 미착수 — `engine/editor/` 전체에 해당 파일/함수 없음(검색 결과 0건) |
| Phase 3: 실제 엔진 연동 경로 검증 | 빌드 후 `import ge_python` 성공 확인 | 🔴 미검증 — 4.2절대로 빌드 산출물 자체가 없음 |
| Phase 4: IDE/정적 분석 경고 완화 | `TYPE_CHECKING`/`Any` 타입 힌트 도입 | 🔴 미착수 |
| Phase 5: 명칭 정합성(`ge_*` → `Quarter Flying`) | 모듈/타겟명 통일 | 🔴 미착수 — `ge_python`, `ge_engine` 명칭 그대로 유지 중 |
| Phase 6: 회귀 테스트 | 더미 모드 import 테스트 추가 | 🔴 미착수 — [engine/editor/test_viewport_compatibility.py](engine/editor/test_viewport_compatibility.py)는 `ge_python`이 **있는** 경우만 검증 |

즉 계획 문서(P0-1)가 요구하는 "바인딩 유무와 상관없이 메인 에디터가 실행되도록 만든다"의 완료 조건은 `main.py` 진입점 기준으로는 충족되지만, `viewport.py`를 다른 진입점에서 직접 사용하면 깨집니다. Phase 1도 완전히 끝난 것으로 보기는 어렵습니다.

### 4.5 이 구조에 대한 정리 권장 사항

1. **즉시**: [engine/editor/viewport.py](engine/editor/viewport.py)의 5번째 줄 `import ge_python`을 나머지 5개 파일과 동일한 `try/except ImportError` 패턴으로 감싸, 이 파일 단독으로도 안전하게 임포트되도록 수정.
2. **단기**: 계획서 Phase 2를 실행하여 `engine/editor/engine_binding.py`(가칭) 공용 래퍼를 도입하고, 6개 파일에 흩어진 `try/except ImportError` + `HAS_ENGINE` 블록을 이 모듈로 단일화.
3. **단기**: `demo_scene_integration.py`의 미사용 `HAS_ENGINE` 플래그 제거 또는 실제 분기 로직에 연결.
4. **중기**: CI 또는 로컬 빌드 절차에 "`ge_python` 빌드 → `cmake --install` → `import ge_python` 성공" 검증 단계를 추가해 Phase 3을 최소 1회 실제 검증. 1.4절의 `build/editor/main.py` 부재 문제도 이 단계에서 함께 해소됩니다.
5. **중기**: [test_viewport_compatibility.py](engine/editor/test_viewport_compatibility.py)에 "`ge_python` 없을 때 `main.py`/`viewport.py`가 예외 없이 임포트되고 `DummyViewport`로 대체되는지" 검증하는 테스트를 추가(계획서 Phase 6).

---

## 5. 죽은코드 폴더 분류 내역 (2026-08-10 신설)

### 5.1 분류 방침

이 프로젝트는 **빌드해서 사용하는 개발자용 원본 소스**이므로, 수작업 Windows 배치 런처 등은 정식 실행 경로가 아닙니다. 다만 **삭제 여부는 사용자가 직접 판단하는 것이 안전**하다는 방침에 따라, 죽은 코드로 판단된 파일을 **삭제하지 않고 `죽은코드/` 폴더로 이동(분류)만** 했습니다. 원래 디렉토리 구조는 그대로 보존했습니다.

분류 근거와 사용자 결정 체크리스트는 [죽은코드/README.md](죽은코드/README.md)에 별도로 정리되어 있습니다.

### 5.2 이동 전/후 경로 매핑 (총 14개 파일)

| 이전 경로 | 새 경로 | 분류 사유 |
|---|---|---|
| `scripts/start_editor.bat` | `죽은코드/scripts/start_editor.bat` | 경로 버그(1.4절) + CMake 정식 런처와 중복 |
| `scripts/start_editor.sh` | `죽은코드/scripts/start_editor.sh` | 동일 구조의 경로 버그 |
| `scripts/restore_legacy.bat` | `죽은코드/scripts/restore_legacy.bat` | 전제 조건 3가지 모두 불성립 — 실행 불가 |
| `scripts/restore_legacy.sh` | `죽은코드/scripts/restore_legacy.sh` | 동일 |
| `scripts/BACKUP_INFO.md` | `죽은코드/scripts/BACKUP_INFO.md` | 위 restore 스크립트의 설명서 |
| `scripts/archive/qml_archive/Main.qml` | `죽은코드/scripts/archive/qml_archive/Main.qml` | QML 실험 종료, 참조하는 코드 없음 |
| `scripts/archive/qml_archive/Theme.qml` | 〃 | 〃 |
| `scripts/archive/qml_archive/SceneTreeItem.qml` | 〃 | 〃 |
| `scripts/archive/qml_archive/ViewportPanel.qml` | 〃 | 〃 |
| `scripts/archive/qml_archive/InspectorPanel.qml` | 〃 | 〃 |
| `scripts/archive/qml_archive/qml_launcher.py` | 〃 | 〃 |
| `scripts/archive/qml_archive/qml_engine_integration.py` | 〃 | 〃 |
| `scripts/archive/qml_archive/QML_MODERNIZATION_PLAN.md` | 〃 | 〃 |
| `scripts/archive/qml_archive/QML_MODERNIZATION_REPORT.md` | 〃 | 〃 |

**이동 결과**: `scripts/` 폴더는 현재 완전히 비어 있습니다(폴더 자체는 유지). 빈 채로 남은 `scripts/archive/`, `scripts/archive/qml_archive/` 하위 디렉토리는 제거했습니다.

### 5.3 이동하지 않은 것 (현역이므로 유지)

혼동을 막기 위해 명시합니다. 아래 파일은 이름이 비슷하지만 **죽은 코드가 아니며 그대로 두었습니다.**

| 파일 | 유지 이유 |
|---|---|
| `engine/scripts/run_editor.bat.in` | CMake `configure_file()`이 사용하는 **현역 런처 템플릿** |
| `engine/scripts/run_editor.sh.in` | 〃 (Linux/macOS용) |
| `build/run_editor.bat` | 위 템플릿으로 실제 생성된 산출물. 경로는 올바름 |

### 5.4 사용자 결정 대기 항목

- [ ] `죽은코드/scripts/start_editor.*` — 완전 삭제 / 경로(`%~dp0..\engine`)를 고쳐 `scripts/`로 복원 중 선택
- [ ] `죽은코드/scripts/restore_legacy.*`, `BACKUP_INFO.md` — 완전 삭제 / 향후 Git 도입 대비 보관
- [ ] `죽은코드/scripts/archive/qml_archive/` — 완전 삭제 / 향후 QML 전환 검토용 보관
- [ ] `engine/ARCHIVE_INFO.md`의 "QML 실험 정리 완료(삭제됨)" 기술을 실제 이력에 맞게 수정할지 여부 (1.1절 참고)

---

## 6. 정리 권장 사항

### 6.1 즉시 정리 권장

#### `viewport.py` 무가드 import 수정
4.5절 1번 항목 참고. `try/except` 한 블록 추가로 해결 가능한 실질적 버그입니다.

#### `engine/build/` 제거 (사실상 빈 캐시)
```powershell
Remove-Item -Recurse -Force "engine\build"
```
(`engine/build_debug/`는 1.2절 표에서 확인했듯 실제 산출물이 있으므로 **사용 여부 확인 없이 삭제하지 말 것**.)

#### 잔재 로그/메모 파일 정리
```powershell
# 내용 검토 후
Remove-Item "engine\build_out.txt","engine\build_out2.txt"
Remove-Item "새 텍스트 문서.txt"
```

### 6.2 ARCHIVE_INFO.md 업데이트

- **(2차 개정 추가)** "QML 실험 = 정리 완료(삭제됨)" 기술이 사실과 다릅니다. 실제로는 `scripts/archive/qml_archive/`로 이동되어 존재했고, 현재는 `죽은코드/`로 재분류되었습니다. 이력을 정확히 반영해야 합니다.
- Git 히스토리 복구 안내(`git checkout HEAD~N -- ...`) 앞에, 현재 이 프로젝트가 Git 저장소가 아니라는 전제 조건을 명시해야 합니다.
- "대형 테스트 결과 파일 제거"는 이미 완료되었으므로 "즉시 수행" 목록에서 제거.

### 6.3 라이센스 파일 정리

**현재 상태**: 프로젝트 루트에 `LICENSE` 파일이 확인되지 않음(이전 보고서는 "중복 존재"라고 기술했으나 오히려 부재 문제로 재확인 필요).

**권장 사항**:
- 루트에 `LICENSE` 파일 존재 여부를 확인하고, 없다면 추가.
- 의존성 라이브러리(`build/deps/*-src/LICENSE*`)의 라이센스 파일은 해당 라이브러리 관리에 맡기고 별도로 손대지 않음.

---

## 7. 유지 보수 필요 항목

### 7.1 모니터링 필요

- 외부 의존성 라이브러리 정기 업데이트 및 보안 패치 확인
- FetchContent 캐싱 전략 개선 (1.3절 참고 — 빌드 트리마다 의존성이 중복 다운로드됨)

### 7.2 문서화 필요

- 엔진 API 문서화 진행, 사용자 가이드 작성
- **실행 방법 문서화** — 1.4절의 혼선(수작업 런처 vs CMake 생성 런처)이 재발하지 않도록, `README.md`에 정식 빌드/설치/실행 절차를 명시할 것을 권장
- `docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md`의 Phase 2~6 실행 여부를 4.4절 표와 함께 주기적으로 재점검

### 7.3 테스트 강화

- 기존 테스트 커버리지 분석 및 케이스 추가
- CI/CD 파이프라인 구축 및 정적 분석 도구 통합
- 4.5절 5번: 더미 폴백 경로에 대한 회귀 테스트 추가

---

## 8. 요약

### 정리 상태

| 항목 | 상태 | 비고 |
|------|------|------|
| **실행 스크립트(런처)** | 🔴 **죽은 코드 — 분류 완료 (2차 개정 신규)** | `start_editor.bat`의 `%~dp0\engine` 경로 버그가 사용자 실행 오류의 직접 원인. CMake 정식 런처와 중복이라 `죽은코드/`로 이동 |
| **QML 아카이브** | 🔧 **위치 정정 (2차 개정)** | 이전 판의 "삭제 완료"는 오류 — `scripts/archive/qml_archive/`에 9개 파일 실재. 현재 `죽은코드/`로 재분류 |
| 아카이브 파일 (GLFW) | ✅ 정리 완료 | `engine/app/` 부재 실측 확인. 단 Git 저장소 부재로 히스토리 복구는 불가 |
| 레거시 코드 (CMake 등) | ✅ 정리 완료 | 소스/빌드 스크립트 본체에는 레거시 참조 없음 |
| TODO/FIXME | 🔧 1건 발견 | `RenderGraph.h:100` — 최초 판의 "0건" 기술은 오류 |
| baseline_test_results.txt | ✅ 이미 제거됨 | 최초 판의 "즉시 제거 필요" 권고는 이행 완료 |
| 빌드 아티팩트 | ⚠️ 정리 필요 | `engine/build/`(빈 캐시)는 삭제 가능, `engine/build_debug/`(390MB 실제 산출물)는 사용 여부 확인 후 판단 |
| 중복 의존성 | ⚠️ 모니터링 필요 | 빌드 트리마다 FetchContent 중복 다운로드 |
| C++ Python 바인딩/더미 폴백 구조 | 🔴 정리 필요 | 6개 파일 중복 가드, `viewport.py` 가드 누락, `ge_python`이 한 번도 빌드된 적 없음(더미 경로만 실전 검증됨) |
| 빌드/설치 파이프라인 | ⚠️ 미완주 | `cmake --install` 미실행 → `build/editor/main.py`, `ge_python.pyd` 모두 부재 |
| 외부 의존성 TODO/FIXME | ✅ 정상 | 서드파티 소스 내부만 해당, 조치 불필요 |

### 권장 작업 우선순위

1. **즉시**: `viewport.py` import 가드 추가(4.5-1), `engine/build/` 빈 캐시 제거(6.1), `죽은코드/` 분류 항목에 대한 최종 삭제/복원 결정(5.4)
2. **단기**: `engine_binding.py` 공용 래퍼 도입으로 6파일 중복 제거(4.5-2, 4.5-3), 잔재 로그/메모 파일 정리, `README.md`에 정식 실행 절차 명시(7.2)
3. **중기**: 빌드→설치→`import ge_python` 전 과정 검증을 CI/빌드 절차에 포함(4.5-4), 더미 폴백 회귀 테스트 추가(4.5-5), FetchContent 중복 다운로드 정리
4. **장기**: 문서화 강화, 테스트 커버리지 확대, CI/CD 파이프라인 구축

---

**최초 작성자**: Devin AI Assistant (2026-07-29)
**개정 작성자**: Claude (Sonnet 5) — 1차/2차 개정, 실제 프로젝트 재분석 후 정정 (2026-08-10)
**관련 문서**: [죽은코드/README.md](죽은코드/README.md) — 죽은 코드 분류 근거 및 사용자 결정 체크리스트
**다음 리뷰 권장일**: `docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md` Phase 2 착수 시점
