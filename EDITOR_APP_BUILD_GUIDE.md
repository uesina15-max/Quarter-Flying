# 완제품 에디터 빌드 가이드 (exe + DLL)

- **작성일**: 2026-10-08
- **대상**: 이 저장소를 받아 완제품 에디터를 직접 만들어 보려는 개발자
- **결과물**: Python·PySide6·Visual Studio가 **설치되지 않은 PC에서도** 더블클릭으로 실행되는 에디터 폴더

> 관련 문서: [GAME_BUILD_PLAN.md](GAME_BUILD_PLAN.md)의 5단계 제품 모델에서 "② 빌드된 완제품 에디터"를 만드는 절차다. 빌드 환경 메모는 저장소 루트의 `CLAUDE.md` "빌드 환경 메모"에 있다.

상태 표시는 다른 문서와 같다. **(실행 확인)** = 실제로 실행해 결과를 봤다, **(확인 필요)** = 아직 시험하지 않았다.

---

## 1. 무엇이 만들어지는가

```text
Quarter Flying/                 (약 171 MB)
├─ Quarter Flying.exe           에디터. Python 코드는 바이트코드로 exe 안에 들어 있다 (.py 파일 없음)
├─ _internal/                   python314.dll, Qt DLL, quarterflying_engine.dll, quarterflying.*.pyd
├─ assets/                      샘플 프로젝트 (프로젝트를 열지 않았을 때 쓰는 것)
└─ runtime/win-x64/             Player 템플릿 (Build Game이 복사해 간다)
```

- 소스 코드, 개발용 헤더·라이브러리(`include/`, `lib/`)는 들어가지 않는다.
- 이 폴더를 통째로 복사하면 어디서든 실행된다. 경로에 한글·공백이 있어도 된다 **(실행 확인)**.

---

## 2. 준비물

| 항목 | 사용한 버전 | 비고 |
|---|---|---|
| Windows | 11 x64 | |
| Visual Studio Build Tools 2022 | MSVC 14.44 | "C++를 사용한 데스크톱 개발" 워크로드 |
| CMake | 4.3.3 | PATH에 `C:\Program Files\CMake\bin` |
| Python | 3.14 (64비트) | 엔진 모듈(.pyd)이 이 버전으로 빌드된다. **3.14가 아니면 완제품의 .pyd를 불러오지 못한다** |
| Git | 아무 버전 | |
| 인터넷 | — | 처음 한 번: CMake가 의존 라이브러리를, 패키징 단계가 PyInstaller·PySide6를 내려받는다 |

PyInstaller와 PySide6는 **직접 설치하지 않아도 된다.** 패키징 단계가 빌드 폴더 안에 전용 venv를 만들어 `engine/packaging/requirements.txt`의 고정 버전(PyInstaller 6.21.0, PySide6 6.11.1)을 설치한다.

---

## 3. 빌드 절차

아래 예시는 PowerShell 기준이다. `<SRC>`는 저장소 폴더, `<BUILD>`는 빌드 폴더(아무 곳이나 가능, 예: `C:\QF_Build`)다.

### 3.1 소스 받기

```powershell
git clone https://github.com/uesina15-max/Quarter-Flying.git <SRC>
```

### 3.2 CMake 구성

```powershell
cmake -S <SRC>\engine -B <BUILD> -G "Visual Studio 17 2022" -A x64
```

- 약 1분. 마지막에 `Build files have been written to: <BUILD>`가 나오면 성공.
- Python이 여러 개 설치돼 있으면 3.14가 잡혔는지 출력의 `Found Python3: ... (found version "3.14.x")`로 확인한다.

### 3.3 엔진 빌드 (Release)

```powershell
cmake --build <BUILD> --config Release --parallel
```

- 반드시 `--config Release`. Debug는 `python3XX_d.lib`가 보통 없어서 링크 단계에서 실패한다.
- 경고(C4312 등)는 나와도 된다. 오류 없이 끝나면 성공.

### 3.4 완제품 만들기

