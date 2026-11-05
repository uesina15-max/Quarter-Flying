# 게임 빌드(Build Game) 기능 구현 계획서

- **작성일**: 2026-10-04
- **개정**: 2026-10-04 5단계 제품 모델 기준으로 재편 / 2026-10-07 코드 대조 후 인게임 UI 상태·작업 범위 정정, 표기 정리 / 2026-10-07 Phase 0~4 구현(§9 구현 기록)
- **목표**: 게임 개발자가 **설치된 Quarter Flying 에디터** 안에서 메뉴 한 번으로 자기 게임을 **에디터 없이 실행되는 폴더**로 빌드할 수 있게 한다.

> **관련 문서**
> - [docs/MASTER_PLAN.md](MASTER_PLAN.md): 이 계획은 MASTER_PLAN의 우선순위 항목 P0-b("에디터 분리형 독립 런타임 패키징")와 P1("배포 빌드/패키징"), §3 8대 핵심 시스템 중 #6 "빌드 & 배포"를 구체화한다.
> - [docs/SOUND_LITE_PLAN.md](SOUND_LITE_PLAN.md) §6.1: 게임 빌드의 오디오는 **v1 제외로 확정**되어 있다. 이 계획도 그 결정을 따른다.
> - [docs/INGAME_UI_CUSTOMIZATION_PLAN.md](INGAME_UI_CUSTOMIZATION_PLAN.md) (이하 "UI 계획"): 인게임 UI는 이 문서의 **선행 과제**다(§0.5).

---

## 읽는 법 — 표기 규칙

이 문서는 다른 계획서의 번호를 함께 인용하므로, 번호와 상태 표시를 다음처럼 구분한다.

| 표기 | 뜻 |
|---|---|
| **Phase 0 ~ Phase 5** | 이 계획의 구현 단계(§4) |
| **UI Phase 0 ~ UI Phase 6** | UI 계획(`INGAME_UI_CUSTOMIZATION_PLAN.md` §5.1)의 단계 |
| **S1 ~ S6** | 이 계획 Phase 0의 검증 항목(Spike) |
| **C1 ~ C4** | 이 계획의 설계 제약(§0.3) |
| **D1 ~ D11** | 이 계획의 설계 결정(§2) |
| **빌드 단계 1 ~ 8** | Phase 4에서 `build_game()`이 실행하는 순서 |
| MASTER_PLAN **P0-b**, 개선안 **P1-1**·**P2-8** | 다른 문서의 우선순위 번호. 이 문서의 번호와 무관하다 |

| 상태 표시 | 뜻 |
|---|---|
| **(코드 확인)** | 소스를 읽어 확인했다. 실행은 하지 않았다 |
| **(실행 확인)** | 실제로 실행해 결과를 봤다 |
| **(문서 기준)** | 다른 문서의 기록을 옮겼다. 이 문서 작성 시 다시 확인하지 않았다 |
| **(확인 필요)** | 아직 모른다. 어디서 확인할지 함께 적는다 |
| **(결정 YYYY-MM-DD)** | 사용자가 정한 사항 |

용어:

- **에디터 GUI** — PySide6로 만든 에디터 창·패널·대화상자. 이 문서에서 "UI"라고만 쓰지 않는다.
- **인게임 UI** — 게임 화면 안에 그려지는 HUD·메뉴 등. 엔진 기능이며 PySide6와 무관하다.
- **Player** — 에디터 없이 게임을 실행하는 C++ 실행 파일(`quarterflying_player.exe`).
- **Player 템플릿** — 에디터 설치 폴더의 `runtime/win-x64/`에 들어 있는, 미리 빌드된 Player와 그 실행에 필요한 파일 묶음. Build Game은 이것을 복사해 쓴다.
- **프로젝트 폴더** — 게임 개발자가 만든 씬·에셋·스크립트가 있는 폴더. 에디터 설치 폴더 밖에 있다(§3.2).
- **빌드 출력 폴더** — Build Game이 만들어 내는 폴더(§3.3).
- **샘플 프로젝트** — 프로젝트를 열지 않았을 때 쓰는 기본 프로젝트. 지금의 `engine/assets`를 그대로 쓴다.

> **구현 현황(2026-10-07)**: Phase 0~4를 구현했다. 단계별로 어디까지 확인했는지(컴파일 / 단위 테스트 / 실제 실행)와 계획에서 달라진 점은 **§9 구현 기록**에 있다. 남은 것은 인게임 UI 선행 과제(§0.5), 깨끗한 PC 두 대로 하는 최종 통과 기준, Phase 5다.

> **2026-10-07 개정 요약**
> 1. 인게임 UI는 "구현돼 있으나 아직 확인 안 됨"이 아니라 **구현되어 있지 않다**(코드 확인). 그래서 옛 검증 항목 "인게임 UI 실측"(지금의 S6)과 옛 미해결 4를 닫고, 인게임 UI를 이 계획의 **선행 과제**로 정했다(결정 2026-10-07).
> 2. 작업 범위를 ①·②로 한정했다(결정 2026-10-07, §0.2).
> 3. 디버그 그리드가 게임 카메라로 볼 때도 그려진다는 것을 코드로 확인했다. 그래서 그리드를 끄는 작업을 Phase 5에서 **Phase 2 필수 항목**으로 옮겼다.
> 4. 사실 정정: `scene.json` 키 목록, `run_editor.bat`가 빌드 폴더로 이동하는 문제, 빌드에 쓰인 Python 경로, `EngineTests`가 검사하는 범위, MASTER_PLAN 절 번호.
> 5. 표기 정리: 검증 항목 번호를 P0-1~P0-6에서 S1~S6으로 바꿨다(다른 문서의 P0-x와 겹치지 않게). 설계 제약에 C1~C4 번호를 붙였다.

---

## 0. 전제

### 0.1 5단계 제품 모델

이 기능은 다음 5단계 제품 모델 위에서 정의된다. 각 단계의 산출물이 다음 단계의 입력이 된다.

```text
① 게임 개발용 제작환경 (도구 개발자용 소스 코드)
        C++ 엔진 · Python · PySide6 · CMake
        │
        │ 빌드 (도구 개발자: VS / CMake / Python)
        ▼
② 빌드된 완제품 에디터
        게임 개발자가 설치해 사용하는 제품
        에디터 GUI(PySide6) · 엔진 · Build Game 기능 · Player 템플릿
        │
        │ 게임 개발자가 사용
        ▼
③ 게임 개발자의 작업
        씬 편집 · 에셋 관리 · 스크립트 작성 · 인게임 UI 구성
        │
        │ Build Game
        ▼
④ 빌드 출력 폴더
        Player 템플릿 + 게임 콘텐츠
        에디터 GUI와 PySide6는 포함하지 않는다
        │
        ▼
⑤ 게임 사용자
        완성된 게임을 실행해 플레이한다
```

- **①** — 이 코드베이스. 도구 개발자만 쓴다. 여기서 ②와 그 안의 Player 템플릿을 만든다.
- **②** — 게임 개발자가 설치하는 제품. **PySide6는 ②의 에디터 GUI를 위해서만 존재한다.** ②는 Build Game 기능과 Player 템플릿(`runtime/`)을 반드시 포함한다.
- **③** — 게임 개발자가 ②로 하는 작업. 게임 개발자에게는 컴파일러·CMake·소스가 없다.
- **④** — Build Game이 만든 독립 실행 폴더. Qt 없는 Player, 엔진 DLL, Python 런타임, 게임 콘텐츠만 들어 있다. `PySide6`, `editor/main.py`, 빌드 대화상자 같은 에디터 구성 요소는 들어가지 않는다.
- **⑤** — ④를 실행하는 사람.

> **핵심 원칙**: ②의 GUI 의존성(PySide6)은 ④로 넘어가지 않는다. 에디터와 Player는 같은 엔진 코드를 공유하지만, 제품으로서의 역할과 배포 대상이 다르다. (자동차 공장의 조립라인을 차와 함께 보내지 않는 것과 같다.)

### 0.2 작업 범위 — ①·②만 다룬다 (결정 2026-10-07)

이 프로젝트는 **게임 개발용 에디터**다. 따라서 이 계획이 만드는 것은 ①과 ②뿐이다.

| 단계 | 이 계획에서의 취급 |
|---|---|
| ① | **작업 대상.** 소스·CMake 타깃·테스트를 고친다. |
| ② | **작업 대상.** Build Game 기능, Player 템플릿, 프로젝트 열기·씬 저장, 설치 폴더 구성(§3.1)을 만든다. |
| ③·④ | **다루지 않는다.** 단, ②가 읽는 **입력 형식**(프로젝트 폴더, §3.2)과 ②가 만들어 내는 **출력 형식**(빌드 출력 폴더, §3.3)은 ②의 사양이므로 정의한다. |
| ⑤ | **다루지 않는다.** |

### 0.3 설계 제약

§0.1의 모델에서 다음 네 가지 제약이 나온다. 이 문서의 모든 결정은 이 제약을 따른다.

- **C1. Build Game은 컴파일하지 않는다.** ②에 미리 들어 있는 Player 템플릿에 게임 개발자의 씬·에셋·스크립트를 **복사해 조립**할 뿐이다(Unity·Godot의 Export Template과 같은 방식). 게임 개발자에게는 컴파일러가 없기 때문이다.
- **C2. Build Game 코드는 ② 안에서 돈다.** 따로 설치하는 CLI나 외부 `tools/` 스크립트가 아니라 에디터 패키지의 일부다.
- **C3. 게임 콘텐츠는 에디터 설치 폴더 밖에 있다.** 그래서 **프로젝트 폴더 개념이 반드시 필요하다.** 지금처럼 모든 에셋이 `engine/assets` 한 곳에 있는 구조로는 불가능하다.
- **C4. 빌드 출력 폴더에는 Qt가 없다.** 이것은 "게임에 UI가 없다"는 뜻이 **아니다**. 인게임 UI는 Qt 없이 엔진이 그리므로 빌드 출력 폴더에서도 동작해야 한다(§0.4).

