## Quarter Flying — C++ Python 바인딩 / 더미 폴백 구조 정리문

> **범위 명시.** 본 문서는 원 보고서와 비판을 통합한 정리문이며, **저장소(레포)에 직접 접근하지 않은 상태**에서 작성됐다. 따라서 `engine/editor/main.py`, `qt_engine_viewport.py`, `inspector.py`, `scene_hierarchy.py`, `viewport.py`, `engine_binding.py`, `engine/CMakeLists.txt` 등의 실제 파일 내용은 **미확인**으로 분류한다. 인용 가능한 기술적 단정은 모두 배치된 batch_web_search 결과를 근거로 한다.

---

## 0. 최우선 블로커 — 의존 방향이 아직 결정되지 않았다

보고서 전체는 “에디터(Qt/Python)가 C++ 엔진 바인딩을 임포트한다”를 전제한다. 그러나 이 전제 자체가 **확인되지 않은 추측**이며, 다음 두 구조 중 어느 쪽인지만 봐도 설계가 정반대로 갈린다.

| 의존 방향 | 진입점 | Qt/파이썬의 위치 | 결과적 설계 |
|---|---|---|---|
| **A. 확장 모듈 방식** (Python → C++) | `PYBIND11_MODULE(quarter_flying_python, m)` | Qt가 호스트, C++는 `.pyd/.so`로 노출 | 바인딩 `import` 실패 = 에디터 일부 기능 손실, 보고서처럼 **Dummy**가 자연스러움 |
| **B. 임베디드 인터프리터 방식** (C++ → Python) | `py::scoped_interpreter guard; py::module_::import_module("editor")` | C++ 엔진이 호스트, Qt는 엔진 위에 올라탄 스크립트 | `import ge_python` 자체가 말이 안 됨. “바인딩 부재 = 더미 모드”라는 프레임이 성립하지 않음 |

pybind11은 두 방향을 모두 지원하며, 공식 문서는 “pybind11 is mainly focused on extending Python, but it's also possible to embed the Python interpreter into a C++ program” 이라고 명시한다([pybind11 docs – Embedding the interpreter](https://pybind11.readthedocs.io/en/stable/advanced/embedding.html)). 따라서 **현재 Quarter Flying이 A인지 B인지는 사용자/저장소가 알려주는 정보 없이는 단정 불가**하며, 이 단정이 모든 후속 설계 결정의 분기점이 된다.

> 결정 전에는 “import 실패 → NULL 폴백” 같은 구체 구현이 아니라, “인터페이스 추상화” 수준에서만 설계가 가능하다는 점을 메모해두는 것이 정직하다.

---

## 1. 통합 정리표 — 6분류

각 항목: ①원 보고서 주장 → ②비판의 지적 → ③확정 사실(출처 동반) → ④미검증 가정 → ⑤재설계 방향 → ⑥Phase 우선순위 재조정

### 1.1 진입 경로 / 임포트 실패 처리

