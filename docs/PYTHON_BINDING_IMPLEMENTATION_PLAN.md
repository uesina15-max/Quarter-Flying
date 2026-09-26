# ge_python 바인딩/더미 모드 구현 계획서

> **참고**: 이 문서의 Phase 1~6은 [engine/editor/PHASE1_TECHNICAL_VERIFICATION.md](../engine/editor/PHASE1_TECHNICAL_VERIFICATION.md), [PHASE2_FUNCTIONAL_MIGRATION.md](../engine/editor/PHASE2_FUNCTIONAL_MIGRATION.md), [PHASE3_LEGACY_REMOVAL.md](../engine/editor/PHASE3_LEGACY_REMOVAL.md)에서 이미 사용 중인 "Phase 1/2/3"(GLFW → PySide6 GUI 통합)과는 **별개의 번호 체계**다. 두 문서 세트를 함께 참조할 때 혼동하지 않도록 주의한다.

> **개정 이력**: 1차 검토에서 "5개 파일이 무가드로 import한다"는 서술을 실제 코드 대비 정정했고, 2차 검토에서 (1) 임시 패턴을 거치지 않고 공통 래퍼로 바로 가는 순서, (2) 대상 파일 누락, (3) 바인딩 API 미확인 상태를 리스크가 아닌 선결 조건으로 재분류, (4) 더미 객체의 타입 계약(Protocol), (5) 이름 변경을 맨 뒤로, (6) 측정 가능한 검증 기준을 반영해 Phase 순서를 전면 재구성했다.

---

## 1. 배경

현재 에디터에서 `ge_python`을 참조하는 파일은 아래 6개다(테스트 파일 [test_viewport_compatibility.py](../engine/editor/test_viewport_compatibility.py) 제외).

| 파일 | 현재 가드 상태 |
|---|---|
| [engine/editor/viewport.py](../engine/editor/viewport.py) | ❌ 무가드 — 5행에서 `import ge_python`을 모듈 최상단에서 직접 수행 |
| [engine/editor/main.py](../engine/editor/main.py) | ✅ `try/except ImportError` + `HAS_ENGINE` |
| [engine/editor/panels/inspector.py](../engine/editor/panels/inspector.py) | ✅ 동일 패턴 (단, 함수 내부에서 `from ge_python import Entity, Vec3` 등 지역 import도 반복) |
| [engine/editor/panels/scene_hierarchy.py](../engine/editor/panels/scene_hierarchy.py) | ✅ 동일 패턴 |
| [engine/editor/qt_engine_viewport.py](../engine/editor/qt_engine_viewport.py) | ✅ 동일 패턴 |
| [engine/editor/demo_scene_integration.py](../engine/editor/demo_scene_integration.py) | ✅ 동일 패턴 |

`main.py`는 `from viewport import EngineViewport` 자체도 `try/except`로 감싸고 있어(`HAS_VIEWPORT`), `viewport.py`가 무가드 import로 실패하더라도 에디터 전체는 현재도 죽지 않고 더미 뷰포트로 폴백한다. 즉 "에디터 import 단계에서 전면 실패"는 현재 코드에서 재현되지 않는다.

실제로 남아 있는 문제는 다음과 같다.

- `viewport.py`만 무가드 상태라 나머지 5개 파일과 방식이 일관되지 않는다.
- 동일한 `try/except ge_python` 가드 로직이 5개 파일에 중복 구현되어 있고, 공통 래퍼(`engine_binding.py` 등)는 존재하지 않는다.
- 더미 객체와 실제 `ge_python` 객체가 같은 인터페이스를 지킨다는 보장이 코드 어디에도 없다 — 더미 모드에서만 통과하고 실제 모드에서 깨지는 회귀를 잡을 방법이 없다.
- IDE는 조건문 기반의 fallback 경로를 완전히 추적하지 못해 빨간 줄/노란 줄 경고를 발생시킨다.