### 0.4 에디터 GUI와 인게임 UI의 구분

"GUI/UI"라고 부를 수 있는 것이 두 가지 있고, 서로 다른 단계에 속한다.

| | 에디터 GUI | 인게임 UI |
|---|---|---|
| 정체 | PySide6 기반 저작 도구(도크 패널, 브라우저, 빌드 대화상자) | 게임 화면 안의 HUD·메뉴·대화창 등을 만드는 엔진 기능 |
| 현재 코드 | 있음 — `engine/editor/`(main.py, panels/) | **없음**(코드 확인) — 설계만 UI 계획에 있다 |
| 어디서 쓰이나 | ② 안에서만 | ②에서 구성하고, Player에서 실행·렌더링 |
| PySide6 의존 | 있음(본질적) | **없어야 한다** — Player는 Qt 없이 그린다 |
| 빌드 출력 폴더에 | **넣지 않는다** | **넣는다** — UI를 담은 씬, UI가 참조하는 폰트·이미지 |

이 구분이 깨지면 두 가지 결함이 생긴다. (a) 인게임 UI 코드가 PySide6를 import해서 Player에서 import 오류가 난다. (b) 인게임 UI가 빌드 출력 폴더에서 빠져 빈 화면이 나온다.

인게임 UI는 아직 구현되어 있지 않다. 그래서 "기존 UI를 Qt에서 떼어 내는" 이관 작업은 없다. 대신 **처음부터 Qt 없이 만드는 것**을 UI 계획의 완료 조건으로 건다(§0.5).

### 0.5 선행 과제 — 인게임 UI (결정 2026-10-07)

이 계획은 인게임 UI를 Build Game v1의 완료 조건에 포함한다. 따라서 UI 계획의 다음 단계가 **이 계획의 Phase 4가 끝나기 전에** 완료되어야 한다.

| UI 계획 단계 | 내용 | 이 계획과의 관계 |
|---|---|---|
| UI Phase 0 | 계층 컴포넌트 + 다중 엔티티 프리팹 | ✅ 2026-10-01 완료 — 컴파일·유닛 테스트·에디터 실행으로 검증(문서 기준: `REVIEW_BASED_IMPROVEMENT_PLAN.md` 개선안 P1-1) |
| UI Phase 1 | UI 기본 컴포넌트(RectTransform, Canvas) | **선행 과제** — 이 계획 Phase 1의 씬 파일에 저장될 컴포넌트 |
| UI Phase 2 | UI 렌더링(TextRenderer, SpriteRenderer) | **선행 과제** — Player가 Qt 없이 그려야 하는 부분 |
| UI Phase 3 | UI 이벤트(클릭·호버) | **선행 과제** — 메뉴·버튼이 Player에서 동작해야 한다 |
| UI Phase 4~6 | UI 에디터 패널, UI 프리팹 통합, 고급 레이아웃 | **선행 과제 아님.** 이 계획과 독립. 그전까지 인게임 UI는 Inspector로 컴포넌트를 편집해서 구성한다 |

**원칙 — 기반만 제공하고, 게임 개발자의 디자인 자유도를 제한하지 않는다 (결정 2026-10-07).** UI 에디터 패널은 엔진이 정한 스타일이나 고정된 위젯 세트를 강요하는 "완성형 디자이너"가 아니다. 게임 개발자가 자기 UI를 직접 설계하도록 받쳐 주는 **기반**이다.

- 엔진이 제공하는 것은 **조합할 수 있는 기본 단위**다: 배치·크기·앵커, 텍스트, 이미지, 입력 이벤트, 계층.
- 버튼·HP 바·메뉴 같은 **완성품은 엔진 기능이 아니다.** 게임 개발자가 기본 단위와 프리팹·스크립트로 만든다. 엔진이 예시를 동봉하더라도 수정 가능한 샘플 프리팹으로만 제공한다.
- UI 에디터 패널은 기본 단위를 배치·편집하는 도구일 뿐이다. 패널이 지원하지 않는 구성(스크립트로 만든 동적 UI, 개발자가 직접 조합한 컴포넌트 등)도 런타임에서 똑같이 동작해야 한다. **"패널에서 만들 수 있는 것"이 "게임에서 가능한 UI"의 한계가 되면 안 된다.**
- 테마·폰트·색은 **덮어쓸 수 있는 기본값**으로 두고, 바꿀 수 없게 고정하지 않는다.

이 계획이 UI 계획과 맞물리는 지점은 네 곳이다.

1. **씬 파일 형식** — UI 계획 §3.2는 인게임 UI를 **ECS 컴포넌트**로 설계했다. 따라서 별도의 UI 레이아웃 파일 없이 씬 파일(`SerializeRegistry` 출력) 안에 저장된다. 단, UI 계획 §4.2(같은 타입 컴포넌트 여러 개 허용 → 컴포넌트 배열 형식, 미착수)가 구현되면 직렬화 형식이 바뀐다. 씬 파일의 `version` 필드(Phase 1)로 이 변경을 구분한다.
2. **렌더 순서** — UI 계획 §3.1은 "기존 RenderGraph 재활용"을 전제한다. 그러나 **게임 실행 중 RenderGraph에 등록되는 패스는 0개다**(코드 확인: 테스트 밖에 패스 등록 코드가 없고, `Renderer::Render()`는 `GetPassCount() == 0`에서 반환한다. 실제 그리기는 그 앞의 즉시 모드 호출이 한다). 그래서 이 계획은 다음을 **방침으로 제안**한다: 인게임 UI는 `Renderer::Render()`의 즉시 모드 순서에서 씬 메시·파티클 **다음**에 그리고, 에디터 전용으로 꺼지는 조건(`drawEditorGizmos`, `RenderMode::Motion`)과 **무관하게** 그린다.
3. **형제 순서** — 인게임 UI에서는 형제 순서가 곧 그리기 순서다. 그런데 `HierarchyComponent`는 형제 순서를 저장하지 않는다(문서 기준: 개선안 P2-8, 미착수). 이것을 UI Phase 2 전에 처리해야 한다.
4. **빌드가 UI 자유도를 줄이지 않는다** — 위 원칙에 따라 게임 개발자는 UI 에디터 패널 밖의 방식으로도 UI를 만든다. 예: 스크립트가 런타임에 이미지·폰트를 불러오는 경우. Build Game의 의존성 수집(빌드 단계 3)이 찾지 못하는 에셋은 `include_dirs` 설정으로 포함할 수 있어야 한다. 판정 기준은 다음과 같다: **참조된 파일이 없으면 빌드 실패, 수집 규칙이 모르는 구성은 허용.**

---

## 1. 현재 상태

### 1.1 이미 있는 것 (재사용 대상)

| 항목 | 위치 | 의미 |
|---|---|---|
| 독립 창 + 메인 루프 | `Engine::Initialize()` / `Engine::Run()` ([engine/core/Engine.cpp](../engine/core/Engine.cpp)) | `Initialize()`가 `PlatformFactory`로 Win32 창을 직접 만들고, `TickFrame()`이 입력 → ECS → 렌더 → `PresentFrame`을 수행한다. **Qt 없이 도는 경로가 이미 있다**(코드 확인). 단, 에디터는 `InitializeFromWindowHandle`을 쓰므로 이 경로는 실제로 쓰인 적이 거의 없다. |
| 월드 직렬화 | `SerializeRegistry()` / `DeserializeRegistry()` ([engine/ecs/Reflection.cpp](../engine/ecs/Reflection.cpp)) | 에디터 Play → Stop 때 월드를 되돌리는 스냅샷용. UUID 기반 `EntityRef`, 계층, 이름을 보존한다. **파일로 쓰는 코드만 없다.** |
| 프리팹 | `PrefabAsset` ([engine/prefab/](../engine/prefab/)) | 직렬화된 엔티티 서브트리를 인스턴스화한다. |
| 스크립팅 | `ScriptSystem` ([engine/ecs/ScriptSystem.cpp](../engine/ecs/ScriptSystem.cpp)) | `moduleName`/`className`을 `py::module_::import`한 뒤 `on_create`/`on_update`/`on_destroy`를 호출한다. |
| 경로 주입 | `EngineConfig::assetRoot` | 에셋 기준 폴더를 외부에서 지정할 수 있다. (`PathResolver`는 정의만 있고 엔진 어디에서도 쓰이지 않는다 — 구현 중 확인. 실제 프레임의 셰이더는 모두 C++ 코드 안의 문자열이라 셰이더 파일 경로가 필요 없다.) |
| Qt 없는 액션 로직 | `core/action_data.py`, `core/action_playback.py` | PySide6를 import하지 않는다 → Player에서 재사용할 수 있다. |
| 계층 컴포넌트 | `HierarchyComponent` + `SetParent` ([engine/ecs/Hierarchy.h](../engine/ecs/Hierarchy.h)) | 인게임 UI 트리(Canvas → Panel → 버튼)의 기반. 2026-10-01 구현·검증(문서 기준). |

### 1.2 없는 것 / 막고 있는 것