```powershell
cmake --build <BUILD> --config Release --target editor_app
```

- 이 타깃이 Player 템플릿(`player_template`)도 함께 빌드한다.
- 처음에는 venv 생성과 패키지 설치로 몇 분 걸린다. 다음부터는 `requirements.txt`가 바뀌지 않았으면 설치를 건너뛴다.
- 마지막 줄 `[editor_app] 완료: <BUILD>\app\Quarter Flying (171 MB)`가 나오면 성공.
- MSBuild 출력에서 한글이 깨져 보일 수 있다. 콘솔 코드 페이지 문제일 뿐 결과에는 영향이 없다.

**결과물 위치: `<BUILD>\app\Quarter Flying\`** — 이 폴더를 원하는 곳(바탕화면 등)에 복사해서 쓴다.

### 3.5 (선택) 테스트

```powershell
cd <BUILD>
ctest -C Release --output-on-failure
```

- `SimpleEngineTests`, `player_template_smoke` 2개가 통과해야 한다 **(실행 확인)**.

---

## 4. 제대로 만들어졌는지 확인하기

"개발 PC에 설치된 Python 덕분에 실행된 것"이 아닌지 확인하려면, Python을 PATH와 환경 변수에서 빼고 실행한다.

```powershell
$env:PATH = "C:\Windows\System32;C:\Windows"
foreach ($v in "PYTHONPATH","PYTHONHOME") { [Environment]::SetEnvironmentVariable($v, $null, "Process") }
Start-Process "<복사한 위치>\Quarter Flying\Quarter Flying.exe" -WorkingDirectory "C:\Windows"
```

확인할 것:

1. **"Quarter Flying Editor — Sample — Untitled"** 창이 뜨고 Scene 뷰포트에 큐브 그리드·램프·피라미드가 그려진다.
2. 로그 파일 `%LOCALAPPDATA%\QuarterFlying\logs\editor.log`에 다음 줄들이 있다.
   ```text
   [DemoScene] Scene loaded: <복사한 위치>\Quarter Flying\assets\scene.json
   [Editor] 엔진 ECS 연결 완료
   [Editor] [SceneLoad] scene.json 엔티티 101/101개 생성
   ```
3. 로그에 `Traceback`, `ERROR`가 없다.
4. 한글 경로(예: `바탕 화면\테스트 폴더\Quarter Flying`)에 복사해서도 1~3이 같다.

2026-10-08에 위 1~4를 모두 확인했다 **(실행 확인)**. 단, Visual Studio·Python이 **한 번도 설치된 적 없는** 깨끗한 PC에서는 아직 시험하지 않았다 **(확인 필요)**.

---

## 5. 동작 원리

### 5.1 `editor_app` 타깃이 하는 일

```text
cmake --build . --target editor_app
  │
  ├─ 1. player_template · quarterflying · quarterflying_engine 빌드 (의존 타깃)
  ├─ 2. cmake --install → <BUILD>/app_stage          (기존 설치 규칙 그대로)
  └─ 3. engine/packaging/build_editor_app.py
         ├─ 전용 venv 준비: <BUILD>/packaging_venv   (requirements.txt 해시가 같으면 설치 생략)
         ├─ PyInstaller 실행: packaging/quarterflying_editor.spec
         │     입력 = app_stage/editor/main.py, 경로 = app_stage/editor + app_stage/bin
         ├─ assets/, runtime/win-x64/ 를 exe 옆에 복사
         └─ 결과 검사: exe·엔진 DLL·.pyd·Player 템플릿 존재, _internal에 .py 소스 0개, PyQt 섞임 없음