**바인딩 빌드 상태(2026-08-10 Phase 1 실행 결과로 갱신)**: `engine/build_debug`에는 `ge_python.vcxproj`/`ge_python.dir`은 있었지만 실제 산출물(`.pyd`)은 없었고, 남아 있던 빌드 로그([build_out.txt](../engine/build_out.txt), [build_out2.txt](../engine/build_out2.txt))는 `cmake.exe`를 못 찾아 실패한 기록이었다. 이 문제 자체는 PATH 미설정이 원인이었고(CMake 4.3.3, VS Build Tools 2022/2026, Python 3.14 모두 이미 설치되어 있었음 — 별도 설치 불필요) PATH를 잡아 해결했다.

**그러나 PATH를 고친 뒤에도 빌드는 실패했다.** `ge_python`이 링크하는 `ge_engine` 라이브러리 자체가 **3건의 선존재 컴파일 버그**로 컴파일되지 않고 있었다 — 즉 `.pyd`가 없었던 진짜 이유는 툴체인 미설정이 아니라 **엔진 코어 소스 자체가 상당 기간(git 이력이 없어 정확한 시점은 알 수 없음) 빌드되지 않는 상태였다는 것**이다.

1. **CMake FetchContent 스코프 버그** — [engine/cmake/Dependencies.cmake](../engine/cmake/Dependencies.cmake)의 `CheckFetchContentDependency`가 `function()`으로 정의되어, 그 안에서 `FetchContent_MakeAvailable`이 설정하는 `glew_SOURCE_DIR`/`stb_SOURCE_DIR`가 함수 밖으로 전달되지 않음 → `GL/glew.h`, `stb_image.h`를 찾지 못해 7개 파일에서 `C1083` 에러. `PARENT_SCOPE`로 전파하도록 수정.
2. **`std::hash` 네임스페이스 오류** — [engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h)에서 `std::hash<InstancedBatchKey>` 특수화가 `namespace Engine { }` 안에 중첩되어 있어 MSVC가 `C2888`로 거부(표준상 `std::hash` 특수화는 `namespace std` 최상위에서만 가능). `namespace Engine`을 닫고 `namespace std { }`를 연 뒤 다시 여는 구조로 수정.
3. **`LODSystem.cpp`의 존재하지 않는 enum 값** — [engine/renderer/LODSystem.cpp](../engine/renderer/LODSystem.cpp) 3곳이 `EngineErrorCode::NotFound`(정의된 적 없는 이름)를 참조. 전체 코드베이스에서 이 이름을 쓰는 곳은 이 3곳뿐이었고, 이미 정의돼 있지만 아무 데서도 안 쓰이던 범용 폴백 `ResourceNotFound`로 교체(도메인별 값이 없는 "리소스를 못 찾음" 케이스에 부합, 의존하는 테스트 없음). 별개로 `GetCurrentLOD() const`가 non-const `FindInstance()`를 호출하던 const-정합성 문제는 const 오버로드를 추가해 해결(기존 non-const 호출부는 그대로 동작).

세 수정 모두 "빌드 통과를 위한 컴파일 에러 수정" 커밋으로 별도 분리했다(초기 커밋과 구분). 이 사실은 **`ge_python` 바인딩 계획과 무관하게, 엔진 코어가 이미 깨져 있었다는 근거 기록**으로 남긴다 — 이후 "언제부터 안 되던 건가"를 추적할 때 이 문서와 해당 커밋을 기준점으로 삼을 수 있다.

현재 저장소의 구조상, C++ 바인딩은 [engine/bindings](../engine/bindings)와 [engine/CMakeLists.txt](../engine/CMakeLists.txt)에서 빌드되며, Python 에디터는 [engine/editor](../engine/editor) 아래에서 동작한다. 따라서 이번 계획은 이 구조를 기준으로 작성한다.

---

## 2. 구현 목표