1. **인게임 UI가 없다**(코드 확인). UI 컴포넌트·UI 시스템·텍스트/스프라이트 렌더러가 하나도 없다. 선행 과제로 둔다(§0.5).
2. **실행 파일 타깃이 없다.** CMake의 실행 파일 타깃은 `SimpleEngineTests`뿐이다(코드 확인).
3. **프로젝트 개념이 없다.** 에셋 경로가 에디터 소스 위치를 기준으로 코드에 고정되어 있다. 고칠 곳은 아래 7곳이다(코드 확인).
   - [demo_scene_seed.py:25](../engine/editor/demo_scene_seed.py) `_ASSETS_DIR`
   - [panels/prefab_browser.py:32](../engine/editor/panels/prefab_browser.py) `_PREFAB_ASSETS_DIR`
   - [panels/scene_hierarchy.py:27](../engine/editor/panels/scene_hierarchy.py) `_PREFAB_ASSETS_DIR`
   - [panels/motion_mixer.py:38](../engine/editor/panels/motion_mixer.py) `_MOTION_ASSETS_DIR`
   - [main.py:762](../engine/editor/main.py) 액션 재생용 `assets_dir`
   - [main.py:838](../engine/editor/main.py) `engine/assets/scene.json`
   - [main.py:855-856](../engine/editor/main.py) `paths.assetRoot`/`paths.shaderRoot`
4. **씬을 파일로 저장·불러오는 기능이 없다.** `New/Open/Save Scene` 메뉴 항목은 만들어져 있지만 아무 동작에도 연결되어 있지 않다([main.py:153-159](../engine/editor/main.py), 코드 확인). 지금의 `scene.json`은 메시 배치 목록이다(오브젝트 키: `name`·`model`·`material`·`texture`·`grid`·`instances`, 최상위 키: `version`·`objects`·`lights`). 그래서 카메라·프리팹·스크립트·액션 컴포넌트를 담을 수 없다. 앞으로 생길 인게임 UI 컴포넌트도 마찬가지다. 에디터는 실행할 때마다 `seed_demo_scene()`(코드)과 `scene.json`으로 월드를 다시 만든다.
5. **에디터 Play 중 돌아가는 파이썬 코드가 Qt에 묶여 있다.** `ScenePlaybackController`([core/scene_action_playback.py](../engine/editor/core/scene_action_playback.py))는 파일 맨 위에서 `core.sound_player`를 import하고, 그 파일은 `PySide6.QtMultimedia.QSoundEffect`를 import한다(코드 확인). 따라서 PySide6가 없는 Player에서는 import 단계에서 실패한다.
6. **현재 `install()` 결과는 §3.1의 설치 폴더 구성과 다르다**(코드 확인).
   - 설치 대상이 `quarterflying`(.pyd), `editor/*.py`, `assets/`, `run_editor.bat`뿐이다. **`quarterflying_engine.dll`이 `install(TARGETS …)`에 없다**([CMakeLists.txt](../engine/CMakeLists.txt)).
   - `run_editor.bat`는 `python editor/main.py`를 실행한다([run_editor.bat.in](../engine/scripts/run_editor.bat.in)). 즉 **PC에 설치된 Python에 의존**한다. 게임 개발자 PC에 Python과 PySide6가 있다는 보장은 없다.
   - 같은 스크립트가 실행 전에 `cd /d "@CMAKE_CURRENT_BINARY_DIR@"`를 한다. 즉 설치된 `run_editor.bat`도 **설치 폴더가 아니라 빌드 폴더로 이동한다.** 따라서 빌드 폴더를 지우거나 설치 폴더를 다른 PC로 옮기면 에디터가 실행되지 않을 것으로 본다(코드 확인, 실행은 하지 않음).
7. **엔진 DLL이 Python에 링크되어 있다.** `BASE_LIBRARIES`에 `pybind11::embed`가 있으므로 Player도 Python 런타임이 필요하다(코드 확인).
8. **Windows 전용이다.** 플랫폼 구현은 `Win32Platform`뿐이다(코드 확인).

### 1.3 빌드 환경

- 빌드 캐시 기준: Visual Studio 17 2022, Python **3.14**(`quarterflying.cp314-win_amd64.pyd`), pybind11 **2.11.1**(`cmake/Dependencies.cmake`)(코드 확인).
- 빌드에 쓰인 인터프리터는 `%LOCALAPPDATA%/Microsoft/WindowsApps/python3.exe`다(`CMakeCache.txt`의 `PYTHON_EXECUTABLE`). 이 경로는 Windows의 앱 실행 별칭이고, 실제 설치 위치는 Microsoft Store 패키지가 아니라 `%LOCALAPPDATA%\Python\pythoncore-3.14-64`(일반 배포본)다(실행 확인, 2026-10-07). 이 폴더에서 표준 라이브러리를 골라 복사할 수 있었다(S4).
- `.pyd`는 `cp314` ABI에 묶여 있다. 그래서 **에디터와 Player는 같은 마이너 버전의 Python을 써야 한다.** 에디터와 Player가 **같은 `quarterflying.pyd`/`quarterflying_engine.dll` 한 벌**을 공유하면 이 조건이 자연히 맞는다.

---

## 2. 설계 결정

| # | 결정 | 이유 |
|---|---|---|
| D1 | **Player 템플릿 방식.** 도구 개발자가 `quarterflying_player.exe`를 빌드해 ②의 `runtime/`에 넣고, Build Game은 그것을 **복사**해 조립한다. | C1 — 게임 개발자에게는 컴파일러가 없다. |
| D2 | Player는 C++ 실행 파일이고 Python을 **내장**한다(`py::scoped_interpreter`). | `ScriptSystem`과 액션 재생이 Python에 의존한다. 엔진 DLL이 이미 `pybind11::embed`를 링크한다. |
| D3 | **Build Game 로직은 에디터 패키지 안의 순수 Python 모듈**(`editor/gamebuild/game_builder.py`)로 만들고, **PySide6를 import하지 않는다.** 에디터 GUI(`editor/gamebuild/build_dialog.py`)는 이 모듈을 호출만 한다. 패키지 이름이 `build`가 아닌 이유: 저장소 `.gitignore`의 `build/` 규칙에 걸려 소스가 커밋되지 않고, PyPI의 `build` 패키지와도 이름이 겹친다(구현 중 확인). | C2 — ② 안에서 돈다. Qt가 없으므로 단위 테스트가 쉽고, `python gamebuild/game_builder.py <프로젝트>`로 에디터 없이도 쓸 수 있다. |
| D4 | **프로젝트 폴더를 도입한다.** `project.json` + `assets/` + `scenes/` + `scripts/`. 에디터는 프로젝트를 열고 그 폴더를 에셋 기준 폴더로 쓴다. | C3 — 게임 콘텐츠는 설치 폴더 밖에 있다. |
| D5 | **엔진 내장 리소스는 프로젝트가 아니라 ②에 둔다.** 프로젝트에서는 `builtin:` 접두사로 참조한다. **(구현 시 보류)** 지금은 내장 리소스 파일이 필요 없다 — 셰이더는 C++ 코드 안에 있고, 기본 큐브는 `meshPath`가 빈 `RenderableComponent`면 엔진이 만든다. 그래서 `builtin/` 폴더는 만들지 않았고, 빌드 수집은 `builtin:` 경로를 건너뛰기만 한다. 기본 폰트·UI 셰이더가 생기면(UI Phase 2) 다시 정한다. | 게임 개발자가 엔진 리소스를 자기 프로젝트로 복사할 필요가 없고, 엔진을 업데이트하면 같이 갱신된다. |
| D6 | **씬 파일 = `*.scene.json`** (`SerializeRegistry()` 출력 + 헤더). 새 형식을 만들지 않는다. | 에디터 Play/Stop 스냅샷에서 UUID·계층·이름·EntityRef 보존이 이미 쓰이고 있다. 에디터 저장과 Player 로드가 같은 코드를 쓰면 형식이 어긋나지 않는다. 인게임 UI도 ECS 컴포넌트이므로 `ComponentRegistry`에 등록되면 이 형식에 포함된다. UI 계획 §4.2가 구현되면 `version`을 올려 구분한다(§0.5). |
| D7 | **Player는 항상 `World::Play()` 상태로 시작한다.** | `RunsInEditMode() == false`인 시스템(예: `CameraRigSystem`)은 Play 상태에서만 돈다. |
| D8 | **Player용 Python 런타임은 Player 템플릿에 처음부터 넣는다(구현 시 결정, §6 미해결 1의 결론).** 템플릿을 조립할 때(`runtime/stage_template.py`) 빌드에 쓰인 Python에서 `python314.dll`·표준 라이브러리 zip(`python314.zip`)·`DLLs/`를 골라 넣고 `python314._pth`로 격리한다. `PySide6`와 `site-packages`는 넣지 않는다. | ②가 Python을 어떤 방식으로 포함하든(폴더 동봉/실행 파일로 묶기) Build Game이 영향을 받지 않는다. 다운로드가 필요 없고, .pyd와 ABI가 항상 일치한다. 대신 템플릿이 커진다(30.2MB, 그중 대부분이 Python 런타임). |
| D9 | **게임 빌드 오디오는 v1에서 제외한다.** `SoundPlayer`를 선택 사항으로 바꾸고, Player에서는 소리를 내지 않는 `NullSoundPlayer`를 쓴다. | SOUND_LITE_PLAN §6.1 확정 사항. 액션 이벤트는 그대로 발생하고 소리만 나지 않는다. |
| D10 | **설정 파일을 `project.json`(편집용)과 `game.json`(실행용)으로 나눈다.** Build Game이 `project.json`의 빌드 설정으로 `game.json`을 만든다. | 기존 `config.json`은 에디터 창·경로용인데, 루트·`engine/`·`engine/editor/`에 사본이 3개 있어 역할이 섞여 있다. |
| D11 | **인게임 UI는 에디터 GUI와 별개인 엔진 기능으로 다루고, Qt 없이 동작해야 한다.** ②에서 구성 → 씬에 저장 → Player에서 엔진 렌더러로 그린다. Player에 PySide6를 넣어서 해결하지 않는다. | C4 위반을 막는다. 인게임 UI는 아직 구현이 없으므로 이관할 Qt 의존도 없다. UI 계획을 구현할 때 런타임 쪽(C++ 컴포넌트·시스템·렌더러, Python 훅)이 PySide6를 import하지 않는 것을 완료 조건으로 건다(§0.5). |