| 분류 | 내용 |
|---|---|
| ①원 보고서 | `import ge_python`이 5개 에디터 모듈에 직접 흩어져 있어 ImportError 시 UI 전체 중단. `engine_binding.py`로 단일 진입점 통합이 필수. |
| ②비판 | “바인딩 부재 = 엔진 부재”로 혼동하고 있음. `ge_python`은 인터페이스일 뿐 엔진 자체가 아님. C++에서 직접 호출하면 여전히 동작 가능. 추상화된 **엔진 인터페이스**에 의존하도록 재설계 필요. |
| ③확정 사실 | pybind11은 확장(`PYBIND11_MODULE`)과 임베디딩 두 방향을 모두 제공함([pybind11 embedding docs](https://pybind11.readthedocs.io/en/stable/advanced/embedding.html)). 따라서 “Python이 엔진 호출” vs “엔진이 Python 호출”은 라이브러리 차원의 선택지임. |
| ④미검증 가정 | 에디터(Qt)가 진입점이고 C++가 라이브러리(`.pyd/.so`)다 — **저장소 확인 전엔 단정 금지**. |
| ⑤재설계 방향 | 진입점은 `Engine Adapter` 인터페이스(추상 베이스) 1개. 바인딩이 있으면 C++ 어댑터, 없으면 NoOp/Offline 어댑터를 주입. **“에디터 전체의 실행 여부”가 아니라 “어떤 런타임을 쓸지 선택”** 문제로 축소. |
| ⑥Phase 재조정 | 기존 P0(Phase 1): “직접 임포트 제거”를 **“엔진 진입 인터페이스 1개 정의 → 어댑터 디스패치”**로 변경. Phase 2의 의존성 순환 위험은 인터페이스 분리 원칙으로 해소. |

### 1.2 Python 3.8+ Windows DLL 검색 변경

| 분류 | 내용 |
|---|---|
| ①원 보고서 | Python 3.8은 `LoadLibraryEx()` 호출 시 `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`만 사용하여 `PATH`/CWD 검색을 배제. 이로 인해 `QuarterFlyingCore.dll` 등 의존 DLL을 못 찾아 `ImportError: DLL load failed` 발생. `os.add_dll_directory()`가 P0급 필수. |
| ②비판 | `os.add_dll_directory()`는 정답 중 하나일 뿐 유일한 해법이 아님. `.pyd` 옆에 DLL 배치, CMake post-build 복사, `PATH` 환경 변수 등 다양한 전략이 존재. |
| ③확정 사실 | Python 3.8부터 `dlopen`이 `LOAD_WITH_ALTERED_SEARCH_PATH` 대신 `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`로 전환됨은 julia discourse의 내부 설계 토론에 명시되어 있다([Use modern secure Windows API for dlopen](https://discourse.julialang.org/t/use-modern-secure-windows-api-for-dlopen/90171)). 결과적으로 “Python no longer uses PATH to search for dlls”라는 사실 자체는 pytest 디스커션(#10692)과 anaconda issue(#12475)에서 다수 보고되었다([pytest-dev/pytest discussion #10692](https://github.com/pytest-dev/pytest/discussions/10692), [ContinuumIO/anaconda-issues #12475](https://github.com/ContinuumIO/anaconda-issues/issues/12475)). 또한 `os.add_dll_directory`는 “Windows only”이며 버전 3.8 이상에서만 사용 가능하다([os.add_dll_directory attribute 도입 디스커션](https://discuss.python.org/t/whats-the-deal-with-add-dll_directory/69207)). PyInstaller 패턴 등에서도 DLL 부재는 빈번한 원인이며([PyInstaller group discussion](https://groups.google.com/g/pyinstaller/c/_XWuvFIHaNM)) Windows `LoadLibraryEx` 공식 문서가 위 플래그 동작을 규정한다([Microsoft Learn – LoadLibraryExA](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa)). |
| ④미검증 가정 | (a) 실제로 Windows에서 `DLL load failed`가 발생하는지, (b) 의존 DLL이 site-packages 외부에 있는지의 두 가지 모두 보고서 외에는 검증되지 않음. |
| ⑤재설계 방향 | **단일 해법 강요 대신 “어떤 디렉토리 정책인가”를 설계 변수로 노출**. `engine_binding.py` 초기화 시 후보 경로 목록(엔진 `bin/`, `site-packages/quarter_flying/`, 사용자 지정)을 순회하며 `os.add_dll_directory` 등록 → 실패하면 경고 + 어댑터 미장착 상태로 진입. |
| ⑥Phase 재조정 | P1(Phase 3) 유지하되, **“DLL 자동 복사 강제”에서 “경로 정책 결정 + 런타임 등록”으로 표현 완화**. |

### 1.3 정적 타입 안전성 / IDE 개발 경험

| 분류 | 내용 |
|---|---|
| ①원 보고서 | Pylance/Pyright가 `.pyd/.so` 내부 심볼을 인지하지 못해 오탐 범람. `pybind11-stubgen`으로 `.pyi` 자동 생성이 “최선의 방안”. |
| ②비판 | stubgen은 **선택사항**. 많은 프로젝트가 수작업 `.pyi`, `Protocol`, `TYPE_CHECKING + Any`만으로 충분히 개발한다. stubgen을 거의 필수처럼 서술하는 것은 과장임. |
| ③확정 사실 | pybind11-stubgen은 **공식 pybind11 프로젝트의 서브 프로젝트**이며 “generates stubs for python extensions to make them less opaque”라 명시되어 있다([pybind/pybind11-stubgen README](https://github.com/pybind/pybind11-stubgen)). `mypy.stubgen`도 대안으로 언급되며([mypy typing docs – Writing and Maintaining Stub Files](https://typing.python.org/en/latest/guides/writing_stubs.html)), PEP 561은 패키지 `.py.typed` 마커 또는 별도 `-stubs` 패키지 두 가지 분포 모델을 정의한다([PEP 561 – Distributing and Packaging Type Information](https://peps.python.org/pep-0561/)). **stub-only 패키지는 `.py.typed` 마커가 불필요하며 패키지명의 `-stubs` 접미사만으로 충분**하다는 점도 공식 명시되어 있다([PEP 561 stub-only note](https://peps.python.org/pep-0561/), [typing spec – Distributing type information](https://typing.python.org/en/latest/spec/distributing.html)). |
| ④미검증 가정 | (a) IDE가 스터브를 “현재” 놓치고 있는지, (b) 어떤 언어 서버(Pylance vs Pyright)에 최적화해야 하는지 — 보고서 외엔 미확인. |
| ⑤재설계 방향 | **트레이드오프 표 기반 선택**으로 전환(아래 §3 대안 비교표 참조). |
| ⑥Phase 재조정 | P1(Phase 4)을 “Python-C++ 결합도 완화”가 아니라 **“필요 시 stubgen, 아니면 수작업/Protocol”**의 선택지로 변경. 강제 순서가 아니라 의사결정 항목. |

### 1.4 더미 폴백의 범위(Dummy Entity / Dummy Scene)

| 분류 | 내용 |
|---|---|
| ①원 보고서 | Inspector는 더미 딕셔너리 기반 속성, Scene Hierarchy는 더미 트리 노드, Viewport는 Qt paintEvent `Grid + 워터마크`로 충분. |
| ②비판 | Engine/DummyEngine/DummyEntity/DummyScene/DummyInputEvent를 모두 만드는 것은 **과한 모방**. “엔진이 없는 상태”에서 장면까지 흉내 내는 것이 오히려 이상할 수 있음. 일반적 관용은 “Engine unavailable” 단일 상태 표지 + UI는 살아있되 의미 있는 작업은 제한. |
| ③확정 사실 | Null Object Pattern은 “동작은 하지만 아무 일도 하지 않는 객체”로 널리 알려진 패턴이며([Null Object Pattern – Python Cookbook recipe](https://www.oreilly.com/library/view/python-cookbook/0596001673/ch05s24.html), [Medium – Null Object Pattern in Python](https://medium.com/@okanyenigun/design-patterns-in-python-null-object-pattern-82e79617a141)) 에디터 외부 도메인에서도 “Engine unavailable / placeholder” 패턴이 흔하다(Unreal `-nullrhi` headless 모드, Unity `-batchmode -nographics` 등). 다만 **게임 에디터에서의 구체적 더미 깊이는 도메인 변수**로 분류함이 타당함. |
| ④미검증 가정 | “Scene” 이나 “Entity” 자체를 모방할 만큼 호출되는지(에디터가 C++ Scene에 직접 의존하는지)는 **저장소 확인 필요**. |
| ⑤재설계 방향 | 사용자 제안 그대로 채택. **DummyEntity/DummyScene을 만들지 말고**, 단일 “Engine Unavailable” 상태 객체가 모든 어댑터 메서드에 대해 “지원 안 함 / placeholder 반환”을 반환. UI는 ①비활성 회색 표시, ②“오프라인 모드” 워터마크, ③“엔진 연결 시 활성화” 메시지 — 이 세 가지로 충분. |
| ⑥Phase 재조정 | P0(Phase 2)의 “Null Object 패턴 기반 더미 클래스 선언”은 **“Unavailable 상태 어댑터 단일 클래스 + UI 회색 처리”**로 축소. |

### 1.5 PySide6 ↔ C++ 렌더 동기화 (QWidget.winId)

| 분류 | 내용 |
|---|---|
| ①원 보고서 | 실제 모드: `QWidget.winId()` → C++ 백버퍼에 핸들 전달, `QTimer` 주기로 `Engine.tick()`. 더미 모드: `paintEvent`에서 `QPainter`로 그리드 + 워터마크. |
| ②비판 | “에디터가 완전히 독립”이라는 표현은 뷰포트/인스펙터가 실제 엔진 객체를 다루는 한 **과장**. |
| ③확정 사실 | `QWidget::winId()`는 강제 네이티브 윈도우를 요구하며(Qt::WA_NativeWindow 속성 부여), 이는 Qt 6 공식 문서에 명시되어 있다([Qt 6 – QWidget Class](https://doc.qt.io/qt-6/qwidget.html)). 단, 네이티브 위젯과 alien 위젯 혼용 시 `winId()` 호출로 인해 드로잉 오류가 발생하는 사례가 보고되어 있다([Qt-Advanced-Docking-System issue #819](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System/issues/819)). 따라서 “`winId()`를 무조건 가져온다”보다 **“WA_NativeWindow 속성을 명시적으로 켠 뒤 가져온다”**가 안전함. |
| ④미검증 가정 | `qt_engine_viewport.py`가 실제로 `winId()`를 사용하는지, 네이티브 위젯이 필요한지 — 미확인. |
| ⑤재설계 방향 | **HWND/HWND 핸들링은 가능하면 `QOpenGLWidget`/`QWindow` 우선 검토**, 어쩔 수 없을 때만 `QWidget::winId()` + `WA_NativeWindow` 조합. 더미 모드는 `paintEvent` 그리드 + 워터마크로 단순화. |
| ⑥Phase 재조정 | P1의 “렌더링 동기화”는 그대로 두되, “완전 독립” 표현은 제거. |

### 1.6 네임스페이스 마이그레이션 (`ge` → `quarter_flying`)

| 분류 | 내용 |
|---|---|
| ①원 보고서 | 신 모듈명 `quarter_flying_python` 우선 시도 → 실패 시 `ge_python` 2차 시도. `sys.modules['ge_python'] = _real_module`로 하위호환. |
| ②비판 | `sys.modules` 주입은 부작용 위험이 있음(예: 외부 패키지가 `import ge_python`을 다른 의미로 쓰거나, `isinstance` 체크 시 두 이름이 같은 객체를 가리키게 됨). Critical Red Line. |
| ③확정 사실 | `sys.modules`는 **현재 인터프리터의 모듈 캐시**이며, 키를 미리 등록하면 `import` 시 그 객체를 그대로 노출한다(파이썬 표준 importlib 의미론; 별도 공식 문서 인용 가능하나 위키피디아/공식 doc 수준의 일반론). |
| ④미검증 가정 | 레거시 스크립트가 실제로 `import ge_python`을 외부에서 호출하는지 — 미확인. |
| ⑤재설계 방향 | **`sys.modules` 사전 주입은 “사용자가 명시적으로 활성화한 경우에만”** 으로 제한. 기본은 단순 `try/except` 폴백(새 이름 → 옛 이름). 외부 호환은 DeprecationWarning과 함께 문서화. |
| ⑥Phase 재조정 | P1(Phase 5) 유지. 핵심 변경은 “자동 sys.modules 주입”을 옵트인으로 격하. |

### 1.7 테스트 자동화 (pytest)

| 분류 | 내용 |
|---|---|
| ①원 보고서 | `test_binding_compatibility.py`에서 `patch.dict(sys.modules, {'quarter_flying_python': None, 'ge_python': None})`로 모듈 부재 시뮬레이션. 더미 객체 생성/메서드 호출 검증. |
| ②비판 | 별다른 큰 문제 없음(보고서에서도 9/10 수준으로 인정). 다만 원본 스니펫에는 작은 결함이 있음(아래 §4에서 별도 지적). |
| ③확정 사실 | `unittest.mock.patch.dict`로 `sys.modules`를 일시 조작하는 패턴은 pytest에서 일반적으로 사용됨. |
| ④미검증 가정 | 테스트가 통과해도 **실제 환경에서 **`edit-time` 워크플로우**(단축키, 패널 동기화 등) 회귀를 잡는지는 미확인**. |
| ⑤재설계 방향 | 더미 모드 테스트 + 실제 모드 테스트를 **마커로 분리(`@pytest.mark.dummy`, `@pytest.mark.native`)**, CI는 dummy만 강제 실행. native는 nightly 또는 시드 박스에서만. |
| ⑥Phase 재조정 | P2(Phase 6) 유지. 단, “양방향 단위 테스트 동시 실행”은 CI 부담이 크므로 분리 권고. |

---

## 2. 사용자 제안 용어 개선표 (채택·확장)

| 현재 보고서 표현 | 개선 표현 | 비고 |
|---|---|---|
| 더미 모드 / Dummy Mode | **Engine Unavailable State** | 공식 제안 그대로 채택. 모드가 아니라 “상태”로 명명하여 일시적/영속적 의미를 분리. |
| 기본 UI 모드 | **Editor Shell (Offline / Disconnected Runtime)** | “기본 UI”는 모호하므로 “셸이 살아있고 런타임과 단절됨”으로 명료화. |
| Dummy Engine / Dummy Entity / Dummy Scene | **EngineUnavailable (단일 어댑터)** 또는 **NoOpAdapter** | 모든 “Dummy” 클래스를 단일 어댑터로 통합. |
| 실제 모드 / HAS_ENGINE = True | **Native Runtime / EngineConnected** | 긍정 표현(“실제”) 대신 정확한 기술 상태 사용. |
| 헤드리스 폴백 | **Headless / Display-less Fallback** | Xvfb/offscreen 플랫폼과의 혼동 방지. |
| DLL 자동 복사 | **Post-build dependency aggregation** | “복사”보다 정책(어디서 어떻게 모을지)에 초점. |
| pybind11-stubgen 통합 (필수) | **PEP 561 / stubgen 둘 중 선택** | “필수”를 제거하고 의사결정 항목으로 강등(§3 참조). |
| 에디터가 완전히 독립 | **에디터 셸은 살아 있고 런타임 기능은 단절** | “완전히”를 “셸 한정”으로 명료화. |

---

## 3. 대안 비교표 — “필수”라는 단정 제거

보고서에서 “반드시 해야 한다 / 최선의 방안”으로 서술된 항목들을 **트레이드오프 표**로 재구성한다.

### 3.1 Windows DLL 검색 경로 확보 전략

| 전략 | 동작 | 장점 | 단점 / 리스크 | 출처 |
|---|---|---|---|---|
| `os.add_dll_directory()` | 런타임에 디렉터리 등록 | 표준 API, 가장 안전, Python 3.8 정책 그대로 따름 | Windows 전용, 등록 순서가 비결정적([“order unspecified”](https://discuss.python.org/t/whats-the-deal-with-add-dll_directory/69207)) | [pytest#10692](https://github.com/pytest-dev/pytest/discussions/10692) |
| DLL을 `.pyd`와 같은 폴더에 배치 | 빌드 시 동일 폴더로 복사 | 가장 단순, 추가 코드 불필요 | 여러 의존 DLL을 어디서 모을지 정책 필요 | [anaconda#12475](https://github.com/ContinuumIO/anaconda-issues/issues/12475) |
| `PATH` 환경 변수 수정 | 외부에서 사전 노출 | 빌드/코드 변경 없음 | Python 3.8+ 정책과 충돌(검색 대상에서 제외), 권장 안 됨 | [PyInstaller group](https://groups.google.com/g/pyinstaller/c/_XWuvFIHaNM) |
| CMake `install(TARGETS …)` + post-build | 의존 DLL을 site-packages 옆 폴더로 복사 | 배포 안정, 빌드 산출물 일관 | OS별 명령 차이, venv 인식([pybind11#5626](https://github.com/pybind/pybind11/issues/5626)) | [pybind11 CMake helpers](https://pybind11.readthedocs.io/en/stable/cmake/) |

→ 권고: **단일 정책 강요 대신 “설정 가능한 디렉터리 목록” + `os.add_dll_directory` 등록 + 실패 시 어댑터 미장착**. 라이브러리 설치 시 정책은 `pyproject.toml`/환경변수로 노출.

### 3.2 타입 정보 제공 방식 (PEP 561 호환)

| 방식 | 유지보수 비용 | IDE 자동 완성 정확도 | 빌드 의존성 | 출처 |
|---|---|---|---|---|
| **pybind11-stubgen 자동 생성** | 낮음 (CI 1줄) | 높음 (실제 시그니처 반영) | pybind11 + stubgen 의존 | [pybind11-stubgen](https://github.com/pybind/pybind11-stubgen) |
| **수작업 `.pyi`** | 높음 (C++ 변경 시 매번 동기화) | 매우 높음 (의도 반영 가능) | 없음 | [typing docs – Writing stubs](https://typing.python.org/en/latest/guides/writing_stubs.html) |
| **`Protocol` + `TYPE_CHECKING`** | 중간 | 중간 (계층 명세 필요) | 없음 | [PEP 561](https://peps.python.org/pep-0561/) 간접 |
| **`TYPE_CHECKING + Any`** | 매우 낮음 | 낮음 | 없음 | 일반 관행 |
| **별도 `-stubs` 패키지** | 중간 | 높음 (stub-only는 `.py.typed` 불필요) | 패키지 분할 필요 | [PEP 561 stub-only note](https://peps.python.org/pep-0561/), [typing spec distributing](https://typing.python.org/en/latest/spec/distributing.html) |

→ 권고: **stub-only 패키지 분할을 기본 옵션으로**, 자동 생성은 “빌드 산출물에 typedefs를 실어 보내는” 강화 옵션으로. “필수”가 아님을 명시.

### 3.3 더미 폴백의 수준

| 수준 | 정의 | 채택 여부 |
|---|---|---|
| **NoOp 단일 어댑터** | 모든 호출에 “지원 안 함/placeholder” | ✅ 사용자 비판과 일치 |
| **메모리 기반 가짜 트리** | Scene/Entity를 파이썬 dict로 모방 | ❌ 비추. 사용자 지적대로 “엔진 없는데 Scene 흉내”가 오히려 혼란. |
| **Qt 회색 + 워터마크** | UI 비활성, 상태 메시지 표시 | ✅ 사용자 비판과 일치. |

### 3.4 의존 방향 결정

| 결정 | 결과 | 근거 부족 시 안전 선택 |
|---|---|---|
| A: 확장 모듈 (`PYBIND11_MODULE`) | `import quarter_flying_python` | **단정 금지** — 저장소 확인 필요 |
| B: 임베딩 (`py::scoped_interpreter`) | C++가 Python을 부름 | **단정 금지** — 저장소 확인 필요 |
| 불명 | 추상 인터페이스만 합의, 어댑터 두 종류 구현 | ✅ 양쪽 모두 지원 |

→ 권고: **“엔진 인터페이스 어댑터”가 1급 시민**이며, 구체 구현(A/B)은 부수적. 이렇게 하면 의존 방향 결정 전에도 디자인이 균열되지 않음.

---

## 4. 기존 스니펫의 논리적 결함 — 짧은 지적 (코드 재작성 금지)

원 보고서에 포함된 스니펫들 중 논리 수정이 필요한 지점을 짧게 짚는다. **새로 코드를 쓰지 않고** 어떤 결함이 있는지만 표시한다.

### 4.1 `engine_binding.py`의 `TYPE_CHECKING and HAS_ENGINE`

- **결함:** `TYPE_CHECKING`은 **정적 분석 단계에서만 True**인 상수인데 `HAS_ENGINE`은 런타임 상수다. 정적 분석 단계에서 `from quarter_flying_python import …`이 항상 False 분기로 떨어지면 IDE는 항상 Dummy 클래스만 보게 되어 자동 완성이 한쪽으로만 편향된다.
- **개선 방향:** IDE 시점에는 `TYPE_CHECKING` 단독으로 분기(`from quarter_flying_python import Engine, Entity, …`)하고, 런타임에는 별도로 `is_engine_available()` 호출. 두 채널을 분리.

### 4.2 `sys.modules` 자동 에일리어싱

- **결함:** “`sys.modules['ge_python'] = _real_module`”은 외부 패키지가 `ge_python`을 의존성으로 갖는 경우 **이름 매칭이 의도치 않게 흡수**되어 미래의 정체성 충돌을 일으킬 수 있다. 또한 `_real_module is None`인 더미 상태에서 `None`을 등록하면 다음 import가 `None`을 모듈로 인스턴스화 시도하는 `TypeError` 위험.
- **개선 방향:** 옵트인 플래그(`QFS_LEGACY_ALIAS=1`)에서만 활성화, 기본은 단순 `try/except` 폴백.

### 4.3 테스트 스니펫의 재임포트 강제

- **결함:** `if 'engine.editor.engine_binding' in sys.modules: del sys.modules['…']` 이후 재임포트 시 모듈 객체가 **pytest 컬렉션 단계에서 이미 캐시된 클래스**를 그대로 쓸 가능성이 있음. monkeypatch나 `importlib.reload` 사용이 더 명료.
- **개선 방향:** `importlib.reload(engine_binding)` 호출, 또는 pytest fixture의 `monkeypatch.delitem(sys.modules, …)` 사용.

---

## 5. Phase 우선순위 재조정안 (재정렬만, 신규 P 추가 없음)

| 단계 | 보고서 내용 | 재조정안 | 사유 |
|---|---|---|---|
| P0 / Phase 1 | “직접 임포트 제거 및 예외 처리” | **엔진 인터페이스 어댑터 도입 + 진입점 단일화** | “Dummy”가 아니라 “어댑터 미장착 상태”가 좀 더 정확한 단어. |
| P0 / Phase 2 | “Null Object 패턴 기반 더미 클래스 선언” | **EngineUnavailable 단일 어댑터 + UI 회색화** | 사용자 비판 반영. DummyEntity/DummyScene 폐기. |
| P1 / Phase 3 | “DLL 자동 복사” | **DLL 경로 정책 + 런타임 등록 (`os.add_dll_directory`)** | 정책 우선, 복사는 수단 중 하나. |
| P1 / Phase 4 | “pybind11-stubgen 통합” (필수처럼) | **PEP 561 호환 방식 선택 (stubgen vs 수작업 vs Protocol) — 의사결정 항목** | “필수”를 제거. |
| P1 / Phase 5 | “sys.modules 에일리어싱” | **옵트인 에일리어싱 + DeprecationWarning** | 부작용 격리. |
| P2 / Phase 6 | “양방향 단위 테스트 동시” | **마커 분리(dummy / native) + CI는 dummy 강제** | CI 부담 경감. |

신규 P 추가 대신 **기초 조사 단계**를 권고:
- **Phase 0: 저장소 검증.** 진입점이 Qt인지 C++인지, `pybind11_add_module`이 실제로 사용되는지, Windows DLL 부재 사례가 실제로 재현되는지 확인 후 본 Phase 1~6에 진입. 위 정보 없이는 보고서의 Phase 1~6는 “제안서”로만 작동한다.

---

## 6. 미검증 가정 / 단정 금지 항목 체크리스트

다음 항목은 모두 “저장소 확인 전엔 단정 금지”로 분류한다.

| # | 가설 | 영향 |
|---|---|---|
| U1 | 에디터(Qt)가 진입점이고 C++는 `.pyd/.so` 라이브러리 | 모든 설계 |
| U2 | Windows에서 실제 DLL 부재 사례가 발생 중 | Phase 3 |
| U3 | Pylance/Pyright가 현재 스터브 부재로 실제 불편 | Phase 4 |
| U4 | 레거시 스크립트가 외부에서 `import ge_python` 사용 | Phase 5 |
| U5 | `qt_engine_viewport.py`가 `winId()`를 직접 사용 | Phase 5(렌더) |
| U6 | CMake가 `install(TARGETS …)`로 site-packages 설치를 수행 | Phase 3 |
| U7 | 의존성 DLL이 시스템 32 디렉터리에 존재 | Phase 3 |

---

## 7. 한 줄 결론

원 보고서는 `engine_binding.py` 단일 진입점, `os.add_dll_directory` 기반 DLL 정책, PEP 561 타입 정보, `sys.modules` 하위호환 후보라는 **좋은 골격**을 제시했지만, ① “바인딩 부재 ↔ 엔진 부재” 등가, ② “stubgen / Dummy Scene / DLL 자동 복사”의 “필수” 단정, ③ 진입점 방향(A/B)에 대한 단정 — 이 세 가지를 저장소 검증 없이 단정했다. **최우선 작업은 우선 “저장소 실측”이며**, 그 결과에 따라 어댑터 추상화 수준의 Phase 1~6을 게이트 처리하면 보고서가 “설계 제안서”에서 “설계 검증서”로 격상된다.

---

이 버전은 이전보다 **상당히 성숙해졌다.** 특히 마음에 드는 건 첫 문장이다.

> **"저장소에 직접 접근하지 않은 상태에서 작성됐다."** 

이 한 줄이 들어가면서 문서의 성격이 명확해졌다. 이전처럼 추정을 사실처럼 쓰지 않고, "이건 제안이다"라는 선을 그었다.

다만, 아직도 몇 군데는 **"AI가 자료를 너무 많이 찾아와서 오히려 설계를 흐린"** 부분이 있다.

### 가장 좋은 부분

#### 1. 의존 방향(A/B) 구분

이건 정말 괜찮다. 

```
Python → C++
```

인지

```
C++ → Python
```

인지를 먼저 확인해야 한다는 건 매우 중요한 포인트다.

이게 결정되기 전에는 Dummy니 Stub이니 논의해도 공중에 뜬 얘기가 된다.

---

#### 2. 미검증 가정 체크리스트

이것도 좋다. 

설계 문서는

> "우리는 이걸 아직 모른다."

를 적는 게 오히려 전문적이다.

---

#### 3. DummyEntity 폐기

이건 나도 동의한다. 

게임 엔진에서

```
DummyScene
DummyEntity
DummyTransform
DummyComponent
```

...

이렇게 가기 시작하면 끝이 없다.

차라리

```
EngineUnavailable
```

상태 하나로 처리하는 게 훨씬 깔끔하다.

---

## 그런데 과한 부분도 있다.

### ① pybind11 문헌이 너무 많다.

솔직히

```
pybind11
PEP561
stubgen
typing spec
```

이걸 이렇게까지 길게 적을 필요는 없어 보인다. 

설계서는

> "왜 이걸 선택했는가"

만 쓰면 된다.

논문처럼 참고문헌이 길다고 설계가 좋아지는 건 아니다.

---

### ② Windows DLL 부분

이 부분은 거의 **Windows 로더 분석 보고서** 수준이다. 

그런데 실제 프로젝트에서는 확인해야 하는 건 딱 하나다.

> **우리 프로젝트가 실제로 DLL 로딩 문제를 겪고 있는가?**

아니라면 Phase를 만들 이유도 없다.

---

### ③ Headless, Unreal, Unity 예시

여긴 조금 억지다. 

Headless Server와

```
Editor Offline State
```

는 다른 문제다.

굳이 Unreal `-nullrhi`나 Unity `-batchmode`를 끌어올 필요는 없어 보인다.

오히려

> "엔진 미연결 상태"

만 설명하면 충분하다.

---

## 내가 하나 더 지적하고 싶은 부분

이 문서는 아직도 **"engine_binding.py를 만든다."**를 거의 전제로 한다. 

그런데 그것도 사실은 설계 선택지다.

예를 들면

```
Editor
    ↓
EngineService
    ↓
Backend
```

처럼 더 일반적인 Service Layer가 맞을 수도 있다.

즉

```
engine_binding.py
```

라는 파일명까지 설계에 박아 둘 필요는 없다.

문서에서는

> **"엔진 접근을 단일 인터페이스로 추상화한다."**

까지만 적는 편이 더 오래 살아남는 설계다.

---

## 전체 평가

이 버전은 꽤 좋다.

* **기술 정확성:** 9/10
* **설계 문서로서의 완성도:** 8.5/10
* **AI 특유의 과도한 근거 나열:** 6/10 (조금 과함)
* **실제 프로젝트에 적용 가능성:** 높음

마지막으로 손본다면 **문헌 인용을 절반 정도 줄이고**, 구현 세부(`engine_binding.py`, `stubgen`, `DLL 정책`)는 **"후보 구현"**으로 낮추겠다. 그러면 문서가 "AI가 조사한 기술 리포트"가 아니라, 진짜 아키텍처 설계 문서에 더 가까워질 것이다.