1. `ge_python` 바인딩이 없어도 에디터가 기본 UI 모드로 시작할 수 있다.
2. 바인딩이 빌드되어 설치되면 에디터가 실제 엔진과 연결되어 동작한다.
3. 더미 객체와 실제 바인딩 객체가 `typing.Protocol` 기반의 동일한 인터페이스를 따르도록 강제해, 더미 모드 통과가 곧 실제 모드 안전성을 보장하게 한다.
4. IDE/정적 분석 경고를 Any 남용이 아니라 Protocol 타입으로 줄인다.
5. 빌드, 설치, 실행 흐름을 문서화하고 테스트 가능한 상태로 만든다.

---

## 3. 현재 구조 기준 작업 범위

### 3.1 C++ 바인딩 빌드 확인 (선결 조건)

대상 파일
- [engine/CMakeLists.txt](../engine/CMakeLists.txt)
- [engine/bindings/PythonModule.cpp](../engine/bindings/PythonModule.cpp)
- [engine/build_debug](../engine/build_debug), [build_out.txt](../engine/build_out.txt), [build_out2.txt](../engine/build_out2.txt)

구현 내용
- cmake가 PATH에서 정상 실행되는 환경을 확보하고 `ge_python` 타겟을 실제로 빌드해 `.pyd` 산출물을 만든다.
- 빌드된 모듈을 `import ge_python` 후 `dir(ge_python)`으로 실제 노출된 심볼을 전수 확인한다.
- 위에서 확인한 실제 API를 3.2절 Protocol 정의의 입력으로 사용한다.

예상 결과
- "바인딩이 무엇을 노출하는지" 미확인 상태가 해소되고, 이후 모든 설계(Protocol, 래퍼)가 추측이 아닌 확인된 사실에 근거한다.

---

### 3.2 런타임 Protocol 정의

대상 파일 (신규)
- [engine/editor](../engine/editor) 아래에 `engine_protocol.py`(가칭) 신규 추가

구현 내용
- 3.1에서 확인한 실제 API와, 현재 6개 파일에서 이미 사용 중인 심볼(`Engine`, `EngineConfig`, `InputEvent`/`InputEventType`, `MouseButton`, `KeyCode`, `EditorAPI`, `CommandManager`, `PathResolver`, `ECSRegistry`/`Registry`, `Entity`, `Vec3`, `EngineError`)을 대조해 `typing.Protocol` 기반 인터페이스를 선언한다.
- 더미 구현과 실제 `ge_python` 바인딩이 **같은 Protocol을 만족**하도록 설계한다. `__getattr__`로 아무 속성이나 받아주는 만능 더미는 오타·시그니처 불일치를 삼키므로 사용하지 않는다 — 더미는 Protocol에 선언된 멤버만 명시적으로 구현한다.

예상 결과
- 더미 모드에서의 통과가 실제 모드에서의 시그니처 안전성도 보장한다.
- 정적 분석기가 Protocol을 기준으로 실제 `ge_python.*` 호출부의 타입을 검사할 수 있게 된다.

---

### 3.3 공통 래퍼 계층 도입

대상 파일
- [engine/editor](../engine/editor) 아래에 `engine_binding.py`(가칭) 신규 추가
- [engine/editor/main.py](../engine/editor/main.py)
- [engine/editor/viewport.py](../engine/editor/viewport.py)
- [engine/editor/panels/inspector.py](../engine/editor/panels/inspector.py)
- [engine/editor/panels/scene_hierarchy.py](../engine/editor/panels/scene_hierarchy.py)
- [engine/editor/qt_engine_viewport.py](../engine/editor/qt_engine_viewport.py)
- [engine/editor/demo_scene_integration.py](../engine/editor/demo_scene_integration.py)