---

## 3. 목표 구성

### 3.1 ②의 설치 폴더 구성 — 이 계획이 지켜야 할 구성

설치 프로그램 자체는 이 계획의 범위 밖이다(§7). 다만 설치된 폴더가 어떤 구성이어야 하는지는 여기서 정한다.

```text
QuarterFlying/                        # 설치 폴더
├─ QuarterFlyingEditor.exe            # (또는 런처) 동봉된 Python으로 editor/main.py 실행
├─ python/                            # 전용 Python 3.14 + PySide6 (에디터 GUI 전용 — 빌드 출력 폴더로 복사하지 않는다)
├─ editor/                            # *.py (gamebuild/game_builder.py 포함, test_*.py 제외)
├─ assets/                            # 샘플 프로젝트 에셋(프로젝트를 열지 않았을 때 쓰는 샘플 = 설치 폴더 자체)
├─ run_editor.bat                     # 설치 폴더 기준으로 bin/을 PYTHONPATH에 넣고 editor/main.py 실행
├─ bin/
│  ├─ quarterflying_engine.dll
│  ├─ quarterflying.cp314-win_amd64.pyd
│  └─ VC 런타임 DLL
└─ runtime/                           # ★ Player 템플릿 (Build Game이 복사하는 원본)
   └─ win-x64/
      ├─ quarterflying_player.exe
      ├─ quarterflying_engine.dll
      ├─ quarterflying.cp314-win_amd64.pyd
      ├─ vcruntime140*.dll, msvcp140*.dll, concrt140.dll
      ├─ python314.dll, python3.dll, python314.zip, DLLs/, python314._pth   # Player 전용 Python 런타임(D8)
      ├─ python/runtime_bootstrap.py, python/core/...   # Qt 없는 런타임 Python 코드
      └─ runtime_manifest.json              # 템플릿 버전, 엔진 버전, Python ABI, 파일 해시
```

- `python/`(PySide6 포함, 에디터용)은 에디터 전용이다. Build Game은 여기서 아무것도 복사하지 않는다 — Player용 Python은 템플릿에 따로 들어 있다(D8). 빌드 출력 폴더에 `PySide6` 폴더가 있으면 잘못된 빌드다(빌드 단계 6에서 검사).
- 구현 상태: `python/`(에디터용 전용 Python)과 `QuarterFlyingEditor.exe`는 아직 없다(§7 범위 밖 — 설치 프로그램·패키징). 지금 설치본은 PC에 설치된 Python으로 `run_editor.bat`를 실행한다.
- 인게임 UI는 ②의 에디터 GUI 안에서 구성한다. 처음에는 Inspector로, UI Phase 4 이후에는 UI 에디터 패널로도 구성한다. 인게임 UI의 런타임 코드(컴포넌트·시스템·렌더러)는 엔진 DLL에 들어간다. 그래서 Player 템플릿에 따로 추가할 파일은 UI 셰이더와 기본 폰트(있다면) 정도다. 정확한 목록은 UI Phase 2 구현 후 확정한다.

### 3.2 프로젝트 폴더 형식 (②가 읽는 입력)

```text
MyProject/
├─ project.json                # 이름, engineVersion, 시작 씬, 빌드 설정
├─ assets/
│  ├─ models/ textures/ fonts/ prefabs/ actions/ sfx/ motion/
│  └─ input/actions.json       # 입력 액션 맵(USABILITY_IMPROVEMENT_PLAN G0a, 2026-10-15). 새 프로젝트는 빈 맵
├─ scenes/Main.scene.json
├─ scripts/player_controller.py        # ScriptComponent.moduleName이 가리키는 모듈
└─ .quarterflying/session.json # 에디터 세션(2026-10-16): 열어 둔 씬·액션, 에디터 카메라, 모드. 사용자별 상태 -
                               # 버전 관리·빌드 출력에 넣지 않는다(editor/project/session.py)
```

인게임 UI용 레이아웃 파일은 따로 없다. UI 요소는 씬·프리팹 안의 ECS 컴포넌트이고, 이미지는 `textures/`, 폰트는 `fonts/`의 파일을 컴포넌트가 경로로 참조한다(UI 계획 §3.2 기준).

### 3.3 빌드 출력 폴더 형식 (②가 만들어 내는 출력)

```text
dist/MyGame/
├─ MyGame.exe                  # quarterflying_player.exe의 이름을 바꾼 것
├─ quarterflying_engine.dll, quarterflying*.pyd, VC 런타임
├─ python314.dll, python3.dll, python314.zip, DLLs/, python314._pth    # 템플릿에 들어 있던 Player용 Python
├─ game.json                   # 시작 씬, 창 설정, 입력 액션 맵 경로 (Build Game이 생성)
├─ assets/ scenes/             # 시작 씬에서 참조되는 파일만, 프로젝트 상대 경로 그대로 (인게임 UI가 참조하는 폰트·이미지 포함)
│                              #   + assets/input/actions.json(있으면 항상 — 씬이 참조하는 파일이 아니라 프로젝트 설정)
├─ scripts/                    # 스크립트와 그 스크립트가 import하는 프로젝트 모듈 (프로젝트의 scripts 폴더 기준)
└─ python/                     # runtime_bootstrap 등
```

**있으면 안 되는 것**(하나라도 있으면 잘못된 빌드): `PySide6/`, `editor/`, `Qt6*.dll`, 빌드 대화상자 관련 모듈.

### 3.4 `game.json` 초안

```json
{
  "version": 1,
  "title": "My Game",
  "window": { "width": 1280, "height": 720, "fullscreen": false, "cursorLock": false },
  "startScene": "scenes/Main.scene.json",
  "assetRoot": "./",
  "inputActions": "assets/input/actions.json",
  "engine": { "numWorkerThreads": 0, "logFrameInterval": 0 }
}
```

`inputActions`(G0b)와 `window.cursorLock`(G3)은 2026-10-15에 추가했다(USABILITY_IMPROVEMENT_PLAN). 선택 키다 — 없으면 Player는 액션 맵 없이 시작하고 로그로 알린다. `version`은 올리지 않았다: 키 추가이고, 옛 Player는 모르는 키를 무시한다.

### 3.5 Player 시작 순서 (`engine/runtime/PlayerMain.cpp`, 신규)

```text
1. 작업 폴더를 exe가 있는 폴더로 고정 (더블클릭·바로가기로 실행해도 경로가 맞게)
2. game.json 로드 → 실패하면 원인(경로·키)을 로그와 메시지 상자로 알리고 종료
3. py::scoped_interpreter 시작(._pth가 있으면 격리 모드), sys.path에 exe 폴더·python/·scripts/ 추가, import quarterflying
4. EngineConfig 채우기(assetRoot, showDebugGrid=false, scriptHotReload=false)
5. Engine::Initialize(config)                   # 자체 Win32 창 + GL 컨텍스트
6. world = engine.CreateWorld(...); engine.SetActiveWorld(world)
7. LoadSceneFromFile(registry, startScene)      # Phase 1에서 만든다. 인게임 UI도 이 씬 안에 있다
8. isMainCamera 엔티티가 없으면 에러 로그        # 아무 표시 없이 검은 화면이 되는 것을 막는다
9. world->Play()                                # 인게임 UI 시스템이 Play 상태에서만 돌지는 UI 계획 구현 때 정한다
10. runtime_bootstrap.start(registry, assetsDir) # 액션 재생 등
11. while (engine.IsRunning()) { TickFrame(); runtime_bootstrap.tick(dt); }
12. world->Stop(); Shutdown(); 인터프리터 종료
```

`Engine::Run()` 대신 루프를 직접 쓰는 이유는 매 프레임 Python 훅을 호출하기 위해서다. 에디터에서는 Qt 타이머의 `_on_fps_tick`([main.py:895](../engine/editor/main.py))이 이 일을 한다.

**Player 화면에 영향을 주는 렌더러 동작** (코드 확인. 2026-10-07 구현 후 Player 화면 캡처로 그리드·기즈모가 보이지 않는 것을 확인했다):

- **디버그 그리드는 게임 카메라로 볼 때도 그려진다.** `Renderer::Render()`는 그리드가 초기화되어 있고 카메라가 있으면 어느 카메라로 보든 그리드를 그린다. 그래서 **Player에서 그리드를 끄는 설정이 필요하다**(Phase 2 필수 항목 — `EngineConfig::showDebugGrid`로 구현).
- 에디터 기즈모(카메라 시야 선)는 에디터 카메라(`ViewCamera::Editor`)로 볼 때만 그려진다. 그래서 Player(`ViewCamera::Game`)에서는 그려지지 않는다. 화면 확인은 S2에서 한다.
- 본 라인은 `RenderMode::Motion`에서만, 씬 메시와 파티클은 `RenderMode::Scene`에서만 그려진다. 따라서 **Player는 Scene 모드로 시작해야 한다.**
- 화면을 지우는 배경색은 `Renderer::Render()`에 `(0.10, 0.11, 0.13)`으로 고정되어 있다. 게임마다 바꿀 수 있게 할지는 v1에서 정하지 않는다.
- 인게임 UI는 위의 에디터 전용 조건과 무관하게, 씬 메시·파티클 다음에 그린다(§0.5 맞물림 2). 인게임 UI의 이벤트나 Python 훅이 생기면 `runtime_bootstrap.tick(dt)`에서 함께 호출한다.