```

| 파일 | 역할 |
|---|---|
| `engine/CMakeLists.txt` (6번 절) | `editor_app` 타깃 정의. 기본 빌드(ALL)에는 들어 있지 않다 |
| `engine/packaging/requirements.txt` | 패키징 venv의 고정 버전 |
| `engine/packaging/quarterflying_editor.spec` | PyInstaller 설정 (창 모드, 이름 `Quarter Flying`) |
| `engine/packaging/build_editor_app.py` | 위 순서를 실행하는 스크립트 |
| `engine/editor/frozen_app.py` | exe로 실행될 때만 하는 시작 준비 (아래 5.3) |

**전용 venv를 쓰는 이유**: 개발 PC의 Python에는 PyQt6 같은 다른 패키지가 섞여 있을 수 있다. PyInstaller가 그것까지 끌어오면 크기가 커지거나 Qt 바인딩이 충돌한다. venv는 이 스크립트를 실행한 Python(= CMake의 `PYTHON_EXECUTABLE`, .pyd를 빌드한 Python)으로 만든다.

### 5.2 경로 규칙 — 왜 코드 경로 계산을 고치지 않았나

에디터의 경로 기준점은 모두 **"에디터 코드 폴더의 부모 = 설치 루트"** 규칙을 따른다.

- `editor/main.py`의 `_EDITOR_DIR` → 부모 폴더의 `assets/scene.json`
- `editor/project/project.py`의 `_ENGINE_DIR`
- `editor/gamebuild/game_builder.py` → 부모 폴더의 `runtime/win-x64`

PyInstaller 결과에서는 `_internal/`이 에디터 코드 폴더 자리에 온다. 그래서 `assets/`와 `runtime/win-x64/`를 exe 옆(= `_internal`의 부모)에 두면 개발 실행과 같은 규칙으로 맞는다.

### 5.3 `frozen_app.py`가 하는 일

`main.py`가 PySide6를 import하기 전에 `frozen_app.setup()`을 부른다. 개발 실행(`python main.py`)에서는 아무것도 하지 않는다.

1. **작업 폴더를 exe 폴더로 바꾼다** — `run_editor.bat`의 `cd /d "%~dp0"`과 같은 역할. `config.json`이 작업 폴더 기준이기 때문이다.
2. **출력을 로그 파일로 보낸다** — 창 모드 exe는 콘솔이 없다. Python `print`뿐 아니라 엔진 DLL(C++ Logger)의 출력도 남기려고 fd 1/2 자체를 `%LOCALAPPDATA%\QuarterFlying\logs\editor.log`로 돌린다. 실행할 때마다 새로 쓴다.
3. **게임 스크립트용 표준 라이브러리를 연결한다** — exe에는 에디터 코드가 쓰는 표준 모듈만 들어 있다. Play 모드에서 엔진 `ScriptSystem`이 게임 스크립트를 import할 때 다른 표준 모듈이 필요할 수 있으므로, Player 템플릿의 `python314.zip`·`DLLs/`를 `sys.path` 뒤쪽에 붙인다. 덕분에 에디터와 빌드된 게임이 같은 표준 라이브러리를 쓴다.
4. **처리되지 않은 예외** — 로그에 남기고, 처음 한 번은 오류 창으로 알린다.

### 5.4 엔진 로그 레벨

완제품은 출력이 전부 파일로 가므로, 엔진 기본 로그 레벨(Trace)에서는 렌더러가 매 프레임 여러 줄을 찍어 로그가 **시간당 약 250 MB**씩 커졌다. 그래서:

- 바인딩에 `quarterflying.set_log_level(LogLevel)` / `get_log_level()`을 추가했다 (`engine/bindings/EngineBindings.cpp`).
- 에디터는 시작할 때 **Info**로 낮춘다 (Player의 `PlayerMain.cpp`와 같은 기준). 프레임별 로그가 필요하면 환경 변수 `QF_LOG_LEVEL=trace`로 실행한다.
- 매 프레임 호출되는 `RenderGraph::Reset()`의 로그를 Info → Trace로 내렸다.

결과: 로그는 1초에 한 줄(FPS 표시) 수준이다 **(실행 확인)**.

---

## 6. 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| `[editor_app] 오류: 설치 결과(--stage)에 다음이 없습니다 ...` | 3.3 빌드가 안 됐거나 실패했다. 3.3부터 다시 |
| `Player template not staged` | `player_template` 빌드 실패. `cmake --build <BUILD> --config Release --target player_template` 출력 확인 |
| pip 설치 단계 실패 | 인터넷 연결 또는 프록시 확인. 실패 후에는 `<BUILD>\packaging_venv`를 지우고 다시 실행 |
| 완제품 실행 시 `SoundPlayer needs PySide6.QtMultimedia` | QtMultimedia는 `PySide6-Essentials`가 아니라 `PySide6-Addons`에 있다. `requirements.txt`에 Addons가 있는지 확인 |
| 엔진 모듈 import 실패 / `DLL load failed ... 애플리케이션 제어 정책에서 이 파일을 차단했습니다` | Windows Smart App Control이 새로 빌드한 .pyd를 막은 것. `CLAUDE.md` "빌드 환경 메모" 참고 (다시 링크하면 통과한 적 있음) |
| 완제품이 바로 꺼짐 | `%LOCALAPPDATA%\QuarterFlying\logs\editor.log`의 Traceback 확인 |
| 에디터에 새 서드파티 패키지를 추가했더니 exe에서만 ImportError | `engine/packaging/requirements.txt`에도 추가해야 venv에 설치되고 exe에 들어간다 |

---

## 7. 알려진 제한 (아직 안 한 것)

1. **`Program Files`에 설치하면 설정 저장이 실패한다** — 에디터가 `config.json`을 작업 폴더(= 앱 폴더)에 쓴다. 바탕화면처럼 쓰기 가능한 곳에서는 문제없다. 설치 프로그램을 만들기 전에 사용자 폴더(`%APPDATA%`)로 옮겨야 한다.
2. **완제품 안에서 Build Game을 실행해 보지 않았다** **(확인 필요)**.
3. **깨끗한 PC에서 시험하지 않았다** **(확인 필요)** — 4장 참고.
4. **설치 프로그램 없음** — 지금은 폴더 복사 방식(포터블).
5. **코드 서명 없음** — 인터넷으로 받은 exe는 다른 PC에서 SmartScreen 경고가 뜰 수 있다.
6. **아이콘 없음** — spec은 `--icon`을 받을 수 있지만 저장소에 .ico가 없어 기본 아이콘을 쓴다.

---

## 8. 작업 기록 (2026-10-08)

처음에는 `cmake --install` 결과에 전용 Python 폴더와 실행기 exe를 붙이는 방식으로 시제품을 만들었다. 실행은 됐지만 `.py` 소스와 표준 라이브러리가 파일 수천 개로 풀려 있고 개발용 파일(`include/`, `lib/`)이 섞여 있어, 일반적인 완제품(exe + DLL) 형태가 아니었다. 그래서 PyInstaller 방식으로 바꾸고 빌드 타깃으로 만들었다.

| 단계 | 결과 |
|---|---|
| CMake 구성 + Release 빌드 | 성공 (경고만) **(실행 확인)** |
| `ctest` (2개) | 모두 통과 **(실행 확인)** |
| `editor_app` | `app/Quarter Flying` 171 MB, `.py` 0개 **(실행 확인)** |
| Python 제거한 환경 + 한글 경로 + 작업 폴더 `C:\Windows`에서 실행 | 창 표시, 씬 렌더링, 오류 0건 **(실행 확인)** |
| 로그 크기 | 시간당 약 250 MB → 1초 1줄 수준 **(실행 확인)** |

---

> 실사용 격상 개선점(데모 시드·사운드·머티리얼, GUI·모션 에디터·게임 조작)은 이 가이드에서 분리했다. 현행 계획: [`docs/USABILITY_IMPROVEMENT_PLAN.md`](docs/USABILITY_IMPROVEMENT_PLAN.md). 원문: [`docs/archive/USABILITY_REVIEW_2026-10-08_ORIGINAL.md`](docs/archive/USABILITY_REVIEW_2026-10-08_ORIGINAL.md).