구현 내용
- `load_ge_python()` / `get_engine_runtime()` / `has_engine_runtime()` 형태의 헬퍼를 구현하고, 반환 타입을 3.2절 Protocol로 명시한다.
- **`viewport.py`를 포함한 6개 파일 전체를 이 래퍼로 바로 전환한다.** `viewport.py`에 임시로 `try/except + HAS_ENGINE` 패턴을 먼저 적용했다가 다시 걷어내는 중간 단계는 두지 않는다 — 어차피 5개 파일의 중복 가드 로직 자체가 이 래퍼로 대체될 대상이므로, `viewport.py`만 별도 패턴으로 먼저 고칠 이유가 없다.
- `HAS_ENGINE`은 이 헬퍼의 결과로 계산되도록 통합한다.
- 래퍼가 실제 바인딩이 있는 환경에서는 실 객체를, 없는 환경에서는 3.2절 더미 구현을 반환하도록 만들어 "실제 엔진 연동 경로 연결"까지 이 단계에서 함께 확보한다.

예상 결과
- 모듈 부재 상태에서도 에디터가 import 오류 없이 로드된다.
- 6개 파일 모두 동일한 방식으로 엔진 바인딩 상태를 확인한다.
- 바인딩이 설치된 환경에서는 래퍼를 통해 실제 엔진 객체와 연결된다(구 "Phase 3: 실제 엔진 연동" 목표를 흡수).

---

### 3.4 더미 모드와 실제 모드의 경계 명확화 + IDE 경고 완화

대상 파일
- [engine/editor/main.py](../engine/editor/main.py)
- [engine/editor/viewport.py](../engine/editor/viewport.py)
- [engine/editor/panels/inspector.py](../engine/editor/panels/inspector.py)
- [engine/editor/panels/scene_hierarchy.py](../engine/editor/panels/scene_hierarchy.py)
- [engine/editor/qt_engine_viewport.py](../engine/editor/qt_engine_viewport.py)
- [engine/editor/demo_scene_integration.py](../engine/editor/demo_scene_integration.py)

구현 내용
- `HAS_ENGINE = False`일 때는 뷰포트가 더미 UI를 표시하고, `tick()`/마우스·키보드 이벤트는 no-op 처리한다.
- `HAS_ENGINE = True`일 때는 실제 엔진 초기화 경로로 진입한다.
- Protocol 타입을 사용하므로 `TYPE_CHECKING`/`Any` 땜질 없이 자연스럽게 타입이 좁혀진다. 남은 `Any`는 Protocol로 대체 가능한지 개별 검토한다.

예상 결과
- 에디터는 항상 실행 가능하며, 엔진 바인딩 유무에 따라 두 모드 중 하나로 동작한다.
- Pylance/pyright 경고가 정량적으로 감소한다(측정 기준은 3.6절 참고).

---

### 3.5 테스트 및 검증 절차 추가

대상 파일
- [engine/editor/test_viewport_compatibility.py](../engine/editor/test_viewport_compatibility.py)
- [engine/editor/test_demo_scene.py](../engine/editor/test_demo_scene.py)

구현 내용
- `ge_python` 모듈이 없는 상황에서 6개 파일 전부 import가 성공하는지 테스트한다.
- 더미 모드에서 주요 UI 클래스가 초기화되는지 확인한다.
- 실제 바인딩이 존재할 때는 엔진 모드 진입 경로가 호출되는지 확인한다.
- 더미 구현이 3.2절 Protocol을 실제로 만족하는지(예: `isinstance(dummy, EngineProtocol)` 또는 정적 분석 기반) 검증하는 테스트를 추가한다.

예상 결과
- 환경에 따라 실행 가능 여부를 자동으로 검증할 수 있다.
- 더미/실제 인터페이스가 어긋나는 회귀를 테스트가 잡아준다.

---

### 3.6 검증 기준의 측정 방법 (신설)

- pyright(또는 mypy) 설정 파일은 현재 저장소에 없다. 최초 실행 시 기본 설정으로 `engine/editor` 전체를 스캔해 **오류/경고 개수를 기준선으로 기록**한다.
- 이후 모든 Phase의 "IDE 경고 완화" 검증 기준은 "N개 → M개"처럼 기준선 대비 수치로 표기한다("크게 감소" 같은 정성적 표현은 사용하지 않는다).

---

### 3.7 프로젝트 이름 정합성 정리 (마지막 순서)