---

## 4. 단계별 구현

### Phase 0 — 검증 스파이크 (이 단계의 코드는 버려도 된다)

가정이 틀리면 설계 전체가 흔들리는 항목만 먼저 확인한다.

- [x] **S1. Python 내장 호환성** — **통과(실행 확인, 2026-10-07)**: 개발 빌드와 템플릿 양쪽에서 인터프리터 시작과 `import quarterflying`이 된다. — Python 3.14 + pybind11 2.11.1에서 `py::scoped_interpreter`로 인터프리터를 시작한 뒤 `import quarterflying`이 되는가. 안 되면 pybind11을 올리거나 Python 마이너 버전을 고정해야 한다.
- [x] **S2. 독립 창 렌더링** — **통과(실행 확인, 화면 캡처)**: 카메라 시점의 큐브·피라미드가 그려지고, 디버그 그리드와 에디터 기즈모는 보이지 않는다. 셰이더 경로 확인은 해당 없음(셰이더가 코드 안에 있음). — 최소한의 `main()`으로 `Initialize` → `CreateWorld` → 카메라+큐브 → `Play()` → 루프를 돌려 큐브가 보이는가.
- [x] **S3. Qt 없이 스크립트 실행** — **통과(실행 확인)**. 단, 처음에는 실패했다: `ScriptSystem`이 어떤 World에도 등록되어 있지 않았다(§9). — `ScriptComponent`가 붙은 엔티티의 `on_update`가 Player에서 호출되는가.
- [x] **S4. 골라 복사한 Python으로 실행** — **통과(실행 확인)**: 템플릿의 `._pth` 격리 환경에서 `sys.path`에 템플릿 폴더와 게임 `scripts/`만 있고, PySide6는 import할 수 없다. 표준 라이브러리는 "최소 목록" 대신 제외 목록(test, tkinter, idlelib 등)으로 골랐다(548개 모듈) — 최소화는 Phase 5. — 에디터용 Python에서 `python314.dll`과 표준 라이브러리만 골라 만든 폴더(`._pth` 사용)에서, PATH 설정 없이 `quarterflying.pyd`가 로드되는가. 이때 **필요한 표준 라이브러리의 최소 목록**도 측정한다. §1.3의 앱 실행 별칭 Python에서 복사가 가능한지도 확인한다.
- [x] **S5. ②가 Python을 포함하는 방식** — **종료(D8 결정으로 대체)**: Player용 Python을 템플릿에 넣기로 해서 이 질문이 Build Game에 영향을 주지 않게 됐다. (§6 미해결 1) — ②가 전용 Python 폴더를 동봉하는지, PyInstaller 등으로 실행 파일 하나로 묶는지에 따라 D8의 복사 원본이 달라진다.
- [x] **S6. 인게임 UI 구현 확인 — 종료(2026-10-07).** 결과: 구현 없음(코드 확인). 떼어 낼 Qt 의존 코드가 없으므로 선행 과제로 돌렸다(§0.5).

**통과 기준**: S1~S4를 개발 PC의 `build/Release`에서 직접 실행해 재현하고, S5의 결론을 §6에 기록한다.

### Phase 1 — 프로젝트 개념 + 씬 저장·불러오기 (Player보다 먼저 필요)

**목적**: Player가 불러올 "저장된 씬"과, 설치 폴더 밖의 "프로젝트"를 만든다. 그 결과 에디터의 Save/Open 메뉴도 동작하게 된다.

신규/변경:

- `engine/scene/SceneIO.{h,cpp}` (신규)
  - `SaveSceneToFile(ECSRegistry&, path)` → `{"format":"quarterflying.scene","version":1,"entities":<SerializeRegistry 출력>}`
  - `LoadSceneFromFile(ECSRegistry&, path)` → `std::expected<void, EngineError>`. 파일 없음·JSON 오류·버전 불일치를 **원인을 적은 서로 다른 에러**로 돌려준다(코드베이스 관례: 조용히 실패하지 않는다).
  - 임시 파일에 먼저 쓴 뒤 이름을 바꾼다. 저장 도중 크래시가 나도 기존 씬 파일이 깨지지 않게 하기 위해서다.
- `engine/bindings/ECSBindings.cpp`: `World.SaveScene(path)` / `World.LoadScene(path)`.
- `editor/project/project.py` (신규, PySide6 import 없음): `Project`(root, assets_dir, scenes_dir, scripts_dir, `project.json` 읽기/쓰기), `create_project(path)`, `open_project(path)`.
- 에디터 경로 일원화: §1.2 항목 3의 고정 경로 7곳이 `Project`에서 경로를 얻도록 바꾼다. 프로젝트를 열지 않았을 때는 샘플 프로젝트(= 지금의 `engine/assets`)를 써서 기존 개발 흐름을 유지한다.
- 메뉴 연결: `File > New Project / Open Project / New Scene / Open Scene / Save Scene / Save Scene As`.
- 초기화 지점(`_apply_config`) 정리: 존재하지 않는 `PathResolver` 바인딩 호출을 지웠다(매번 실패하던 호출). 에셋 기준 폴더는 현재 프로젝트에서 정한다(`viewport.py`, `Engine.SetAssetRoot`).
- **인게임 UI 대비**: UI 컴포넌트는 아직 없지만, 생겼을 때 `ComponentRegistry`에 등록하기만 하면 씬 파일에 저장되게 한다. **등록되지 않은 컴포넌트가 붙은 엔티티를 저장하면 경고를 낸다**(§6 리스크 표 3행). UI 계획 §4.2가 구현되면 씬 `version`을 올린다.
- 지금의 데모 월드는 에디터에서 한 번 저장해 샘플 프로젝트의 `scenes/Demo.scene.json`으로 만든다(별도 변환기는 만들지 않는다).

테스트:

- C++(`engine/tests/SceneIOTests.cpp`, `tests/CMakeLists.txt`에 등록): 저장 → 불러오기 후 엔티티·계층·이름·컴포넌트가 같은지 / 세 가지 에러가 구분되는지 / 등록된 모든 컴포넌트 타입이 왕복되는지. 마지막 테스트는 등록된 타입 목록을 순회하므로, 인게임 UI 컴포넌트가 등록되면 따로 고치지 않아도 검사 대상에 들어간다.
- Python(`editor/test_project.py`, `editor/test_scene_save_load.py` — 기존 `test_*.py`처럼 직접 실행): 프로젝트 생성·열기, 경로 해석, 저장 왕복.

**통과 기준**: 설치 폴더가 아닌 임의 위치의 프로젝트를 열어 편집·저장한 뒤, 에디터를 다시 실행해 같은 월드가 복원되는 것을 화면으로 확인한다. 인게임 UI 컴포넌트의 왕복은 UI Phase 1 완료 후 같은 테스트로 확인한다.

### Phase 2 — Player 실행 파일 + 런타임 Python

신규/변경:

- `engine/runtime/PlayerMain.cpp` (§3.5 순서), `GameConfig.{h,cpp}`(`nlohmann_json` 사용. 빠진 키는 기본값, **타입이 틀린 키는 에러**).
- `engine/runtime/python/runtime_bootstrap.py`: `start(registry, assets_dir)`, `tick(dt)`. `ScenePlaybackController`를 `NullSoundPlayer`로 만든다.
- `core/scene_action_playback.py` / `core/sound_player.py`: `SoundPlayer` import를 필요할 때만 하도록 바꿔, **PySide6 없이도 import되게** 한다. 에디터 동작은 바뀌면 안 된다(`test_scene_action_playback.py`, `test_sound_player.py`로 회귀 확인).
- **디버그 그리드 끄기** (필수): `EngineConfig`에 그리드 표시 플래그를 추가하고 Player에서는 끈다. 에디터에서는 지금처럼 켠다(§3.5).
- **인게임 UI**: 이 Phase에서는 인게임 UI 코드를 만들지 않는다. UI Phase 1~3이 끝나면 Player 확인용 씬에 UI 요소를 넣어 같은 통과 기준으로 확인한다.
- `engine/CMakeLists.txt`:
  ```cmake
  add_executable(quarterflying_player runtime/PlayerMain.cpp runtime/GameConfig.cpp)
  target_link_libraries(quarterflying_player PRIVATE quarterflying_engine)
  ```
  콘솔 창 없이 실행하는 빌드는 옵션(`QF_PLAYER_CONSOLE`)으로 고를 수 있게 한다.
- 종료 순서: `WindowClose` 이벤트 → 루프 종료 → `Stop()` → `Shutdown()`. 이 순서를 지켜야 `ScriptSystem::OnStop`이 스크립트의 `on_destroy`를 호출한다.
- `--smoke-test N` 인자: N프레임을 그린 뒤 종료 코드 0으로 끝난다(Phase 3·4의 자동 검증용).

테스트:

- C++: `GameConfig` 파싱(정상 / 키 누락 / 타입 오류).
- 수동 확인: `game.json`·씬·에셋을 손으로 구성해 Player를 실행한다. 씬이 보이는지, 스크립트 `on_update`가 동작하는지, 창을 닫을 때 `on_destroy` 로그가 남고 정상 종료되는지, 디버그 그리드가 **보이지 않는지** 확인한다.

**통과 기준**: PySide6가 **없는** 환경에서 Player가 씬을 그리드 없이 그리고 정상 종료한다.

### Phase 3 — Player 템플릿을 설치 폴더에 넣기

**목적**: 도구 개발자가 빌드·설치하면 §3.1의 `runtime/` 구성이 만들어지게 한다.