대상 파일
- [engine/CMakeLists.txt](../engine/CMakeLists.txt) (`project("Quarter Flying")`이지만 타겟명은 `ge_python`/`ge_engine`)
- [engine/bindings/PythonModule.cpp](../engine/bindings/PythonModule.cpp) (`PYBIND11_MODULE(ge_python, m)`)
- 6개 Python 파일의 `import ge_python` 전체

구현 내용
- `ge_engine`, `ge_python` 등 기존 이름이 남아 있는지 전역적으로 점검하고 `Quarter Flying` 기준으로 통일한다.
- **3.3절의 공통 래퍼가 도입된 뒤에 진행한다.** 래퍼가 있으면 `import ge_python`을 참조하는 지점이 래퍼 모듈 한 곳으로 줄어들어, 개명 시 고쳐야 할 지점이 6개 파일에서 1개 파일로 줄어든다.
- **3.5절의 회귀 테스트가 갖춰진 뒤에 진행한다.** CMake 타겟명, `PYBIND11_MODULE` 매크로 인자, Python import를 동시에 맞춰야 하는 전역 치환이라 회귀 테스트 없이는 위험도가 높고, 버전 관리(git)가 없는 현재 저장소 상태에서는 더더욱 그렇다.

예상 결과
- 프로젝트 명칭이 일관된 코드/문서 세트가 되며, 개명 작업 자체의 리스크는 래퍼+테스트 덕분에 최소화된다.

---

## 4. Phase별 작업 분해표

### Phase 1: 바인딩 빌드 확인 (선결 조건) — ✅ 완료 (2026-08-10)

**목표**
- `ge_python`이 실제로 무엇을 노출하는지 확인해, 이후 모든 설계의 근거를 마련한다.

**작업 항목**
- [x] cmake가 정상 실행되는 빌드 환경을 확보한다 — CMake 4.3.3 / VS Build Tools 2022 / Python 3.14는 이미 설치돼 있었고 PATH만 누락된 상태였음(재설치 불필요).
- [x] `ge_python` 타겟을 빌드해 `.pyd` 산출물을 생성한다 — `engine/build/Release/ge_python.cp314-win_amd64.pyd` 생성 확인. `--config Debug`는 `python3XX_d.lib` 부재로 링크 실패하므로 `--config Release` 사용.
- [x] `import ge_python; help(ge_python)`으로 실제 노출 심볼을 전수 확인하고 기록한다 — [ge_python_api_dump_20260810.txt](ge_python_api_dump_20260810.txt) (861줄).

**실제로는 "빌드 확인" 이상이 필요했다.** `.pyd`가 지금까지 없었던 진짜 이유는 툴체인 미설정이 아니라 **엔진 코어(특히 `renderer/`)가 오랫동안 컴파일된 적이 없어 보이는 상태**였기 때문이다. 실제로 고쳐야 했던 항목(전부 git 커밋으로 분리 기록됨, `86bd2f8`/`1b1682a`/`36ec29c`):
- CMake 스코프 버그(FetchContent 결과가 함수 밖으로 전파 안 됨), `std::hash` 네임스페이스 오류, `InstanceData` static_assert 미완성 타입/패딩 계산 오류, `SortBatchesForRendering` 중복 선언, `unique_ptr` 역참조 누락 2건(`CleanupStaleBatches`, `SortBatchesForRendering` 구현부)
- 존재하지 않는 `EngineErrorCode` 값 참조(`NotSupported`/`GPUResourceCreationFailed`는 enum에 추가, `NotFound`/`OutOfRange`는 기존 값으로 치환)
- `OpenGLResourceManager::CreateTexture` 헤더-구현 시그니처 불일치(죽은 선언 제거)
- `OcclusionCulling.cpp`의 GL API 오용 3건(`glEndQuery` 인자 누락, `resultAvailable` 타입, 존재하지 않는 GL 상수)
- `tinyobjloader`(서드파티) MSVC 19.44 constexpr 비호환 — 공식 우회 매크로(`TINYOBJLOADER_DISABLE_FAST_FLOAT`)로 해결, OBJ 로딩 기능은 그대로 유지(제외하지 않음)
- `GLEW_SOURCE_DIR`을 설정만 해두고 `add_subdirectory()`가 없어 `glew_s` 타겟이 아예 생성된 적이 없던 문제
- pybind11 바인딩 자체의 버그 2건: `EditorBindings.cpp`의 `ECSRegistry` 불완전 타입, `CommandManager`의 암묵적 삭제 복사 생성자 판정 시 MSVC eager-instantiation