- `engine/CMakeLists.txt`의 `install()` 정리:
  - `quarterflying_engine`, `quarterflying`, `quarterflying_player`를 `bin/`과 `runtime/win-x64/`에 설치
  - VC 런타임 DLL 동봉(`InstallRequiredSystemLibraries`)
  - `run_editor.bat`가 빌드 폴더로 이동하지 않도록 고친다(§1.2 항목 6)
- `runtime_manifest.json` 생성: 템플릿 버전, 엔진 버전, Python ABI(`cp314`), 주요 파일의 SHA-256.
- `check_template_layout`(스크립트 또는 CTest): 설치 결과에 템플릿 필수 파일이 모두 있는지 검사한다. UI Phase 2에서 UI 셰이더·기본 폰트가 생기면 필수 파일 목록에 추가한다.

**통과 기준**: `cmake --install`로 만든 폴더만으로(소스 폴더·빌드 폴더 없이) §3.1 구성이 완성되고, 그 안의 `runtime/win-x64/quarterflying_player.exe --smoke-test`가 통과한다.

### Phase 4 — Build Game (②의 에디터 기능)

`editor/gamebuild/game_builder.py` (신규, **표준 라이브러리만 사용, PySide6 import 없음**):

```python
@dataclass
class BuildSettings:
    project_root: Path
    out_dir: Path
    name: str
    start_scene: str            # 프로젝트 폴더 기준 상대 경로
    window: WindowSettings
    include_dirs: list[str]     # 자동 수집과 무관하게 통째로 포함할 폴더 (예: 스크립트가 런타임에 읽는 assets/prefabs)
    zip_output: bool

def build_game(settings, progress: Callable[[str, float], None], is_cancelled: Callable[[], bool]) -> BuildReport
```

빌드 단계 (각 단계는 `progress`로 진행 상황을 알리고, 중간에 취소할 수 있다):

1. **템플릿 검증** — `runtime_manifest.json`을 읽어 **엔진 버전과 Python ABI가 지금 에디터와 같은지**, 파일이 바뀌거나 빠지지 않았는지(해시) 확인한다. 다르면 원인을 적고 중단한다.
2. **씬 검증** — 포함할 모든 `*.scene.json`의 헤더·버전을 `SceneIO` 형식으로 검사한다.
3. **의존성 수집** — 시작 씬과 포함할 씬에서 참조되는 파일만 모은다.
   - `RenderableComponent.meshPath` / `texturePath` / `materialPath` → `*.material.json` → 그 안의 `texture` (머티리얼은 2026-10-15 R-4에서 추가. 머티리얼 파일 안의 경로 키는 `game_builder.py`의 `MATERIAL_PATH_FIELDS` 표 하나에 있다)
   - `ActionPlayerComponent.action` → `.action.json` → `params.clip` (경로 기준은 `.action.json` 파일 위치, SOUND_LITE_PLAN §6.4)
   - `ScriptComponent.moduleName` → `scripts/<모듈>.py`와, 그 파일이 import하는 프로젝트 내 모듈(`ast`로 정적 분석)
   - **인게임 UI** — UI 컴포넌트의 에셋 경로 필드를 메시·텍스처와 같은 규칙으로 모은다. UI 계획 §3.2 초안에는 `TextComponent.fontAsset`, §4.2 예시에는 `ImageComponent`의 `sprite`가 있지만, 필드 이름은 UI Phase 1 구현 때 확정된다. 그래서 수집 규칙을 "컴포넌트 이름 → 경로 필드" 표 하나로 두고, UI 컴포넌트가 생기면 표에 행만 추가한다.
   - 프리팹: 씬에 이미 펼쳐져 저장되므로 따로 모을 필요가 없다. 스크립트가 런타임에 `InstantiatePrefab`을 호출한다면 그 폴더를 `include_dirs`에 넣는다.
   - `builtin:` 참조: Player 템플릿에 이미 있으므로 복사하지 않는다.
   - 수집 규칙이 모르는 구성(예: 스크립트가 런타임에 여는 파일)은 빌드를 막지 않는다. 그런 파일은 `include_dirs`로 포함한다(§0.5 맞물림 4).
4. **참조된 파일이 없으면 빌드 실패** — "어느 씬의 어느 엔티티·컴포넌트가 가리키는 어떤 경로"가 없는지 보고한다. 빌드 출력 폴더를 실행해 보고 나서야 발견되는 일을 막는다.
5. **스크립트 import 점검** — 표준 라이브러리·`quarterflying`·프로젝트 내 모듈 외의 import(예: `PySide6`, `numpy`)는 **경고**로 보고한다(v1은 서드파티 패키지를 넣지 않는다). `PySide6` import 경고에는 "게임 화면의 UI는 인게임 UI 컴포넌트로 만든다. 에디터 GUI(PySide6)는 빌드 출력 폴더에 들어가지 않는다"를 함께 적는다.
6. **조립** — 템플릿 복사(Player용 Python 런타임과 `._pth` 포함, D8) → `quarterflying_player.exe`를 `<name>.exe`로 이름 변경 → 수집한 에셋·씬은 프로젝트 상대 경로 그대로, 스크립트는 `scripts/` 아래로 복사 → `game.json` 생성.
   - 출력은 **임시 폴더에 먼저 만들고, 성공하면 기존 출력 폴더와 바꾼다.** 실패한 빌드가 이전 결과를 망가뜨리지 않게 하기 위해서다.
   - 조립이 끝나면 출력 폴더에 `PySide6/`, `Qt6*.dll`, `editor/`가 **없는지** 검사한다(§3.3의 "있으면 안 되는 것").
7. **자체 검증** — 출력 폴더에서 `<name>.exe --smoke-test 30`을 시간 제한을 두고 실행해 종료 코드를 확인한다. 실패하면 Player 로그를 보고서에 붙인다.
8. 선택: zip 생성, 결과 폴더 열기.

에디터 GUI (`editor/gamebuild/build_dialog.py`) — 얇게 유지한다. **Build Game 기능에서 PySide6를 쓰는 곳은 이 파일뿐이다.**

- `File > Build Settings…`(이름, 시작 씬, 창 크기·전체 화면, 포함 폴더, 출력 폴더)와 `File > Build Game`(`Ctrl+B`).
- `build_game()`을 `QThread`에서 실행해 에디터 창이 멈추지 않게 한다. 진행률과 취소 버튼을 보여 주고, 단계별 로그를 상태바 로그 패널에 출력하고, 끝나면 보고서(포함 파일 수·용량·경고)를 표시한다.
- 설정은 `project.json`에 저장한다.

테스트:

- Python(`editor/gamebuild/test_game_builder.py`, 직접 실행): 의존성 수집(메시·텍스처·액션 → 클립·스크립트, UI 컴포넌트가 생기면 폰트·이미지), 누락 파일 에러 메시지, 템플릿 ABI·해시 불일치, 참조되지 않은 에셋 제외, 서드파티 import 경고, **Qt 파일(`PySide6`, `Qt6*.dll`)이 발견되면 빌드 실패**, 취소했을 때 출력 폴더에 일부 파일이 남지 않는지.
- 통합: 출력 폴더를 **다른 경로로 복사**한 뒤 `--smoke-test`가 통과하는지(경로가 코드에 고정되어 있지 않은지 검증). UI Phase 1~3 완료 후에는 인게임 UI가 있는 샘플 씬으로도 같은 검증을 한다.

**최종 통과 기준(②의 기능 검증)**: 두 환경으로 확인한다.

- **환경 A** — Python·Qt·Visual Studio가 설치되지 않은 깨끗한 Windows(VM 가능). 여기에 ②를 설치하고, 샘플 프로젝트로 Build Game을 실행한다.
- **환경 B** — 에디터도 Python도 설치되지 않은 Windows. 환경 A의 출력 폴더를 복사해 실행한다.

환경 B에서 인게임 UI가 있는 샘플 씬이 그려지고 정상 종료되면 통과다. 인게임 UI 부분은 선행 과제(§0.5)가 끝나야 확인할 수 있다.

### Phase 5 — 다듬기 (선택, 위에서부터 우선)