**산출물**
- 실제 `ge_python` API 목록: [docs/ge_python_api_dump_20260810.txt](ge_python_api_dump_20260810.txt) (3.2절 Protocol 정의의 입력).
- 확인된 핵심 시그니처:
  - `Engine.InitializeFromWindowHandle(self, hwnd: int, config: EngineConfig) -> None`
  - `EngineConfig`: `windowWidth`, `windowHeight`, `windowTitle` 3개 필드만 노출(에디터 쪽 `config_manager.py`가 참조하는 `numWorkerThreads`/`logFrameInterval` 등은 **바인딩에 없음** — Protocol 설계 시 주의).
  - `InputEvent`: `type`, `keyCode`, `mouseButton`, `mouseX`, `mouseY`, `mouseDeltaX`, `mouseDeltaY`, `mouseWheelDelta`, `windowWidth`, `windowHeight` 10개 필드.
  - `CreateTexture`/`OpenGLResourceManager`/`TextureDescriptor`는 바인딩에 전혀 노출되지 않음 — 3.1절에서 시그니처를 4-인자로 정정한 변경은 Python API 표면에 영향 없음.
  - 최상위 노출 클래스: `Engine`, `EngineConfig`, `EngineError`, `InputEvent`, `InputEventType`, `MouseButton`, `KeyCode`, `Entity`, `Vec3`, `Quaternion`, `World`, `ECSRegistry`, `EditorAPI`, `CommandManager`, `TransformComponent`/`TransformComponentView`, `RenderableComponent`, `CameraComponent`, `ScriptComponent`.

**검증 기준**
- [x] `.pyd` 파일이 생성되고 `import ge_python`이 성공한다 — DLL 로드 실패 없음(`ge_engine.dll`이 같은 `Release/` 디렉터리에 있어 해결됨).
- [x] 노출된 심볼 목록이 문서화된다.

---

### Phase 2: 런타임 Protocol 정의

**목표**
- 더미와 실제 바인딩이 공통으로 만족할 `typing.Protocol` 인터페이스를 확정한다.

**작업 항목**
- [ ] Phase 1에서 확인한 API와 6개 파일의 기존 사용처를 대조해 필요한 멤버만 Protocol에 선언한다.
- [ ] `__getattr__` 기반 만능 더미를 금지하고, Protocol 멤버만 구현하는 명시적 더미 클래스를 작성한다.

**산출물**
- `engine_protocol.py`(가칭) — `EngineProtocol`, `InputEventProtocol` 등.

**검증 기준**
- 정적 분석기가 실제 `ge_python.*` 호출부와 더미 구현 양쪽에서 Protocol 위반을 잡아낸다.

**리스크**
- Phase 1이 지연되면 Protocol도 추측 기반으로 작성해야 하므로, Phase 1 완료 전에는 착수하지 않는다.

---

### Phase 3: 공통 래퍼 계층 도입

**목표**
- `ge_python` 접근 방식을 6개 파일에 분산시키지 않고 공통 래퍼로 통합하며, 이 과정에서 `viewport.py`도 함께 전환한다(중간 패턴 없이).

**작업 항목**
- [ ] [engine/editor](../engine/editor) 아래에 `engine_binding.py`를 추가하고 `load_ge_python()` / `get_engine_runtime()` / `has_engine_runtime()`을 Phase 2의 Protocol 타입으로 구현한다.
- [ ] [engine/editor/viewport.py](../engine/editor/viewport.py), [main.py](../engine/editor/main.py), [panels/inspector.py](../engine/editor/panels/inspector.py), [panels/scene_hierarchy.py](../engine/editor/panels/scene_hierarchy.py), [qt_engine_viewport.py](../engine/editor/qt_engine_viewport.py), [demo_scene_integration.py](../engine/editor/demo_scene_integration.py) 6개 파일 모두를 이 래퍼를 사용하도록 동시에 변경한다.
- [ ] 래퍼가 실제 바인딩 존재 시 실 객체를, 부재 시 Phase 2의 더미를 반환하는지 확인한다(= 실제 엔진 연동 경로 연결까지 이 단계에서 완료).

**산출물**
- 에디터 전반에서 동일한 방식으로 엔진 바인딩 상태를 확인할 수 있는 공통 모듈.

**검증 기준**
- 6개 파일 import 시 `ge_python` 부재 상황에서도 예외 없이 로드된다.
- 실제 바인딩이 있을 때 래퍼가 Protocol을 만족하는 실 객체를 반환한다.

**리스크**
- 여러 모듈 간 의존성 순환이 발생할 수 있다.

---

### Phase 4: 더미 모드 경계 명확화 + IDE 경고 완화

**목표**
- `HAS_ENGINE` 분기의 동작을 명확히 하고, IDE/정적 분석 경고를 측정 가능한 수치로 줄인다.

**작업 항목**
- [ ] 더미 모드에서 `tick()`, 마우스/키보드 이벤트 처리를 no-op으로 정리한다.
- [ ] 3.6절 방법으로 pyright/mypy 기준선을 기록한다.
- [ ] Protocol 타입 적용 후 재측정해 기준선 대비 감소량을 기록한다.

**산출물**
- 더미 모드와 실제 모드의 코드 경로가 분명히 구분된 코드.
- 기준선(N) 대비 재측정값(M) 기록.

**검증 기준**
- "IDE 경고가 N개 → M개로 감소했다"처럼 수치로 검증 가능하다.

**리스크**
- 기존 코드의 타입 힌트가 부족하면 리팩터링 범위가 넓어질 수 있다.

---

### Phase 5: 테스트 및 회귀 방지

**목표**
- 바인딩 유무 및 Protocol 준수 여부를 자동으로 검증한다.

**작업 항목**
- [ ] [test_viewport_compatibility.py](../engine/editor/test_viewport_compatibility.py)에 더미 모드 import 테스트를 추가한다.
- [ ] [test_demo_scene.py](../engine/editor/test_demo_scene.py) 또는 신규 테스트 파일에 엔진 상태 분기 테스트를 추가한다.
- [ ] 더미 구현이 Phase 2 Protocol을 만족하는지 검증하는 테스트를 추가한다.

**산출물**
- 환경 의존성 및 인터페이스 일치에 대한 회귀 테스트 세트.

**검증 기준**
- 테스트가 통과하면 향후 변경 시 바인딩/더미 모드 경계와 인터페이스 일치가 깨지지 않음을 확인할 수 있다.

**리스크**
- 실제 빌드 환경이 없으면 일부 테스트는 조건부로 skip 처리해야 할 수 있다.

---

### Phase 6: 프로젝트 이름 정합성 정리 (마지막) — ✅ 완료(2026-08-20)

**목표**
- 기존 코드/문서/모듈명이 `ge` 계열에서 `Quarter Flying` 계열로 바뀐 현재 구조와 일치하도록 이름을 정리한다.

**작업 항목**
- [x] `ge_engine`, `ge_python` 등 기존 이름이 남아 있는지 전역적으로 점검한다.
- [x] Phase 3의 래퍼 덕분에 좁혀진 참조 지점(래퍼 1개 파일 + CMake/PYBIND11_MODULE)만 수정한다.
- [x] 사용자-facing 문구와 코드 내 표시 이름을 `Quarter Flying` 기준으로 통일한다.
- [x] 이름 변경이 빌드/실행/배포에 영향을 주지 않는지 Phase 5의 회귀 테스트로 검증한다.