1. **스크립트 자동 다시 불러오기 끄기** — `ScriptSystem`은 스크립트를 인스턴스화할 때마다 `mod.reload()`를 호출한다. Player에서는 `EngineConfig` 플래그로 끈다.
2. **로그 정책** — 릴리스 빌드에서는 매 프레임 로그를 남기지 않고, 파일 로그만 `%LOCALAPPDATA%\<name>\`에 남긴다.
3. **CI** — `game_builder` 단위 테스트 + 템플릿 `--smoke-test`. 지금은 `.github/`가 없으므로 CI 도입부터 필요한지 정해야 한다(확인 필요).
4. **배포 크기 줄이기** — 표준 라이브러리 줄이기, `.pyc` 미리 컴파일, zip 묶기.
5. **참조되지 않는 에셋 보고서**(MASTER_PLAN §3 #6) — 프로젝트에 있지만 어느 씬에서도 참조되지 않는 에셋 목록. 인게임 UI가 참조하는 폰트·이미지도 빌드 단계 3과 같은 규칙으로 판정한다.
6. **엔진 DLL 나누기**(선택) — `EDITOR_SOURCES`를 별도 라이브러리로 빼서 Player 크기를 줄인다.

(옛 Phase 5 항목 "에디터 전용 요소 확인"은 코드 확인으로 끝나 §3.5에 기록했고, 디버그 그리드 끄기는 Phase 2 필수 항목으로 옮겼다.)

---

## 5. 파일 변경 요약

| 구분 | 파일 | Phase |
|---|---|---|
| 신규 | `engine/scene/SceneIO.{h,cpp}`, `engine/tests/SceneIOTests.cpp` | 1 |
| 신규 | `engine/editor/project/{__init__,project}.py`, `engine/editor/project_actions.py` (File 메뉴) | 1 |
| 신규 | `engine/runtime/PlayerMain.cpp`, `GameConfig.{h,cpp}`, `runtime/python/runtime_bootstrap.py`, `engine/tests/GameConfigTests.cpp` | 2 |
| 신규 | `engine/runtime/stage_template.py`, `engine/tests/data/smoke_game/` | 3 |
| 신규 | `engine/core/Utf8Path.h` (UTF-8 경로 규칙, §9) | 2·4 |
| 신규 | `engine/editor/gamebuild/game_builder.py`, `build_dialog.py`, `test_game_builder.py`(프로젝트 테스트 포함) | 4 |
| 변경 | `engine/CMakeLists.txt` (씬 소스, Player 타깃, install 재구성, manifest) | 1·2·3 |
| 변경 | `engine/scripts/run_editor.bat.in` (빌드 폴더로 이동하지 않게) | 3 |
| 변경 | `engine/bindings/ECSBindings.cpp` (SaveScene/LoadScene) | 1 |
| 변경 | `editor/main.py`, `demo_scene_seed.py`, `panels/{prefab_browser,scene_hierarchy,motion_mixer}.py` (프로젝트 경로, 메뉴) | 1·4 |
| 변경 | `editor/core/scene_action_playback.py`, `sound_player.py` (PySide6를 필요할 때만 import) | 2 |
| 변경 | `engine/core/Engine.{h,cpp}`, `renderer/Renderer.cpp` (디버그 그리드 표시 플래그) | 2 |
| 변경 | `engine/ecs/ScriptSystem.cpp` (다시 불러오기 플래그) | 5 |
| 선행 과제 | 인게임 UI 컴포넌트·시스템·렌더러 — 이 계획이 아니라 UI 계획의 UI Phase 1~3에서 만든다 | 이 계획 Phase 4 완료 전 |
| 갱신 | `ROADMAP.md`(착수·검증 기록), `docs/MASTER_PLAN.md` P0-b 상태 | 각 Phase 종료 시 |

---

## 6. 리스크와 미해결 사항

### 미해결 (착수 전에 정해야 함)

1. ~~②는 Python을 어떤 방식으로 포함하는가?~~ — **종료(2026-10-07): Player용 Python을 템플릿에 넣는다(D8).** 아래는 원래 문제 설명이다. 지금은 `python editor/main.py`로 PC에 설치된 Python에 의존한다. ②가 전용 Python 폴더를 동봉하는지, PyInstaller·Nuitka로 실행 파일 하나에 묶는지에 따라 D8의 복사 원본이 달라진다. 실행 파일로 묶으면 표준 라이브러리가 exe 안에 들어가 골라 복사하지 못할 수 있다. 그 경우 **Player용 Python 런타임을 `runtime/win-x64/`에 따로 동봉**하는 쪽이 안전하다(크기는 늘어난다). → S5에서 정한다.
2. **프로젝트 도입이 기존 에디터 흐름을 깨지 않는가?** 고정 경로 7곳을 바꾸는 작업이라 회귀 위험이 있다. 프로젝트를 열지 않으면 샘플 프로젝트(= 지금의 `engine/assets`)를 쓰는 방식으로 기존 흐름을 유지한다. Phase 1이 끝나면 에디터 Play·Motion Editor·프리팹 브라우저를 직접 실행해 점검한다.
3. **엔진 내장 리소스의 범위** — 어떤 파일을 `builtin/`(②가 소유)에 두고 어떤 파일을 프로젝트 쪽(게임 개발자가 소유)에 둘지. 셰이더는 `builtin/`으로 확정했다. 기본 메시(큐브 등)는 정해야 한다. UI Phase 2에서 기본 폰트·UI 셰이더가 생기면 같은 기준으로 정한다.
4. ~~인게임 UI의 현재 구현 위치와 성격~~ — **종료(2026-10-07).** 구현 없음(코드 확인). 선행 과제로 정했고, 범위는 UI Phase 1~3이다. UI 에디터 패널은 선행 과제가 아니며, 게임 개발자의 자유도를 제한하지 않는 기반으로 설계한다(§0.5).

### 리스크

| 리스크 | 영향 | 대응 |
|---|---|---|
| ~~Python 3.14 + pybind11 2.11.1에서 Python 내장이 동작하는지 미검증~~ | — | **해소(S1 통과).** |
| `Engine::Initialize()` 독립 창 경로를 에디터가 쓰지 않아 **실제로 쓰인 적이 거의 없다** | 입력·창 크기 변경·전체 화면 버그 | S2에서 확인한다. `EngineTests.cpp`는 `engine.Initialize(config)`를 호출하지만 서브시스템 초기화 순서만 검사한다(코드 확인). 렌더·입력·창 크기 변경을 다루는 Player 확인 테스트를 추가한다. |
| `SerializeRegistry`는 **등록되지 않은 컴포넌트를 저장하지 않는다**(`PrefabInstanceComponent.h` 주석, 코드 확인) | 저장 → 불러오기에서 데이터가 사라진다. 인게임 UI 컴포넌트도 등록을 빠뜨리면 UI가 통째로 사라진다 | Phase 1에 "등록된 모든 컴포넌트 왕복" 테스트를 넣고, 등록되지 않은 컴포넌트는 저장할 때 경고한다. |
| **인게임 UI 선행 과제의 일정** | UI Phase 1~3(UI 계획 §5.1 추정 4주)이 끝나지 않으면 Build Game v1을 완료로 표시할 수 없다 | 이 계획의 Phase 0~3은 인게임 UI와 독립이므로 병행한다. "인게임 UI 코드가 PySide6를 import하지 않음"을 UI 계획의 완료 조건으로 건다. 형제 순서(개선안 P2-8)를 UI Phase 2 전에 처리한다. |
| 게임 스크립트가 서드파티 패키지나 PySide6를 import | 빌드 출력 폴더 실행 시 `ImportError` | 빌드 단계 5에서 경고한다. v1은 표준 라이브러리만 지원한다. |
| Player 템플릿과 에디터의 버전이 다름(일부만 업데이트된 경우 등) | 씬 형식·ABI 불일치 | 빌드 단계 1에서 `runtime_manifest.json`으로 검증한다. 씬·프로젝트 파일에 `version` 필드를 둔다. |
| Player가 VC 런타임·OpenGL 버전에 의존 | 개발 도구가 없는 환경에서 Player 시작 실패 | Player 템플릿에 VC 런타임 DLL을 같이 넣는다. GL 컨텍스트를 만들지 못하면 원인을 메시지 상자로 보여 준다. |
| Windows 전용 | 다른 OS용 빌드 불가 | v1 범위 밖. `IPlatform` 추상화가 있어 나중에 확장할 수 있다. |

---

## 7. 범위 밖 (v1에서 하지 않는 것)

- **②의 설치 프로그램·패키징·코드 서명.** 단, §3.1의 설치 폴더 구성과 Phase 3의 템플릿 배치는 이 계획의 범위다.
- 게임 빌드 오디오(엔진 오디오 백엔드) — SOUND_LITE_PLAN §6.1.
- Windows 이외 플랫폼, 모바일·웹(WebGL) 빌드.
- 에셋 압축·암호화·아카이브(`.pak`), 서드파티 Python 패키지 동봉.
- 게임 실행 중 씬 전환 API(`LoadScene` 호출). Phase 1의 `LoadSceneFromFile`이 기반이 되므로 나중에 추가하기 쉽다.
- 에디터 안에서 Player를 바로 실행하는 "Build And Run". Phase 4 이후 `subprocess` 호출 하나로 추가할 수 있다.
- **인게임 UI 시스템 자체의 설계·구현** — UI 계획의 몫이다. 이 문서는 UI Phase 1~3 완료를 선행 조건으로 걸고, 씬 파일 형식·의존성 수집·렌더 순서가 맞물리는 지점(§0.5)만 다룬다.
- ③의 게임 콘텐츠 제작, ④의 배포·설치, ⑤ 지원 — 이 프로젝트는 ①·②만 다룬다(§0.2).

---

## 8. 권장 착수 순서

```text
이 계획:   Phase 0(검증) → Phase 1(프로젝트·씬 저장) → Phase 2(Player) → Phase 3(템플릿 배치) → Phase 4(Build Game) → Phase 5(다듬기)
UI 계획:                     └→ UI Phase 1 → UI Phase 2 → UI Phase 3 ───────────────→ (이 계획 Phase 4 완료 전까지)
```

- Phase 0과 Phase 1은 서로 독립이라 동시에 진행해도 된다.
- 인게임 UI 선행 과제는 이 계획의 Phase 0~3과 독립이라 병행한다. 다만 UI 컴포넌트를 씬 파일에 저장하려면 씬 저장 기능이 있어야 하므로, **UI Phase 1은 이 계획의 Phase 1 다음에 시작한다.**
- **Phase 0~2**까지 하면 "개발 PC에서 에디터 없이 게임 실행"이 된다. **Phase 3~4와 인게임 UI 선행 과제**까지 마쳐야 "설치된 에디터(②)에서 게임을 빌드한다"는 목표가 충족된다.
- 가장 큰 불확실성이던 S1과 미해결 1은 해소됐다(§9). 남은 가장 큰 일정 요인은 **인게임 UI 선행 과제**다.

---

## 9. 구현 기록 (2026-10-07)

검증 수준 표기는 CLAUDE.md 관례 2를 따른다: **컴파일**(빌드만 통과) / **단위 테스트** / **실제 실행**(프로그램을 띄워 결과를 봄, 화면 캡처 여부를 따로 적음).

### 9.1 단계별 상태

| 단계 | 상태 | 검증 수준 |
|---|---|---|
| Phase 0 (S1~S6) | 완료 | 실제 실행(S2는 화면 캡처). S5는 D8 결정으로 대체 |
| Phase 1 — SceneIO | 완료 | 단위 테스트 10개(`SceneIOTests`: 왕복, 계층·이름, 등록된 모든 컴포넌트, 에러 4종, 실패 시 기존 월드 보존, 한글 경로) |
| Phase 1 — 프로젝트·메뉴·경로 7곳 | 완료 | 단위 테스트(`test_game_builder.py`의 프로젝트 테스트 5개) + 실제 실행(에디터 자동화: 샘플 프로젝트 시작 → 새 프로젝트(한글 경로) → 엔티티 추가 → 저장 → 새 씬 → 다시 열기 → 엔티티 3개 복원, 화면 캡처) |
| Phase 2 — Player | 완료 | 단위 테스트(`GameConfigTests` 6개) + 실제 실행(개발 빌드·템플릿·설치본 각각 스모크 테스트, 창 화면 캡처, 스크립트 `on_create`/`on_update`/`on_destroy` 로그) |
| Phase 3 — 템플릿·설치 | 완료 | 실제 실행: `player_template` 조립(48개 파일, 30.2MB), CTest `player_template_smoke` 통과, `cmake --install` 결과만으로 템플릿 스모크 테스트·Build Game CLI·`run_editor.bat` 에디터 시작(엔진 초기화까지) 확인 |
| Phase 4 — Build Game | 완료(인게임 UI 제외) | 단위 테스트 18개(가짜 템플릿 조립 + 실제 템플릿으로 한글 폴더 프로젝트를 빌드해 다른 한글 경로로 옮겨 실행) + 실제 실행(Build Settings 저장, 진행 대화상자, 취소 버튼, 결과 상자 — 대화상자 자동화). 에디터 흐름으로 빌드한 게임을 띄워 화면 캡처 |
| 최종 통과 기준(환경 A/B) | **미확인** | 깨끗한 PC(VM) 두 대로 확인하지 않았다. 개발 PC에서 설치본·템플릿만으로 실행되는 것까지 확인했다 |
| 인게임 UI 선행 과제 | **미착수** | §0.5 |
| Phase 5 | 일부 | 1번(스크립트 다시 불러오기 끄기, `EngineConfig::scriptHotReload`)만 했다. Player 로그 레벨은 Info로 낮췄다 |

### 9.2 계획에서 달라진 점

- **Python 런타임을 템플릿에 동봉(D8)**, `builtin/` 보류(D5), 패키지 이름 `editor/gamebuild`(D3) — 각 결정의 이유는 해당 행에 적었다.
- **씬 파일 형식**: `SerializeRegistry()` 출력에 `format`·`version` 키를 붙인 한 단계 구조다(`{"format", "version", "entities"}`). 계획의 `"entities": <SerializeRegistry 출력>`(두 단계)이 아니다.
- **프로젝트 테스트 위치**: 계획의 `editor/test_project.py`·`test_scene_save_load.py` 대신 `gamebuild/test_game_builder.py`에 넣었다. 씬 저장 왕복은 C++ `SceneIOTests`와 에디터 자동화로 확인했다.
- **새 프로젝트의 시작 씬**에 Main Camera 하나를 넣는다(빈 씬이면 카메라가 없어서 화면이 비었다 — 실제 실행에서 발견).
- **`World.NewScene()` 바인딩**을 추가했다(File > New Scene에 월드를 비울 방법이 없었다).
- **`Engine.SetAssetRoot()`**를 추가했다. 프로젝트를 바꾸면 에셋 기준 폴더와 `RenderSystem`의 메시·텍스처 캐시가 다음 프레임에 교체된다(GL 자원 해제는 자기 컨텍스트가 current인 프레임 안에서 한다).
- 하지 않은 것: **데모 월드를 `Demo.scene.json`으로 저장**. 프리팹 인스턴스의 `PrefabInstanceComponent.prefabPath`가 절대 경로로 저장되어, 저장소에 이 PC의 경로가 들어가고 프로젝트를 옮기면 Revert가 깨진다. 프리팹 경로를 프로젝트 상대 경로로 바꾸는 것이 먼저다(후속 과제). → 2026-10-15 해결: 프리팹 경로 상대화(X-1a) 후 `engine/assets/scenes/Demo.scene.json`을 샘플 프로젝트의 시작 씬으로 삼았다(USABILITY_IMPROVEMENT_PLAN X-1b, §5.5).

### 9.3 구현 중 발견해 고친 기존 버그

모두 코드 안에 증상·원인 주석을 남겼다(CLAUDE.md 관례 1).

| 버그 | 증상 | 원인 | 위치 |
|---|---|---|---|
| `ScriptSystem` 미등록 | `ScriptComponent`를 붙여도 `on_create`/`on_update`가 한 번도 호출되지 않음(에디터 Play 포함, 에러 없음) | 어떤 World에도 등록되지 않았다 | `core/Engine.cpp` `CreateWorld` |
| 스크립트 예외 전파 | `on_update`의 예외가 `TickFrame` 밖까지 새어 나감 | 호출을 감싸지 않았다 | `ecs/ScriptSystem.cpp` |
| 로그 레벨이 DLL마다 따로 | Player에서 `SetMinLevel(Info)`를 해도 엔진 TRACE가 60프레임에 1000줄 이상 | `static inline` 헤더 변수가 모듈마다 사본을 가짐 | `core/logging/Logger.{h,cpp}` |
| RenderGraph 경고 반복 | 매 프레임 "Graph not compiled" 경고 두 줄 | 패스가 0개라 컴파일되지 않은 그래프를 매 프레임 정리 | `renderer/Renderer.cpp` `EndFrame` |
| 창 제목 첫 글자만 | 독립 실행 창 제목이 "P" | 유니코드 창 클래스에 `DefWindowProcA` 사용 | `platform/Win32Platform.cpp` |
| **한글 경로** | 한글 폴더의 프로젝트에서 에디터가 매 프레임 `No mapping for the Unicode character exists in the target multi-byte code page.` 예외, 빌드한 게임은 시작 직후 종료 | UTF-8 경로 문자열을 `std::filesystem::path(std::string)`/`fopen`/`ifstream(const char*)`이 ANSI로 해석 | `core/Utf8Path.h` 도입, `RenderSystem`·`Mesh`·`TextureImporter`·`AssetManager`·프리팹 명령·`PrefabAsset`·`SceneIO`·`GameConfig` |
| 빌드 진행 대화상자 교착 | Build Game이 진행 대화상자를 띄운 채 멈춤 | 작업자 스레드의 시그널을 일반 함수가 받아 작업자 스레드에서 `thread.wait()` | `editor/gamebuild/build_dialog.py` |
| 취소 버튼 무효 | 취소가 빌드가 끝난 뒤에야 도착 | 취소 메서드가 작업자 스레드 큐로 감 | 같은 파일 |
| 설치본 실행 스크립트 | 설치된 `run_editor.bat`가 빌드 폴더로 이동 | `cd /d @CMAKE_CURRENT_BINARY_DIR@` | `engine/scripts/run_editor.bat.in` |
| `PathResolver` 호출 실패 | 에디터 시작마다 `module 'quarterflying' has no attribute 'PathResolver'` | 없는 바인딩 호출 | `editor/main.py` `_apply_config` |
| `test_demo_scene.py` 무출력 종료 | 종료 코드 127, 메시지 없음 | `QApplication` 없이 `QWidget` 생성 | `editor/test_demo_scene.py` |

### 9.4 남은 일

1. **인게임 UI 선행 과제**(UI Phase 1~3) — Build Game v1 완료 조건.
2. **최종 통과 기준**: 깨끗한 Windows(VM)에서 환경 A/B 확인.
3. ~~**프리팹 경로를 프로젝트 상대 경로로** 저장(§9.2 마지막 항목).~~ 2026-10-15 완료 — 씬 version 2, `core/ProjectPath.h`(USABILITY_IMPROVEMENT_PLAN X-1a, §5.4).
4. **설치본에 서드파티 파일이 섞임**: `cmake --install`이 `include/`, `lib/`, `share/`, `glew32.dll`도 설치한다(glew·fmt·googletest의 자체 설치 규칙). 기능에는 영향이 없지만 ②의 구성과 다르다.
5. **Player 콘솔 창**: `QF_PLAYER_CONSOLE` 기본값이 ON이라 게임 실행 시 콘솔 창이 같이 뜬다. 배포용 템플릿은 OFF로 빌드할지 정해야 한다(OFF면 로그를 파일로 남기는 Phase 5 2번이 필요).
6. Phase 5 2~6번.

---

## 요약 — 이 문서가 지키는 두 가지 경계

1. **PySide6는 ②의 에디터 GUI 전용이다.** Build Game 로직(D3)도, Player도, 빌드 출력 폴더도 PySide6를 쓰지 않는다.
2. **인게임 UI는 에디터 GUI가 아니라 엔진 기능이다.** 아직 구현되어 있지 않고(코드 확인), Build Game v1의 선행 과제다. ②에서 ECS 컴포넌트로 구성해 씬에 저장하고, Player의 엔진 렌더러가 PySide6 없이 그린다. 엔진은 기본 단위만 제공하고 게임 개발자의 디자인 자유도를 제한하지 않는다. 빌드 출력 폴더에서 인게임 UI가 빠지거나 PySide6를 요구하면 결함이다.