**산출물**
- 프로젝트 명칭이 일관된 코드/문서 세트.

**검증 기준**
- Phase 5 테스트가 개명 후에도 통과한다.
- 빌드 및 실행 경로에서 이름 혼선이 없다.

**리스크**
- 기존 스크립트나 외부 의존성이 구명칭을 참조하고 있을 수 있다. Phase 3/5 없이 이 Phase만 단독으로 먼저 진행하지 않는다.

**완료 기록**: `PYBIND11_MODULE(ge_python, m)` → `PYBIND11_MODULE(quarterflying, m)`(engine/bindings/PythonModule.cpp), CMake 타겟 `ge_engine`→`quarterflying_engine`/`ge_python`→`quarterflying`(engine/CMakeLists.txt, 딸린 링크/인클루드/설치 규칙 전부). 예상대로 실제로 고쳐야 했던 Python import 지점은 [engine/editor/engine_binding.py](../engine/editor/engine_binding.py) 한 곳뿐이었다 — Phase 3의 래퍼가 정확히 이 시나리오를 위해 만들어졌고 그대로 증명됐다. 사용자에게 보이는 문구(상태 바 메시지, 주요 독스트링)도 옛 이름을 일반화된 표현으로 정리했다. `SimpleEngineTests` 전체 스위트 410개 — 405 통과/5 실패(기존 무관 실패와 동일, 새 회귀 없음). 상세는 [ROADMAP.md](../ROADMAP.md) P1 Phase 6 참고.

---

## 5. 구현 우선순위표

| 순서 | Phase | 핵심 작업 | 기대 효과 | 난이도 |
|---|---|---|---|---|
| 1 | Phase 1 | 바인딩 빌드 및 API 확인 | 이후 설계의 근거 확보 | 낮음(환경 설정 제외) |
| 2 | Phase 2 | 런타임 Protocol 정의 | 더미/실제 인터페이스 일치 보장 | 중간 |
| 3 | Phase 3 | 공통 래퍼 계층 도입 (viewport.py 포함 6개 동시 전환) | `ge_python` 의존성 분산 문제 해소 + 실제 연동 경로 확보 | 중간 |
| 4 | Phase 4 | 모드 경계 명확화 + IDE 경고 완화(측정 가능) | 빨간 줄/노란 줄 정량적 감소 | 중간 |
| 5 | Phase 5 | 테스트 및 회귀 방지 | 변경 시 안정성 확보 | 중간 |
| 6 | Phase 6 | 프로젝트 이름 정합성 정리 | 코드/문서/모듈명 통일 (가장 마지막, 리스크 최소화된 상태에서 진행) | 중간~높음 |

---

## 6. 완료 기준

다음 조건을 모두 만족하면 구현 완료로 간주한다.

- `ge_python`이 없어도 에디터가 import 오류 없이 실행된다.
- 더미 모드에서 기본 UI가 동작하며, 더미 구현이 Protocol을 만족함이 테스트로 검증된다.
- 실제 바인딩이 있을 때 엔진 연동 경로로 전환된다.
- 6개 에디터 모듈 모두 공통 래퍼를 통해 `ge_python` 의존성 경계를 관리한다.
- IDE 경고가 기준선 대비 수치로 감소했음이 기록되어 있다.
- 문서와 실제 코드 구조가 일치한다.

---

## 7. 예상 리스크

- Windows 환경에서 Python 확장 모듈 로딩 경로가 다를 수 있다.
- PySide6 런타임 환경이 로컬에 없으면 UI 검증이 제한될 수 있다.
- 저장소에 버전 관리(git)가 없어 Phase 6(개명)처럼 되돌리기 어려운 전역 변경은 특히 신중히 진행해야 한다.

이러한 리스크는 단계별로 분리해 구현하면 관리 가능하다.
