# 실사용 격상 개선안 (2026-10-08 원안 → 2026-10-13 아키텍처 규칙 기준 개정)

목표: *에디터에서 프로젝트를 열고 → Play → 키보드로 캐릭터를 움직이고 → 마우스로 카메라를 돌리고 → Stop 후 저장*하는
"한 판"을 문서 없이 할 수 있게 한다. 같은 조작이 Build Game으로 만든 게임(Player)에서도 동작해야 한다.

원안(2026-10-08, 데모 시드·사운드·머티리얼 리뷰 + GUI·모션 에디터·게임 조작 개선점)은
[`archive/USABILITY_REVIEW_2026-10-08_ORIGINAL.md`](archive/USABILITY_REVIEW_2026-10-08_ORIGINAL.md)에
원문 그대로 보관했다. 이 문서는 그 원안을 **§0의 아키텍처 규칙에 맞춰 고친 현행본**이다. 고친 이유는 §1에 있다.
2026-10-15: 원인 추적·영향 범위 규칙(R13, `CLAUDE.md` 관례 4)을 더하고 항목별 영향 범위 표(§2.6)를 추가했다.
2026-10-15(2차): 착수 전 검토 반영 — G0를 G0a(액션 모델·스키마·프레임 순서·저장 API)/G0b(Python·Player 연결)로,
X-1을 X-1a(프리팹 경로 마이그레이션)/X-1b(Demo 씬 전환)로 나눴다. R-2 기본색과 X-1 씬 버전을 결정했다(결정 2026-10-15).
G1 점프의 dt 적분, R-1 라이트 선택 규칙, E-2 포커스 상실 처리 범위를 정했다.

## 읽는 법

| 표기 | 뜻 |
|---|---|
| **E-n / A-n / G0~G3 / R-n / X-n** | 이 문서의 항목 번호: 에디터 GUI / 모션 에디터 / 게임 입력·조작 / 렌더링·머티리얼 / 샘플 콘텐츠 |
| **규칙 Rn** | §0의 아키텍처 규칙 |
| C1~C4, D1~D11, Phase n, UI Phase n | [`GAME_BUILD_PLAN.md`](GAME_BUILD_PLAN.md)의 번호 |
| 개선안 P1-1 등 | [`REVIEW_BASED_IMPROVEMENT_PLAN.md`](REVIEW_BASED_IMPROVEMENT_PLAN.md)의 번호 |
| **(코드 확인)** / **(실행 확인)** / **(문서 기준)** / **(확인 필요)** | `GAME_BUILD_PLAN.md`의 상태 표시와 같다 |

사실 확인 기준: HEAD `35429a4` + 커밋 안 된 작업 트리(2026-10-13). 원안이 밝힌 기준 커밋 `766b5f8`은
이 저장소에 없어서(`git cat-file` 실패) 원안의 사실 서술을 전부 다시 확인했다.

---

## 0. 적용한 아키텍처 규칙

| # | 규칙 | 출처 |
|---|---|---|
| R1 | **범위는 ①·②뿐.** ③(게임 개발자 작업)·④(빌드 출력)는 ②가 읽는 입력 형식과 써 내는 출력 형식으로만 다룬다. ⑤(게임 사용자) 대응은 범위 밖이다. | `GAME_BUILD_PLAN.md` §0.2 (결정 2026-10-07) |
| R2 | **PySide6는 ②의 에디터 GUI 전용.** 게임 화면 안에서 동작해야 하는 것은 Qt 없는 엔진 기능으로 만든다. Player·빌드 출력 폴더에 Qt가 들어가면 결함이다. | 같은 문서 C4, D11, "두 가지 경계" |
| R3 | **에디터 저장과 Player 로드는 같은 코드.** 게임에 영향을 주는 씬 데이터는 `ComponentRegistry`에 등록된 ECS 컴포넌트로 `*.scene.json`(`SceneIO`)에 들어가야 한다. 에디터 Python만 읽는 경로(`scene.json` → `scene_instantiation.py`)에 두면 Player에서 사라진다. | 같은 문서 D6 |
| R4 | **게임 데이터는 프로젝트 폴더에.** 에디터 설정은 `config.json`, 프로젝트 편집 설정은 `project.json`, 실행 설정은 Build Game이 만드는 `game.json`. 새 데이터 파일은 §3.2(프로젝트 폴더)·§3.3(빌드 출력) 형식과 빌드 단계 3의 수집 규칙("컴포넌트 이름 → 경로 필드" 표)에 함께 추가한다. | 같은 문서 C3, D4, D10, Phase 4 |
| R5 | **엔진은 기반만 제공하고 게임 개발자의 자유도를 제한하지 않는다.** 조합 가능한 기본 단위를 주고, 완성품은 수정 가능한 샘플(프리팹·스크립트)로만 둔다. 기본값은 덮어쓸 수 있게 한다. 제품의 차별점은 "Python으로 게임 규칙을 붙이는 것"이다. | 같은 문서 §0.5 (결정 2026-10-07), `MASTER_PLAN.md` §5 |
| R6 | **모듈 의존 방향.** ECS 코어는 renderer를 모른다(렌더러를 아는 것은 `RenderSystem` 같은 시스템 파일뿐). 렌더러는 ECS 컴포넌트가 아니라 순수 데이터를 받는다. 에디터는 `engine_binding` 한 곳으로만 엔진에 닿고, 패널끼리 import하지 않는다. | `ARCHITECTURE_COUPLING_AUDIT.md` §2.4, §2.5, §3.1 |
| R7 | **에디터 편집은 `EditorAPI` 명령을 거친다**(Undo/Redo). 부모 변경은 `SetParent`. | 개선안 P1-5, `CODE_REVIEW_2026-10-07.md` §4-5 |
| R8 | **입력은 큐를 거친다.** 외부(Qt) 스레드는 `Engine::PushInputEvent`로만 입력을 넣고, `InputState`는 `TickFrame` 안에서만 바뀐다. | `core/Engine.h:172-177` (코드 확인) |
| R9 | **핸들 도메인을 섞지 않는다**(논리 / RenderGraph / GPU). | `core/Types.h:136-143` |
| R10 | **"안 쓰인다"는 이유로 지우지 않고 연결한다.** 지우기 전에 내용을 확인한다. | 사용자 지침 (2026-09-30) |
| R11 | **경로 문자열은 UTF-8**, C++에서는 `core/Utf8Path.h`를 거친다. 경로를 다루는 테스트에는 한글 경로를 넣는다. | `CLAUDE.md` 빌드 환경 메모 |
| R12 | **검증 3단계를 구분해 적는다**(컴파일 / 단위 테스트 / 실제 실행). 조용히 틀리는 대신 원인이 보이는 메시지를 낸다. | `CLAUDE.md` 코드 품질 관례 2·3 |
| R13 | **원인 추적과 영향 범위가 설계 단계에서 드러나게 한다.** 기능 진입점은 한 곳, 모듈 경계는 순수 데이터, 실패는 자기 단계 이름과 출처 태그로 남긴다. 항목마다 바뀌는 곳·영향받는 곳·호환성·고장 시 갈리는 지점·회귀 확인을 적는다(§2.6). | `CLAUDE.md` 코드 품질 관례 4 (2026-10-15) |

---

## 1. 원안 검토 결과

### 1.1 규칙 위반

| 원안 | 위반 규칙 | 문제 | 고친 방향 |
|---|---|---|---|
| G1 — C++ `PlayerMoveSystem` + `CharacterMoveComponent { speed, sprintMultiplier }`. WASD 이동·Shift 스프린트·Space 점프를 엔진이 구현 | R5, R1 | 이동 방식·점프 규칙은 게임 규칙(③)이다. 엔진에 고정하면 그 구현이 게임 조작의 상한이 된다. 진짜 빈 곳은 따로 있다: **Python 스크립트가 입력을 읽을 방법이 없다**(코드 확인: 바인딩에는 입력을 *넣는* `InputEvent`/`PushInputEvent`만 있고, `ScriptSystem`은 스크립트에 `Entity`만 넘긴다 — `ecs/ScriptSystem.cpp:113`). | 엔진은 **입력 액션 맵 + 스크립트용 입력 API**(G0)만 제공한다. 캐릭터 이동은 샘플 프로젝트의 **수정 가능한 Python 스크립트 + 샘플 프리팹**으로 둔다(G1). |
| E-1·§3.4 단축키 — Play 중 `Space` = Pause, `Esc` = Stop | R8, R5 | 뷰포트는 이미 `Space`·`Escape`를 게임 키로 엔진에 넘긴다(`viewport.py:185-218`, 코드 확인). 원안 G1도 `Space`를 점프로 쓴다. 에디터가 이 키를 가로채면 게임이 그 키를 못 쓴다. | Play 중 에디터 단축키는 **수식키 조합만** 쓴다. 맨 키는 뷰포트에 포커스가 있으면 게임에 간다(E-1). |
| §1.2 "Help → Keyboard Shortcuts(**아래 조작키와 공유 데이터**)" | R1, R4 | 에디터 단축키는 ②의 설정이고, 게임 조작 맵은 프로젝트 데이터라 ④로 나간다. 데이터를 공유하면 에디터 설정이 빌드 출력에 섞이거나, 게임 개발자가 바꾼 조작이 에디터 단축키를 바꾼다. | 두 데이터를 분리한다. 단축키 표는 에디터 내부 표에서, 조작 표는 프로젝트의 액션 맵에서 읽는다(E-5, G0). |
| G0 `actions.json` — "에디터/플레이어가 같은 맵 로드" | R4 | 파일 위치와 빌드 포함 방법이 없다. 이대로면 에디터에서는 되고 빌드한 게임에서는 맵이 없다. | `<프로젝트>/input/actions.json`으로 정하고, `GAME_BUILD_PLAN.md` §3.2·§3.3과 빌드 단계 3에 추가한다(G0). |
| G2 — Play 첫 프레임 조작 안내 오버레이, "Settings → Key Bindings", 화면 중앙 "Click to capture" | R1, R2, R5 | 게임 화면 안의 안내·키 재설정 화면은 **인게임 UI**다. 인게임 UI는 아직 없고(UI Phase 1~3 선행), 무엇을 띄울지는 게임 개발자(③)가 정한다. 최종 사용자용 키 재설정은 ⑤ 대응이다. | 둘로 나눈다. (a) **에디터 뷰포트 위의 Qt 오버레이**(에디터 Play 모드에서만, ④에 안 나감)는 에디터 GUI로 허용(E-3). (b) 게임 안 안내·재설정 화면은 계획에서 뺀다. 엔진은 재설정 **API**만 제공한다(G0). 에디터의 "Key Bindings"는 프로젝트 `actions.json`을 편집하는 에디터 패널로 바꾼다(G2). |
| R-1 — 라이트를 "`scene.json` lights 또는 DirectionalLight 컴포넌트"로 연결. 머티리얼은 "`scene.json` material과 연동" | R3 | `scene.json`은 샘플 프로젝트의 옛 형식이고 에디터 Python만 읽는다(`scene_instantiation.py`, `demo_scene_integration.py:67`). Player는 `*.scene.json`만 읽는다. `scene.json`에 연결하면 **에디터에서만 조명이 바뀌고 빌드한 게임에서는 원래대로** 보인다. | 라이트와 머티리얼 값은 **ECS 컴포넌트 필드**로 만든다. `scene.json`은 불러올 때 그 필드로 옮겨 주기만 한다(R-1, R-2). |
| R-1 — "`SceneMeshRenderer` lightDir에 씬 라이트 연결" | R6 | 렌더러가 라이트 컴포넌트를 직접 읽으면 renderer → ecs 의존이 새로 생긴다. | `RenderSystem`이 컴포넌트를 읽어 순수 데이터(`SceneLighting` 구조체)로 렌더러에 넘긴다(R-1). |
| §5 P0 — "`RenderableComponent`에 color 필드 **또는** 최소 Material 구조체 + **배치 키 반영**", P1 — "`materialHandle`: 쓰거나 정리" | R9, R10 | (1) 배치 키에 색을 넣으면 색마다 배치가 갈라진다. 그럴 필요가 없다 — `InstanceData`에 이미 인스턴스별 `color`/`roughness`/`metallic`이 있다(코드 확인: `RenderSystem.cpp:248-250`에서 채우고, 셰이더가 `InstanceColor`를 쓴다 — `SceneMeshRenderer.cpp:77`). (2) `materialHandle`을 그대로 둔 채 색 필드를 더하면 머티리얼 지정 방법이 셋(핸들·인라인 값·`texturePath`)이 된다. `CODE_REVIEW_2026-10-07.md` §5 P1(`meshHandle`/`meshPath` 이중성)과 같은 문제를 하나 더 만든다. | 인라인 값은 **인스턴스 데이터**로만 보낸다(배치 키 변경 없음). `materialHandle`은 지우지 않고(R10) "읽는 코드 없음, 머티리얼 에셋 도입 때 결정"을 주석으로 남긴 뒤, `meshHandle`/`meshPath` 정리와 **같은 결정으로** 다룬다(R-2, R-4). |
| §4.3 — 셰이더를 `.cpp` 문자열에서 파일로 | R4 | D5: 셰이더가 C++ 안에 있어서 `builtin/` 리소스 폴더가 필요 없었다. 파일로 빼면 엔진 리소스를 ②와 Player 템플릿에 넣는 `builtin:` 규칙을 먼저 정해야 한다. 프로젝트 `assets/`에 두면 엔진 셰이더가 게임 콘텐츠가 된다. | 파일화는 D5의 `builtin/` 결정과 묶어 뒤로 미룬다(R-4). `engine/assets/shaders/*.vert/frag`(샘플 프로젝트 안, 현재 읽는 코드 없음 — `SceneMeshRenderer.h:13-16`이 쓰지 않는 이유를 적어 둠)는 지우지 않는다(R10). |
| §1 개선 제안 — 데모 시드를 `config` 또는 `assets/demo/seed.json`으로 외부화. 1.2 — `project.json`에 `autoSeedDemo: false` | R3, R4 | 씬 형식이 셋(`scene.json`, `*.scene.json`, `seed.json`)이 된다. 또 에디터 설정(`config.json`)에 씬 내용이 들어간다. | 샘플 프로젝트의 데모 월드를 **보통의 씬 파일**(`scenes/Demo.scene.json`)로 바꾼다. 그러면 플래그도 시드 형식도 필요 없다(X-1b, 선행 X-1a). |
| G1 — "DemoSeed에 Player 엔티티 + Orbit 카메라 한 세트" | R3, R5 | 코드 시드는 에디터에만 있고 Player에는 없다. 시드를 줄이자는 X-1과도 반대 방향이다. | 샘플 캐릭터 프리팹을 `Demo.scene.json`에 넣는다(G1). |
| E-4 — "Delete: 선택 엔티티 삭제", "Ctrl+D: 복제" | R7, R8 | 방향은 맞지만 조건이 없다. Play 중에는 `Delete`도 게임 키일 수 있다. | Edit 모드에서 Scene Hierarchy나 뷰포트에 포커스가 있을 때만, 기존 `DestroyEntityCommand`/`DuplicateEntityCommand`를 거친다(E-4). |
| §3.3 E-2 — "포커스 아웃 시 `ReleaseAll()`(이미 InputState에 API 있음)" | R8 | `InputState::ReleaseAll()`은 있지만(`input/InputState.h:39`) 바인딩이 없고, Qt 스레드에서 직접 부르면 R8을 어긴다. | 입력 큐에 "포커스 잃음" 이벤트를 넣고 `TickFrame` 안에서 `ReleaseAll()`을 부른다(E-2). |
| 모든 "완료 기준" | R12 | 어느 검증 단계까지가 완료인지 없다. | 각 항목에 검증 기준을 단계별로 적는다. |
| 문서 위치 — 빌드 가이드(`EDITOR_APP_BUILD_GUIDE.md`) 뒤에 빈 줄을 두고 덧붙임 | 문서 체계 | 빌드 방법 문서와 개선 계획이 한 파일에 섞였다. | 이 파일로 분리했다. 빌드 가이드에는 링크만 남겼다. |

### 1.2 사실 정정

| 원안 서술 | 실제 | 근거 |
|---|---|---|
| "프로덕션 씬과 데모가 같은 World에 섞임 — 시작할 때마다 검증 엔티티가 들어감", "열 때마다 DemoSeed가 끼어들면 실작업 씬이 오염됨" | **샘플 프로젝트에서만** 시드한다. 사용자 프로젝트는 시작 씬을 열거나 빈 씬으로 시작한다. 시드는 세션당 한 번이다. 남은 문제는 "샘플 프로젝트의 데모 월드가 씬 파일이 아니라 코드"라는 점이다. | `main.py:561-574` (코드 확인) |
| Test Cube "주석에 '메시 파이프라인 없음 → 절차적 큐브'" | 메시 경로(`meshPath`)는 2026-09-30에 연결됐다. Test Cube는 텍스처 검증용으로 남은 것이다. | 개선안 P0-2 (문서 기준) |
| "Player(게임 빌드)에는 엔진 오디오 없음"을 결함처럼 서술 | 결함이 아니라 결정이다. 게임 빌드 오디오는 v1에서 뺐다. | `GAME_BUILD_PLAN.md` D9 |
| "Pause는 연결돼 있으나 단축키·상태 표시가 약함" | 맞다. `act_pause`에 단축키가 없다. | `main.py:196` (코드 확인) |
| "About은 메뉴만 있고 동작 불명확" | 메뉴 항목에 연결된 슬롯이 없다(눌러도 아무 일도 없음). | `main.py:204-205` (코드 확인) |
| "Player에서 동일 조작 — 입력은 플랫폼 루프에 달려 있음" | Player(독립 창)는 `Win32Platform`의 `WindowProc`이 키·마우스를 `PushInputEvent`로 넣는다. 에디터와 같은 `InputState`에 도착한다. | `Win32Platform.cpp:533-609` (코드 확인, 실행은 안 함) |

---

## 2. 개정된 개선안

### 2.1 게임 입력·조작 (G) — 가장 먼저

```text
Qt 뷰포트(에디터) ─┐
Win32 WindowProc(Player) ─┴→ PushInputEvent → [입력 큐] → TickFrame: InputState (하드웨어 상태)
                                                              │
                                     InputActions (프로젝트 assets/input/actions.json, 덮어쓰기 가능)
                                                              │
                                 ┌────────────────────────────┴──────────────┐
                    C++ 시스템(CameraRigSystem 등)          Python 스크립트(ScriptComponent)
                                                             └ 샘플: character_controller.py
```

**G0a — 액션 모델 (C++ 엔진 코어, Python·Player 연결 전)**

C++ 로더·Python API·G2 편집 패널이 각자 형식을 짐작하지 않도록, 스키마·합성 규칙·프레임 순서·저장 API를
여기서 한 번 정하고 셋 다 이것만 따른다(R13 — 진입점 한 곳).

- **모듈**: `input/InputActions.{h,cpp}`. `InputState`만 읽는다(renderer·ecs 의존 없음, R6).
- **파일**: `<프로젝트>/assets/input/actions.json`(구현 때 변경 — 처음 계획은 `<프로젝트>/input/`였으나, 샘플 프로젝트의 루트가 `engine/`이고 `engine/input/`은 C++ 소스 폴더라 프로젝트 데이터가 소스와 섞인다. assets 아래는 샘플·사용자 프로젝트 모두 같은 규칙이다). 스키마:

  ```json
  {
    "format": "quarterflying.input-actions",
    "version": 1,
    "actions": {
      "Jump":  { "type": "button", "bindings": ["Key:Space", "Mouse:Right"] },
      "MoveX": { "type": "axis",   "bindings": [ { "negative": "Key:A", "positive": "Key:D" },
                                                 { "negative": "Key:Left", "positive": "Key:Right" } ] },
      "LookX": { "type": "axis",   "bindings": [ { "source": "MouseDeltaX", "scale": 0.1 } ] },
      "Zoom":  { "type": "axis",   "bindings": [ { "source": "Wheel", "scale": 1.0 } ] }
    }
  }
  ```

- **정규화 규칙**:
  - 입력 이름은 `Key:<KeyCode 이름>`(`platform/IPlatform.h`의 enum 이름 그대로 — 뷰포트 `_map_key`와 같은 이름 집합), `Mouse:Left|Right|Middle`. 아날로그 소스는 `MouseDeltaX`, `MouseDeltaY`, `Wheel`(`InputState`의 프레임 이동량).
  - **button**: `bindings`는 키·마우스 버튼 목록. `IsDown` = 하나라도 눌려 있음. 엣지는 액션 단위로 계산한다. `WasPressed` = 직전 프레임 끝에 액션이 눌려 있지 않았고 이번 프레임에 묶인 입력 중 하나라도 Down 이벤트가 있었음. `WasReleased` = 직전 프레임 끝에 눌려 있었고 지금은 아님. 한 프레임 안에 눌렀다 떼면 `WasPressed`와 `WasReleased`가 둘 다 true, `IsDown`은 false다. 이미 Space로 눌린 상태에서 다른 Jump 키를 더 눌러도 `WasPressed`는 다시 나지 않는다.
  - **axis**: 디지털 쌍(`negative`/`positive`)은 각각 −1/0/+1을 내고, 쌍들의 합을 [−1, 1]로 자른다. 아날로그 소스는 `값 × scale`이며 자르지 않는다(프레임 이동량이라 상한이 없다). `GetAxis` = 디지털 합(잘린 값) + 아날로그 합.
  - **타입이 다른 호출**(axis에 `WasPressed`, button에 `GetAxis`)과 **없는 액션 이름**은 0/false를 돌려주고 액션 이름당 한 번 경고한다. 조용히 다른 의미로 바꿔 주지 않는다(R12).
  - 모르는 입력 이름은 그 바인딩만 건너뛰고 이름을 적어 경고한다. `format`이 다르거나 `version`이 더 높으면 파일 전체를 거절하고 이유를 남긴다. 파일이 없으면 빈 맵으로 시작하고 한 번 경고한다.
- **API**: `LoadActions(path)` / `SaveActions(path)` — 파싱과 직렬화를 **둘 다 이 모듈에만** 둔다(G2 패널도 이것을 바인딩으로 부른다). `GetAxis`, `IsDown`, `WasPressed`, `WasReleased`, `Rebind(name, bindings)`(메모리만 바꾸고 파일은 `SaveActions`가 쓴다).
- **프레임 순서** (`Engine::TickFrame`, 현재 순서는 코드 확인 `core/Engine.cpp:411-457`):
  1. `InputState::BeginFrame()` — 이동량·휠·드래그를 0으로, **이번 프레임 엣지 기록도 비움**(신규).
  2. 플랫폼 이벤트(Player) → `InputState::Apply`.
  3. 큐 이벤트(에디터 Qt) → `InputState::Apply`. 포커스 상실 이벤트는 여기서 E-2의 단일 처리 함수로 간다.
  4. **`InputActions::Update(inputState)`** (신규) — 액션 상태를 프레임당 **한 번** 계산해 스냅샷으로 둔다. 직전 프레임 값은 엣지 계산용으로 보관.
  5. `WorldManager::Update` — C++ 시스템과 `ScriptSystem`(Python)이 **같은 스냅샷**을 읽는다. 읽는 순서와 상관없이 같은 프레임 안에서는 모두 같은 `WasPressed`를 본다. 읽기가 엣지를 소비하지 않는다.
  6. 렌더링.
  - `InputState`에는 아직 엣지 기록이 없다(코드 확인: 눌림 상태·이동량·드래그만 있음). `Apply`가 프레임 안의 Down/Up 발생을 기록하도록 추가한다.
  - World가 Pause면 시스템과 스크립트가 돌지 않으므로, Pause 중 프레임의 엣지는 아무도 보지 않고 사라진다(눌린 상태는 유지). 이것은 의도된 동작으로 문서화한다.
- 엔진이 정하지 않는 것: 기본 액션 이름 목록, 키 배치, 재설정 화면(R5). 샘플 프로젝트가 예시 `actions.json`을 갖는다.
- 기존 `CameraOrbitControlComponent`(C++)는 지금처럼 `InputState`를 직접 읽게 둔다. 액션 맵으로 옮길지는 G0b 뒤에 따로 정한다.
- 검증: 단위 테스트(스키마 파싱, 모르는 키, 타입 불일치, 축 합성·자르기, 같은 프레임 눌렀다 떼기, 두 키 중복 눌림의 엣지, Pause 프레임, Load→Save→Load 왕복, 한글 프로젝트 경로 — R11). 이 단계에서는 Python·Player에 연결하지 않는다.

**G0b — Python 스크립트와 Player 연결**
- Python 바인딩: 스크립트에서 `quarterflying.input.get_axis("MoveX")` 같은 함수로 G0a의 스냅샷을 읽는다. Player도 같은 바인딩을 쓰므로 Qt가 필요 없다(R2). 바인딩은 C++ API를 그대로 노출만 하고 규칙을 다시 구현하지 않는다.
- 로드 위치: 에디터는 프로젝트를 열 때 `<프로젝트>/assets/input/actions.json`, Player는 `game.json`이 가리키는 경로. 둘 다 `LoadActions` 하나를 부른다.
- Build Game: `assets/input/actions.json`을 빌드 출력에 복사하고 `game.json`에 경로를 적는다. `GAME_BUILD_PLAN.md` §3.2·§3.3 표와 빌드 단계 3에 행을 추가한다(R4).
- 검증: 단위 테스트(바인딩 호출, `test_game_builder.py` 파일 복사) → 실제 실행(에디터 Play에서 스크립트가 액션 값을 로그로 출력, Player 스모크 테스트에서 같은 로그).

**G1 — 샘플 캐릭터 조작 (게임 개발자가 고치는 샘플)**
- 샘플 프로젝트에 `scripts/character_controller.py`(이동·스프린트·점프를 카메라 앞 방향 기준으로)와 이를 붙인 `prefabs/Character.prefab.json`을 둔다. 수치와 규칙은 스크립트 안에 있고 게임 개발자가 고친다.
- 궤도 카메라의 target을 캐릭터로 지정한 세트를 `scenes/Demo.scene.json`(X-1b)에 넣는다. X-1b 전환이 끝난 뒤에 넣는다.
- 엔진에 `PlayerMoveSystem`/`CharacterMoveComponent`를 만들지 않는다(R5). 충돌·물리가 없으므로 점프는 단순 수직 속도와 바닥 높이 0 기준이다(샘플의 한계로 주석에 명시).
- **구현 때 추가된 엔진 기본 단위**: 스크립트는 생성자에서 `Entity`만 받고 레지스트리에 닿을 방법이 없었다(바인딩 어디에도 없음). 그래서 `quarterflying.script.registry()`(스크립트 훅 실행 중에만 유효, 밖에서는 예외)를 추가했다 — 이동 규칙이 아니라 "자기 Transform을 읽고 쓴다"는 기본 단위라 R5와 맞는다. `quarterflying.input`과 같은 "프레임 안에서만 유효" 규칙이다.
- **프레임률 독립**: 모든 이동은 `on_update(dt)`의 `dt`로 적분한다(`ScriptSystem`이 이미 `deltaTime`을 넘긴다 — `ecs/ScriptSystem.cpp:158`, 코드 확인). 점프는 `v -= g*dt; y += v*dt`, `y < 0`이면 `y = 0, v = 0`. 창을 끌다 돌아오는 등 `dt`가 튀는 경우를 막도록 스크립트 안에서 `dt`를 0.1초로 자른다(`runtime_bootstrap.py`의 `MAX_STEP_SECONDS`와 같은 값). 점프 높이가 프레임률에 따라 달라지면 결함이다.
- **구조**: 계산은 엔진 없이 부를 수 있는 순수 함수 `step(state, inputs, dt) -> state`로 두고, `on_update`는 입력을 읽어 `step`을 부르고 Transform에 쓰기만 한다. 그래야 "입력 값이 틀렸나(G0), 계산이 틀렸나(샘플), 적용이 틀렸나(Transform)"를 경계에서 가를 수 있다(R13).
- 검증: 단위 테스트(`step`만: `dt = 1/30`과 `1/144`로 같은 점프의 최고 높이·체공 시간이 허용 오차 안에서 같음, `dt` 자르기) → 실제 실행 — 에디터에서 Play 후 10초 이상 조작(화면 캡처), 스크립트를 고쳐 속도가 바뀌는지 확인.

**G2 — 에디터의 조작 편집 패널**
- "Input Actions" 패널: 프로젝트 `assets/input/actions.json`을 표로 편집·저장한다. 에디터 GUI(PySide6)지만 다루는 것은 프로젝트 데이터다.
- 패널은 JSON을 직접 읽거나 쓰지 않는다. G0a의 `LoadActions`/`Rebind`/`SaveActions`를 바인딩으로 부르고, 화면에는 그 결과만 그린다. 형식 지식은 C++ 한 곳에만 있다(R13).
- 게임 안 조작 안내·키 재설정 화면은 이 계획에 넣지 않는다(인게임 UI, 게임 개발자 몫 — R1, R5).
- 검증: 단위 테스트(읽기/쓰기 왕복) → 실제 실행(패널에서 키를 바꾸고 Play에서 반영 확인).

**G3 — 빌드한 게임에서 같은 조작**
- Player가 `game.json`의 액션 맵을 읽고 같은 스크립트로 움직인다.
- 커서 잠금은 `game.json` 옵션(기본 꺼짐) + 스크립트 API로 바꿀 수 있게 한다. 엔진이 정책을 고정하지 않는다(R5).
- 커서 잠금과 포커스: 잠금은 "요청 상태"와 "실제 적용 상태"를 나눠 관리한다. 포커스를 잃으면 E-2의 처리 함수가 실제 잠금을 푼다(요청 상태는 유지). 창 안을 다시 클릭하면, 요청이 아직 켜져 있을 때만 다시 잠근다. 에디터 임베드 모드에서는 잠금 요청을 받기만 하고 적용하지 않는다(Qt가 창을 소유).
- 검증: `player_template` 재빌드 후 Build Game으로 샘플 프로젝트를 빌드해 실행(화면 캡처). 깨끗한 PC 확인은 `GAME_BUILD_PLAN.md` 최종 통과 기준에 따른다.

### 2.2 에디터 GUI (E)

| # | 항목 | 내용 | 검증 |
|---|---|---|---|
| E-1 | Play 중 단축키 정리 | 에디터 단축키는 수식키 조합만 쓴다: Play `Ctrl+P`, Pause/Resume `Ctrl+Alt+P`(신규 — 에디터에 `Ctrl+Alt` 조합은 아직 없다, 코드 확인), Stop `Ctrl+Shift+P`. `Space`·`Esc`·`F`·`Delete` 같은 맨 키는 Play 중 뷰포트에 포커스가 있으면 게임에 간다. Pause 버튼은 Play 중에만 활성. | 실제 실행: Play 중 `Space`가 게임(G1 점프)에 가고 `Ctrl+Alt+P`가 Pause인지 |
| E-2 | 뷰포트 포커스 | Play 진입 시 뷰포트에 `setFocus()`. 포커스를 잃으면 입력 큐에 "포커스 잃음" 이벤트를 넣는다(R8). 키가 눌린 채 다른 창으로 가도 캐릭터가 계속 걷지 않게 한다. Player 창도 같은 문제가 있다 — `Win32Platform`이 `WM_KILLFOCUS`를 처리하지 않는다(코드 확인). 같은 이벤트를 Player 쪽에서도 넣는다. **처리 진입점은 하나**: `Engine::HandleFocusLost()`가 이 이벤트를 소비하고, 상태 정리 루틴을 각각 부른다. 이벤트 자체가 정리를 하지 않는다(R13). ① `InputState::ReleaseAll()` — 키 전부, 마우스 버튼 3개, 마지막 마우스 위치를 해제한다(코드 확인 `input/InputState.cpp:97-102`). 이동량·휠은 다음 `BeginFrame`이 비운다. ② `InputActions`의 직전 상태도 "안 눌림"으로 맞춘다. 그래서 **포커스 상실은 `WasReleased`를 만들지 않는다**. 스크립트가 "떼는 순간"에 동작(예: 차지 공격 발사)을 걸어도 창 전환으로 발동하지 않는다. ③ 커서 실제 잠금 해제(G3). | 단위 테스트(이벤트 → 키·버튼 해제, `WasReleased` 없음) → 실제 실행(에디터·Player 둘 다) |
| E-3 | 상태 표시 | 상태바 고정 영역: 모드(Edit/Play/Pause)·FPS·선택 엔티티. 에디터 Play 뷰포트 위에 Qt 오버레이로 모드 배지와 "클릭하면 입력 받음" 안내. 이 오버레이는 에디터에만 있고 빌드 출력에는 없다(R2). | 실제 실행(캡처) |
| E-4 | Delete / Ctrl+D | Edit 모드에서 Scene Hierarchy나 뷰포트에 포커스가 있을 때만. 기존 `DestroyEntityCommand`/`DuplicateEntityCommand`를 거쳐 Undo 가능(R7). | 실제 실행(삭제·복제 후 Ctrl+Z). 개선안 P1-5의 남은 "실제 Qt 에디터 확인"도 여기서 함께 한다 |
| E-5 | 저장 피드백·도움말 | 창 제목 dirty 표시(`*씬이름`), 저장 후 상태바 "Saved". About 대화상자(버전·빌드). Help → Keyboard Shortcuts는 **에디터 단축키 표만** 보여 준다. 게임 조작은 G2 패널의 몫이다. | 실제 실행 |
| E-6 | 워크플로 | 다중 선택, Hierarchy 검색 필터, 컴포넌트 추가 메뉴(`GetComponentSchema` 목록 기반 — 등록된 컴포넌트를 하드코딩하지 않는다), 도킹 배치 저장·복원(`config.json` — 에디터 설정이므로). Prefab Apply는 개선안 P2-7 전까지 비활성 + 툴팁 유지. | 실제 실행 |
| E-7 | UX 품질 | 위험한 동작 확인 대화상자 일관화, 긴 작업 진행 표시, 에디터 GUI 문자열 언어 통일(어느 쪽으로 할지는 사용자 결정 필요). | 실제 실행 |

### 2.3 모션 에디터 (A)

| # | 항목 | 내용 | 검증 |
|---|---|---|---|
| A-1 | FBX 스텁 정직화 | `do_import_fbx()`(`motion_editor.py:145`)는 "다음 단계에서 연동" 메시지만 띄운다(코드 확인). 메뉴를 비활성으로 두고 툴팁에 "미지원 — JSON 스켈레톤/클립을 쓰세요"를 적는다. 공식 경로는 현재 JSON 형식. 외부 형식은 `MASTER_PLAN.md` §4 Alpha 게이트의 glTF 임포터와 같은 작업으로 묶는다. FBX SDK는 ②에 재배포할 수 있는지(라이선스) 먼저 확인한다 **(확인 필요)**. | 실제 실행 |
| A-2 | Motion Graph 더미 곡선 | `sin` 곡선이 실제 데이터처럼 보이는 것은 관례 3 위반(조용히 틀린 결과). 섹션 `speed`를 그대로 그리거나, 데이터가 없으면 "데이터 없음"을 표시한다. | 실제 실행(캡처) |
| A-3 | 샘플 자동 로드 | `load_default_sample()`(`motion_mixer.py:455`)은 프로젝트의 `motion/` 폴더에서 읽는다(`_motion_dir()` → `current_project().motion_dir`, 코드 확인). 사용자 프로젝트에 샘플 파일이 없으면 무엇이 일어나는지 **(확인 필요)**. 진입 시 자동 로드를 없애고 "Load Sample" 버튼으로 바꾼다. 빈 액션으로 시작할 수 있어야 한다. | 단위 테스트(샘플 없는 프로젝트) → 실제 실행 |
| A-4 | 콘텐츠 작업 | 액션 New / Save / Save As / 최근 파일, 타임라인 이벤트 마우스 추가·이동·삭제, 클립·스켈레톤 경로는 프로젝트 상대 경로로 저장(R4, 한글 경로 포함 — R11). | 단위 테스트 → 실제 실행 |
| A-5 | 프리뷰 | 재생 헤드와 타임라인 1:1, 루프·배속(0.5x/1x/2x). `"Attack"` role 특수 처리(`animation_preview.py`)를 role → 스타일 표로 바꾸고, 표에 없는 role은 기본 스타일로 그린다(R5 — role 이름은 게임 개발자가 정한다). | 실제 실행 |
| A-6 | `MaskPreset::Custom` | 이미 드롭다운에서 빠져 있다(`motion_mixer.py:44-45`, 코드 확인). 할 일 없음. | — |

### 2.4 렌더링·머티리얼·라이트 (R)

| # | 항목 | 내용 | 검증 |
|---|---|---|---|
| R-1 | 방향광 1개 | ECS 컴포넌트 `DirectionalLightComponent { direction, color, intensity, ambient }`를 `ComponentRegistry`에 등록한다(→ `*.scene.json`에 저장, Player도 읽음 — R3). `RenderSystem`이 고른 라이트 하나를 `SceneLighting` 구조체로 `SceneMeshRenderer`에 넘긴다(R6). **선택 규칙**: "첫 번째"는 ECS 순회 순서에 달려 있어 저장·불러오기·복제 뒤에 바뀔 수 있으므로 쓰지 않는다. 활성 라이트 중 `priority`(정수, 기본 0) 가 가장 높은 것, 같으면 **UUID가 가장 작은 것**을 쓴다. UUID는 씬 파일에 저장되므로 에디터와 Player가 같은 라이트를 고른다. 엔티티 id는 불러올 때 다시 매겨지므로 기준으로 쓰지 않는다. 활성 라이트가 둘 이상이면 고른 라이트와 무시한 개수를 경고로 남긴다(선택이 바뀔 때 한 번). 참고: 카메라 선택(`CameraSystem.cpp:33-73`)은 동순위일 때 가장 작은 엔티티 id를 쓴다. 에디터와 Player에서 같은 결과가 나오는지 **(확인 필요)** — 다르면 같은 UUID 규칙으로 맞추는 후속 과제로 올린다. 라이트가 없으면 지금의 고정값(`lightDir (0.4,1,0.35)`, ambient 0.25)을 쓰고 그 사실을 한 번 로그로 남긴다. 샘플 프로젝트 `scene.json`의 lights는 불러올 때 이 컴포넌트로 옮긴다. | 단위 테스트(직렬화 왕복, 라이트 없음 폴백) → 실제 실행(방향을 바꾸면 음영이 바뀜, 캡처) → 빌드한 게임에서 같은 음영 |
| R-2 | 인스턴스별 색 | `RenderableComponent`에 `baseColor`, `roughness`, `metallic`을 추가하고 `RenderSystem.cpp:248-250`의 고정값 대신 넣는다. **배치 키는 바꾸지 않는다**(인스턴스 데이터로 충분). 셰이더는 지금 `roughness`를 읽지 않는다(`SceneMeshRenderer.cpp:21`) — 그 사실을 Inspector 툴팁에 적고, 쓰는 것은 R-4로 미룬다. **기본색 (결정 2026-10-15)**: `baseColor`는 "표면의 최종 고정색"이 아니라 텍스처에 곱하는 **tint**다. 새로 만드는 컴포넌트의 기본값은 흰색 `(1,1,1)`. 텍스처가 있으면 `baseColor × 텍스처`라서 흰색이면 지금 외형 그대로다(셰이더가 `InstanceColor × pow(texture, 2.2)`로 곱하고, 지금 텍스처 있는 인스턴스의 색은 1.0이다 — `SceneMeshRenderer.cpp:77-80`, `RenderSystem.cpp:248`, 코드 확인). 텍스처가 없는 기존 오브젝트의 회색 `(0.75,0.75,0.78)`은 **파일에서 불러오는 경로에서만** 보존한다: 저장된 `RenderableComponent`에 `baseColor` 키가 없고 `texturePath`가 비어 있으면 회색, 키가 없고 텍스처가 있으면 흰색. `roughness`·`metallic` 기본값은 지금 고정값(0.6, 0.0)과 같게 둔다. 이 보정은 X-1a의 **파일 로드 업그레이드 훅**으로만 한다. 일반 역직렬화(`deserialize`)에 넣으면 안 된다 — `AddComponentCommand`는 빈 JSON `{}`로 역직렬화하고(`AddComponentCommand.cpp:42`), `SetComponentJson`은 일부 필드만 담은 JSON을 받을 수 있어서(`ECSBindings.cpp:249`), 거기서 "키 없음"은 레거시 파일이 아니라 "기본값"이나 "그대로 둠"을 뜻한다(코드 확인). 키가 없는 것 자체가 옛 형식의 표시이고 새 파일은 항상 이 필드를 쓰므로, 씬 `version`은 이 일로 따로 올리지 않는다. 샘플 `scene.json`(`scene_instantiation.py`)은 `material.color`가 있으면 그 값을, 없으면 회색을 `baseColor`로 **명시해서** 넘긴다. 키를 빼면 흰색이 되어 샘플 그리드가 하얗게 바뀐다. | 단위 테스트(새 컴포넌트 = 흰색, 키 없는 옛 씬·프리팹 = 텍스처 유무에 따른 회색/흰색, `AddComponent`·부분 `SetComponentJson`은 회색 보정을 받지 않음) → 실제 실행: 옛 씬을 열어 화면이 지금과 같음 + 서로 다른 색 인스턴스를 한 화면에 캡처. 이것으로 `CLAUDE.md` "알려진 미검증 항목"의 `InstanceData.color` 인스턴스별 전달도 검증된다 |
| R-3 | 메시 폴백 표시 | `meshPath` 로드 실패 시 절차적 큐브로 대체하는 것은 유지하되, 실패한 엔티티를 Inspector에 경고로 보여 준다(관례 3). | 실제 실행 |
| R-4 | 머티리얼 에셋 — **경로 필드로 통일 (결정 2026-10-15)** | 에셋 참조는 경로 필드다. `RenderableComponent`(스키마 `_version` 1 → 2): `meshPath`, **`materialPath`(신규)**, `texturePath`(인스턴스별 텍스처 덮어쓰기), `baseColor`(인스턴스별 tint), `castShadows`. 저장·Inspector에서 빠지는 것: `meshHandle`(코드의 `RegisterMesh`용 런타임 값으로만 남는다 — 파일에 저장하지 않음), `materialHandle`(필드 삭제), `roughness`·`metallic`(머티리얼로 옮김). 머티리얼 파일 `*.material.json` = `{format: "quarterflying.material", version: 1, baseColor, texture, roughness, metallic}`, 파서는 C++ 한 곳(`asset/MaterialAsset`). **합성 규칙(함수 하나)**: 색 = 머티리얼 baseColor × 컴포넌트 baseColor, 텍스처 = 컴포넌트 `texturePath`가 있으면 그것, 없으면 머티리얼 texture, roughness·metallic = 머티리얼(없으면 0.6/0.0). 셰이더가 roughness·metallic을 쓴다 — 기본값(0.6/0.0)에서 지금 화면과 같도록 매핑한다(반사 세기 0.75×(1−r), 하이라이트 지수 32×2^((0.6−r)×8), metallic은 반사색을 표면색 쪽으로). **계획에서 뺀 것**: 셰이더 파일화는 D5 `builtin/`(②·Player 템플릿에 엔진 리소스를 넣는 규칙) 결정이 먼저라 하지 않는다 — 셰이더는 지금처럼 C++ 안에 둔다. | 단위 테스트(머티리얼 파서, 합성 규칙, 옛 `_version` 1 파일 처리) → 실제 실행: 같은 메시에 다른 머티리얼(색·roughness·metallic)이 한 화면에 다르게 보임(캡처, `InstanceData.roughness/metallic` GPU 레이아웃 검증 포함) → 빌드한 게임에서 같게 |

### 2.5 샘플 콘텐츠 (X)

| # | 항목 | 내용 | 검증 |
|---|---|---|---|
| X-1a | 프리팹 경로를 프로젝트 상대 경로로 (마이그레이션) | `PrefabInstanceComponent.prefabPath`의 의미를 "절대 경로"에서 "프로젝트 상대 경로 우선"으로 바꾼다. 기준 폴더는 프로젝트 루트 — `meshPath`·`texturePath`와 같은 기준이다(에디터가 `Engine.SetAssetRoot(project.root)`를 부른다, `project_actions.py:136`, 코드 확인). 해석·정규화는 C++ 함수 하나에 두고 프리팹 명령·바인딩·Inspector가 모두 그것을 부른다(R13). **씬 version (결정 2026-10-15)**: 필드 추가가 아니라 기존 필드의 의미 변경이므로 `kSceneFormatVersion`을 1 → **2**로 올린다(`scene/SceneIO.h:29`, 코드 확인). 버전을 그대로 두면 파일만 보고 어느 의미 체계인지 구분할 수 없다. 로더 규칙: version ≤ 1이면 옛 절대 경로 의미를 허용한다. version 2에서는 상대 경로가 정규 형식이다. 단 version 2에서도 절대 경로를 읽는 것은 허용해서, 손으로 고친 파일이나 옛 도구 출력이 바로 실패하지 않게 한다. 불러올 때 프로젝트 안을 가리키는 절대 경로는 상대 경로(`/` 구분자)로 정규화하고, 다음 저장 때 그 형식으로 쓴다. 프로젝트 밖을 가리키면 절대 경로를 유지하고 경고를 남긴다(다른 PC에서 Revert가 깨질 수 있음). version 3 이상은 지금처럼 거절한다(`SceneIO.cpp:75`). **파일 로드 업그레이드 훅**: 이 정규화와 R-2의 레거시 기본색 보정은 컴포넌트별 선택 훅(`ComponentInfo`에 `upgradeFromFile(json&, const FileLoadContext&)` — 파일 version, 프로젝트 루트)으로 둔다. 훅은 **파일 로더 두 곳에서만** 부른다: `SceneIO` 로드와 프리팹 파일 로드. PIE 스냅샷(Play/Stop), Undo 스냅샷, `AddComponent`, `SetComponentJson`은 부르지 않는다. 컴포넌트별 마이그레이션 지식이 로더마다 흩어지지 않고 해당 컴포넌트 등록 옆에 모인다(R13). | 단위 테스트(`SceneIOTests`: v1 절대 경로 → 상대 경로, v2 왕복, v2 안의 절대 경로 허용·정규화, 프로젝트 밖 경로 유지 + 경고, v3 거절, 한글 프로젝트 경로 — R11. 프리팹 명령 테스트) → 실제 실행(옛 씬을 열어 Revert가 동작, 저장 후 파일이 v2·상대 경로) |
| X-1b | 데모 월드를 씬 파일로 | 샘플 프로젝트의 코드 시드(`demo_scene_seed.py`) + `scene.json` 조합을 `scenes/Demo.scene.json` 하나로 저장하고, 샘플 프로젝트의 시작 씬으로 지정한다. 그러면 VFX·카메라 수치는 코드가 아니라 씬 데이터가 되고, 같은 씬을 Player로 빌드할 수 있다. **선행 조건**: X-1a. `demo_scene_seed.py`는 지우지 않고 `test_demo_scene_seed.py`의 검증용으로 남긴다(R10). G1의 캐릭터는 이 전환이 끝난 **뒤에** 씬에 넣는다 — 전환 자체가 화면을 바꾸지 않았음을 먼저 확인해야, 이후 차이가 G1 탓인지 전환 탓인지 가를 수 있다. | 실제 실행(샘플 프로젝트 시작 화면이 전환 전 캡처와 같음) → Build Game으로 실행 |
| X-2 | 임시 사운드 표시 | `sfx/Blip.wav`가 유일한 효과음이다(코드 확인). 샘플 효과음 몇 개를 추가하고, Sound Test 엔티티 이름에 "(sample)"을 붙인다. 레이어·태그 기능은 아직 없다(`MASTER_PLAN.md` §3 #1 요구사항) — 그 기능이 생기면 태그로 바꾼다. 게임 빌드 오디오는 D9에 따라 범위 밖. | 실제 실행(Play에서 소리) |

### 2.6 항목별 영향 범위와 원인 추적 지점 (R13)

`CLAUDE.md` 관례 4의 다섯 칸을 항목마다 적었다. 착수 전에 이 표로 영향 범위를 확인하고,
구현이 끝나면 실제와 다른 곳을 고친다. 로그 태그는 C++ `"<모듈>: ..."`, 에디터 `[<모듈>]` 형식을
쓴다(에디터 로그를 `logging`으로 옮기는 일은 `ARCHITECTURE_COUPLING_AUDIT.md` §3.4에서 따로 한다).

전제 — 리플렉션에 등록된 컴포넌트·필드는 다음 곳에 **자동으로 퍼진다**: Inspector(`GetComponentSchema`),
`EntitySnapshot`(Undo·Delete·Duplicate), 프리팹 캡처·적용, `SceneIO`(`*.scene.json`), Python 바인딩의
JSON 접근. 그래서 컴포넌트를 바꾸는 항목(R-1, R-2)은 이 경로 전체가 영향 범위다. 필드 **추가**는
옛 파일에서 기본값으로 읽혀 호환된다(`ecs/Reflection.h:191`의 `data.contains` 분기, 코드 확인). 단 "키 없음"이 레거시 파일을 뜻하는지는 호출 경로마다 다르다 — 그래서 레거시 보정은 일반 역직렬화가 아니라 파일 로더에서만 부르는 업그레이드 훅(X-1a)으로 한다.

| 항목 | 바뀌는 곳 | 영향받는 곳 | 호환성 | 고장 시 갈리는 지점 | 회귀 확인 |
|---|---|---|---|---|---|
| **G0a** | 신규 `input/InputActions.{h,cpp}`(`InputState`만 읽음, `LoadActions`/`SaveActions` 포함), `input/InputState`(프레임 엣지 기록 추가), `Engine::TickFrame`(`InputActions::Update` 호출 위치) | `InputState`를 읽는 기존 시스템(`CameraRigSystem`) — 엣지 기록 추가는 기존 API를 바꾸지 않는다. `BeginFrame`이 비우는 값이 늘어난다 | 새 파일 형식(`format`·`version` 키). 없으면 빈 맵 | 키 → `InputState`(눌림·엣지) → `InputActions` 스냅샷 → 읽는 쪽. 스냅샷 값이 맞는데 동작이 틀리면 읽는 쪽, 스냅샷이 틀리면 `InputActions`, `InputState`가 틀리면 이벤트 전달. 로드 실패는 `InputActions: <경로>: <이유>` 한 번 | 신규 `InputActionsTests`(G0a 검증 목록 전부), 기존 `CameraRigTests`, 기존 입력 테스트 |
| **G0b** | `bindings/`(입력 함수), `editor/project/project.py`(`input/` 폴더), `gamebuild/game_builder.py`(복사), `runtime/GameConfig`·`PlayerMain`(로드) | Player 템플릿 재빌드. `GAME_BUILD_PLAN.md` §3.2·§3.3 표 | `game.json`에 키 추가. `GameConfig`는 없는 키를 기본값으로 읽는다(확인 완료: `GameConfigTest.MissingOptionalKeys_UseDefaults_UnknownKeysIgnored`) — 옛 game.json은 액션 맵 없이 시작 | 에디터에서는 되고 Player에서 안 되면 `game.json` 경로 → `LoadActions` 로그 순. 바인딩은 C++를 노출만 하므로 Python 쪽에서 값이 바뀔 곳이 없다 | `GameConfigTests` 확장, `test_game_builder.py`(파일 복사), `player_template_smoke` |
| **G1** | 샘플 프로젝트 파일(`assets/scripts/character_controller.py`, `assets/prefabs/Character.prefab.json`, `assets/scenes/Demo.scene.json`) + **엔진 기본 단위 하나**: `ecs/ScriptContext`와 바인딩 `quarterflying.script.registry()`(구현 때 추가 — 스크립트가 자기 Transform에 닿을 방법이 없었다) | `ScriptSystem::Update`/`OnStop`(문맥 설정), ECS 바인딩. Player도 같은 `ScriptSystem`이라 G3에서 그대로 쓴다 | 해당 없음(새 API) | `ScriptSystem: <모듈>::<클래스>.on_update raised` 로그가 있으면 스크립트. 없으면 `step()`에 들어간 입력 값(G0)과 나온 상태(샘플 계산), 그리고 Transform에 쓴 값을 비교한다. 스크립트 밖에서 `registry()`를 부르면 원인을 적은 예외 | `step()` 단위 테스트(프레임률 독립), `ScriptContextTests`, 실제 실행(캡처) |
| **G2** | 신규 패널(`panels/input_actions_panel.py`) + `main.py` 도크 등록 | 파일 형식 지식이 C++ 로더와 Python 패널 두 곳에 생기면 형식이 어긋난다 → **패널도 G0a의 `LoadActions`·`SaveActions`를 바인딩으로 쓴다**(파서는 한 곳). `HAS_ENGINE=False`에서는 패널을 읽기 전용 안내로 둔다 | `actions.json` 형식은 G0a와 같음 | 패널에서 저장한 파일을 G0a 로더가 거절하면 로더 메시지가 원인이다(같은 코드라 형식 차이는 생길 수 없다) | G0a 테스트 + 패널 왕복 단위 테스트 |
| **G3** | `runtime/PlayerMain.cpp`, `GameConfig`(커서 잠금 옵션), `Win32Platform`(커서 잠금 요청/적용 분리) | `Win32Platform`은 에디터 임베드 모드와 공유한다 → 임베드 모드에서는 잠금 요청을 받기만 하고 적용하지 않는다. E-2의 `HandleFocusLost()`가 실제 잠금을 푼다. Player 템플릿 재빌드 | `game.json` 키 추가(G0b와 같은 확인) | 조작이 에디터에서는 되고 게임에서 안 되면: `game.json`의 액션 맵 경로 → 로드 로그 → 입력 이벤트 순. 커서가 안 풀리면 요청 상태와 적용 상태를 따로 로그로 본다 | `player_template_smoke`, `GameConfigTests`, 빌드한 게임 실제 실행(포커스 전환 포함) |
| **E-1** | `main.py` 액션 단축키·툴바 활성 상태 | `viewport.py` 키 전달(Qt 단축키가 `keyPressEvent`보다 먼저 키를 가져간다) | 해당 없음 | 키가 게임에 안 가면: Qt 단축키가 가져갔는지(단축키 컨텍스트) vs `_map_key`가 `None`인지(`viewport.py:226`) | 실제 실행 |
| **E-2** | `InputEventType`에 "포커스 잃음" 추가, 신규 `Engine::HandleFocusLost()`(단일 진입점), `viewport.py` `focusOutEvent`, `Win32Platform` `WM_KILLFOCUS`, `InputActions`(직전 상태 초기화) | `InputEventType`을 다루는 파일 5개: `InputState.cpp`, `Engine.cpp`, `Win32Platform.cpp`, `IPlatform.h`, `EngineBindings.cpp`(코드 확인). `switch`에 새 값이 빠지면 조용히 무시된다. G3의 커서 잠금 | enum 값은 **끝에 추가**해 기존 번호를 유지한다 | 이벤트가 큐에 들어갔나(`Engine` 로그) → `HandleFocusLost()`가 불렸나 → 정리 루틴별 결과(키·버튼 해제, `WasReleased` 없음, 커서 해제)를 각각 확인 | `InputState`·`InputActions` 단위 테스트 추가, 실제 실행(에디터·Player 둘 다) |
| **E-4** | `scene_hierarchy.py`·`viewport.py` 단축키 연결만 | 기존 `DestroyEntityCommand`/`DuplicateEntityCommand` 재사용 → C++ 변경 없음 | 해당 없음 | 메뉴로는 되고 단축키로 안 되면 단축키 연결, 둘 다 안 되면 명령 쪽 | `DuplicateEntityCommandTests`, 삭제 명령 테스트(기존), 실제 실행 |
| **E-5~E-7** | `main.py`, `panels/`(에디터 GUI만) | 엔진·저장 형식 영향 없음. E-6 컴포넌트 추가 메뉴는 `GetComponentSchema`만 읽으므로 R-1의 새 컴포넌트가 자동으로 나타난다. 도킹 배치는 `config.json` 키 추가(`config_manager.py` 기본값) | `config.json` 키 추가 — 없으면 기본 배치 | 화면 문제는 해당 패널 안에서 끝난다 | 실제 실행 |
| **A-1~A-3, A-5** | `motion_editor.py`, `panels/motion_mixer.py`, `animation_preview.py`, Motion Graph 패널 | 에디터 GUI 안에서 끝난다 | 해당 없음 | 패널 단위 | 실제 실행, A-3은 샘플 없는 프로젝트 단위 테스트 |
| **A-4** | 액션 저장 경로 처리(상대 경로) | **액션 JSON은 Player도 읽는다**: `runtime_bootstrap.py` → `core/scene_action_playback.py`. 경로 규칙을 바꾸면 에디터 Play와 빌드한 게임이 함께 영향받는다. Build Game 수집 규칙(액션 파일 경로) | 옛 파일의 절대 경로도 계속 읽는다. 경로 해석은 `core/` 한 곳에서 한다 | 재생이 안 되면 `[ScenePlayback]` 계열 로그의 해석된 경로부터 본다 | `test_scene_action_playback.py`, `test_action_playback.py`, `test_motion_sound_integration.py`, `test_game_builder.py` |
| **R-1** | `ecs/Components.h`(새 컴포넌트), `ecs/Reflection.cpp`(등록), `ecs/RenderSystem`(라이트 수집), `renderer/SceneMeshRenderer`(유니폼). `SceneLighting` 구조체는 **renderer 쪽 헤더**에 둔다 — ecs → renderer 방향만 유지(R6). `editor/scene_instantiation.py`(`scene.json` lights 변환) | 위 "자동으로 퍼지는 곳" 전체. 등록 컴포넌트 수를 세는 테스트(`ComponentTypeRegistryTests` 등) **(확인 필요)**. Player 템플릿 재빌드 | 새 컴포넌트 → 옛 씬은 라이트가 없어 지금과 같은 고정값으로 그려진다 | 어느 라이트가 골라졌나(UUID 로그) → `RenderSystem`이 만든 `SceneLighting` 값: 값이 맞는데 화면이 틀리면 셰이더(renderer), 값이 틀리면 컴포넌트·수집(ecs) | `RenderSystemTests`에 GL 없이 `SceneLighting` 생성 테스트(선택 규칙: priority, UUID 동순위, 여러 개 경고, 저장·불러오기 뒤 같은 선택), `SceneIOTests` 왕복, 실제 실행 캡처(에디터·빌드한 게임) |
| **R-2** | `RenderableComponent` 필드 3개, `Reflection.cpp`(등록 + 파일 로드 업그레이드 훅), `RenderSystem.cpp:248-250`, `scene_instantiation.py`(기본색 명시) | 위 "자동으로 퍼지는 곳" 전체 + `RenderableComponent`를 쓰는 파일 25개(테스트 포함, 코드 확인). 저장된 프리팹 파일. X-1a의 훅 메커니즘 | 필드 추가라 옛 파일은 읽힌다. 기본색은 결정됨(§2.4 R-2): 새 컴포넌트 흰색, 옛 파일의 텍스처 없는 오브젝트는 로드 훅으로 회색 유지 → **기존 씬의 화면은 바뀌지 않아야 한다** | `InstanceData.color`에 들어간 값: 맞으면 셰이더, 틀리면 `RenderSystem`. 옛 씬이 하얗게 보이면 로드 훅이 안 불렸는지(로더 경로) 먼저 본다 | `RenderSystemTests`(인스턴스별 값), `PrefabAssetTests`, `SceneIOTests`(키 없는 옛 파일), `AddRemoveComponentCommandTests`(빈 JSON = 흰색), 실제 실행 캡처 |
| **R-3** | `RenderSystem`(실패 목록 보관), 바인딩(조회 함수), `inspector.py`(경고 표시) | Inspector가 엔진 상태를 하나 더 읽는다 — 조회는 바인딩 한 함수로만 | 해당 없음 | `RenderSystem: mesh load failed '<경로>'` 로그(경로당 한 번) | `RenderSystemTests`(실패 경로 기록), 실제 실행 |
| **R-4** | `ecs/Components.h`·`Reflection.cpp`(Renderable 스키마 2 + 파일 로드 훅), 신규 `asset/MaterialAsset`(파서), `ecs/RenderSystem`(경로당 한 번 로드·캐시, 합성 규칙), `renderer/SceneMeshRenderer`(셰이더), `bindings/ECSBindings.cpp`(`material_handle` 제거), `gamebuild/game_builder.py`(수집 표에 `materialPath`, 머티리얼의 `texture`), `project/project.py`(`assets/materials`) | 위 "자동으로 퍼지는 곳" 전체(Inspector 필드 목록이 바뀐다 — Inspector의 스키마 버전 검사가 새 버전을 그대로 따른다). 옛 씬·프리팹 파일(`_version` 1). Player 템플릿 재빌드(Build Game의 템플릿 해시 검사가 잡는다) | **필드 삭제 = 스키마 변경**이라 컴포넌트 `_version`을 2로 올린다(씬 파일 version은 그대로 — 바뀐 것은 이 컴포넌트뿐). 옛 파일의 `meshHandle`·`materialHandle`·`roughness`·`metallic` 키는 읽을 때 무시하되, `meshHandle`이 0이 아니면 경고 (파일에 저장된 핸들은 더 이상 의미가 없다) | 머티리얼 로드 실패는 `RenderSystem: material '<경로>' ...` 경고 한 번 + 기본 머티리얼. 합성 결과(`InstanceMaterial`)가 맞는데 화면이 틀리면 셰이더, 틀리면 합성 규칙·머티리얼 파일 | 신규 `MaterialAssetTests`, `LightingMaterialTests` 확장, `SceneIOTests`·`PrefabAssetTests`, `test_game_builder.py`(머티리얼 수집), 실제 실행 캡처 |
| **X-1a** | `scene/SceneIO`(version 2, 로더 규칙), `ecs/Reflection`(`ComponentInfo` 업그레이드 훅), `prefab/`(프리팹 파일 로드에서 훅 호출), 경로 해석·정규화 함수(신규, C++ 한 곳), `editor/commands/` 프리팹 명령, `bindings/EditorBindings.cpp`, `prefab_browser.py`·`inspector.py`(표시) | `InstantiatePrefabCommand`, `RevertPrefabInstanceCommand`, Duplicate(프리팹 인스턴스 복제). **Player**: 새 에디터가 v2로 저장한 씬은 옛 Player 템플릿이 거절한다(`version > kSceneFormatVersion`) → Player 템플릿 재빌드 필수. Build Game은 `prefabPath`를 읽지 않는다(코드 확인: `gamebuild/`에 사용처 없음) — 프리팹 파일은 에디터의 Revert만 쓴다. 단 **Build Game의 씬 버전 검사**는 영향받는다(아래 §5.4). 프리팹 파일에는 `prefabPath`가 들어가지 않는다(확인 완료: 캡처가 항상 `PrefabCaptureOptions().excludePrefabMetadata = true`, `PrefabAsset.cpp:211`) | **의미 변경 + version 1 → 2** (결정 2026-10-15). v1·v2의 절대 경로 모두 읽고, 프로젝트 안이면 상대 경로로 정규화 | 프리팹 해석 실패 로그에 저장된 원래 값·파일 version·정규화 결과·최종 절대 경로를 함께 남긴다. 훅이 불렸는지는 로더별 로그로 가른다 | `SceneIOTests`(X-1a 검증 목록), `InstantiatePrefabCommandTests`, `RevertPrefabInstanceCommandTests`, `DuplicateEntityCommandTests`, `PrefabAssetTests`, `player_template_smoke`(v2 씬), 한글 경로(R11) |
| **X-1b** | 샘플 프로젝트 파일(`scenes/Demo.scene.json`, `project.json`의 시작 씬), `main.py` 샘플 분기 | 샘플 프로젝트를 쓰는 테스트와 Build Game 샘플 빌드 | 해당 없음(X-1a의 v2 형식으로 저장) | 화면이 전환 전과 다르면 씬 파일 내용(컴포넌트 값) vs 로드 경로를 비교한다 | `test_demo_scene_seed.py`, `test_game_builder.py`, 전환 전후 캡처 비교 |
| **X-2** | 샘플 에셋만 | 없음 | 해당 없음 | — | 실제 실행 |

---

## 3. 실행 순서

| 순서 | 작업 | 완료 기준 (검증 단계) |
|---|---|---|
| 1 | E-1, E-2, E-5(dirty·About) | Play 중 맨 키가 게임에 가고, 포커스를 잃으면 입력이 풀린다 (실제 실행) |
| 2 | G0a | 액션 모델 단위 테스트 전부 통과 (단위 테스트까지 — Python·Player 연결 전) |
| 3 | G0b | 에디터 Play와 Player 스모크 테스트에서 스크립트가 같은 액션 값을 읽는다 (단위 테스트 + 실제 실행) |
| 4 | X-1a | v1 씬이 열리고 v2·상대 경로로 저장되며 Revert가 동작한다. Player 템플릿 재빌드 후 v2 씬 실행 (단위 테스트 + 실제 실행) |
| 5 | X-1b | 샘플 프로젝트 시작 화면이 전환 전 캡처와 같다 (실제 실행, 캡처) |
| 6 | G1 | 샘플 프로젝트에서 Play → 캐릭터 조작 10초 이상, `step()` 프레임률 테스트 통과 (단위 테스트 + 실제 실행, 캡처) |
| 7 | G3 | Build Game으로 만든 게임에서 같은 조작, 포커스 전환 후 입력·커서 정상 (실제 실행, 캡처) |
| 8 | A-1, A-2, A-3 | 메뉴·그래프가 사실과 맞고 빈 액션으로 시작 가능 (실제 실행) |
| 9 | R-1, R-2 | 옛 씬의 화면이 그대로이고, 라이트·색이 에디터와 빌드한 게임에서 같게 보임 (실제 실행, 캡처). R-2는 X-1a의 업그레이드 훅을 쓴다 |
| 10 | G2, E-3, E-4, E-6, A-4, A-5, X-2 | 항목별 검증 기준 |
| 11 | R-4 (결정 2026-10-15) | 머티리얼 파일이 에디터와 빌드한 게임에서 같게 보이고, 옛 파일이 그대로 열린다 (단위 테스트 + 실제 실행, 캡처) |
| 보류 | E-7 언어 통일 | 사용자 결정 후 |

각 단계는 시작 전에 §2.6의 "영향받는 곳"을 코드에서 다시 확인하고(grep·호출부), 끝난 뒤 "회귀 확인" 칸의 테스트를 모두 돌린다.

**진행 상황(2026-10-15)**: 실행 순서 1~11을 모두 구현했다(11 = R-4, §5.11). 단계별로 어디까지 확인했는지(단위 테스트 / 실제 실행 / 화면 캡처)와 계획에서 달라진 점은 §5 구현 기록에 있다. 남은 것은 "보류" 행(E-7)과 깨끗한 PC 확인이다.

## 4. 하지 않는 것

- 엔진에 게임 규칙을 고정하는 컴포넌트(캐릭터 이동·점프 수치 등) — 샘플 스크립트로 둔다(R5).
- 게임 화면 안의 조작 안내·키 재설정 화면 — 인게임 UI이고 게임 개발자 몫이다(R1, R5). 인게임 UI 자체는 `INGAME_UI_CUSTOMIZATION_PLAN.md`가 다룬다.
- 에디터 Play 모드에서만 동작하는 게임 기능(Qt로 그린 게임 HUD, `scene.json`에만 있는 조명 등) — 빌드한 게임과 달라진다(R2, R3).
- FBX 풀 임포트를 GUI 개선과 동시에 진행, 머티리얼 에셋·D3D12와 조작 개선을 한 묶음으로 진행 — 원안의 판단을 유지한다.

---

## 5. 구현 기록

검증 수준 표기는 `CLAUDE.md` 관례 2를 따른다: **컴파일**(빌드만 통과) / **단위 테스트** / **실제 실행**(에디터를 `HAS_ENGINE=True`로 띄워 결과를 봄, 화면 캡처 여부를 따로 적음).

### 5.1 실행 순서 1 — E-1, E-2, E-5(dirty·About) (2026-10-15)

| 항목 | 한 일 | 검증 수준 |
|---|---|---|
| E-1 | Pause를 메뉴(`Ctrl+Alt+P`)·툴바에 연결했다(예전에는 둘 다 연결된 슬롯이 없었다). Pause는 Play 중에만 켜진다. Play 중 다시 Play를 누르면 일시정지/계속. 상태 표시는 `_refresh_play_controls()` 한 곳에서 한다. Play 시작·계속 시 뷰포트에 포커스를 준다 | 실제 실행(자동화): Play 중 `Space`·`Esc`가 Play 상태를 바꾸지 않음, `Ctrl+Alt+P`로 일시정지·계속, 화면 캡처(툴바 Pause 체크, 상태바 "Paused") |
| E-1 부수 수정 | **기존 버그**: Pause가 `is_playing`을 `False`로 만들어서, 일시정지 중에 Save Scene이 통과해 Play로 바뀐 월드가 씬 파일에 저장됐다. `is_playing`(Play~Stop 전체)과 `is_paused`를 나눴다. 일시정지 중에는 액션 재생 커서도 전진하지 않는다 | 실제 실행: 일시정지 중 Save가 "Play 중에는 할 수 없습니다" 확인 창으로 막힘 |
| E-2 | `InputEventType::FocusLost`(enum 끝에 추가), 두 입력 경로를 `Engine::DispatchInputEvent` 하나로 합쳤다, 처리는 `Engine::HandleFocusLost()` 한 곳(`InputState::ReleaseAll`). 에디터는 `viewport.focusOutEvent`, Player는 `WM_KILLFOCUS`가 이벤트를 넣는다 | 단위 테스트(`InputRoutingTests` 4개). 에디터 실제 실행: 키를 누른 채 포커스를 옮기면 엔진 로그 `Engine: focus lost - released all held keys and mouse buttons`. Player 쪽(`WM_KILLFOCUS`)은 G3에서 실제 실행으로 확인(§5.7: W를 누른 채 창을 최소화하면 캐릭터가 멈춤) |
| E-5 | 창 제목 `*` 표시. 기준은 `CommandManager::GetChangeCount()`(성공한 Execute/Undo/Redo마다 증가, Clear로 되돌아가지 않음) 하나다. 저장·열기·새 씬·시작 월드 생성 직후를 기준으로 삼고, Stop은 Play 전의 표시 상태로 되돌린다. About 대화상자(엔진 버전·Python·프로젝트 경로) | 단위 테스트(변경 카운터 2개). 실제 실행: 명령 편집 → `*`, 한글 경로에 저장 → `*` 사라짐, 깨끗한 상태에서 Play/Stop → `*` 없음, 편집 후 Play/Stop → `*` 유지, About에 `엔진 버전: 0.1.0` |

- 전체 C++ 테스트: 580/580 통과.
- 한계: 명령을 거치지 않는 편집(`editor_api`가 없을 때의 Inspector 폴백 등)은 `*`를 켜지 않는다(`project_actions.py` 주석).
- Player 템플릿은 다시 만들지 않았다. Build Game 결과에 E-2를 반영하려면 `player_template`을 다시 빌드해야 한다(G0b·X-1a에서 함께 한다).

### 5.2 실행 순서 2 — G0a 액션 모델 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| 신규 `input/InputActions.{h,cpp}`: §2.1 G0a의 스키마·정규화 규칙·엣지 규칙, `LoadActions`/`SaveActions`(임시 파일에 쓴 뒤 바꿔치기)/`LoadFromJson`/`ToJson`, 편집 API `SetActionJson`/`Rebind`/`RemoveAction`. 거절된 파일은 기존 맵을 그대로 둔다 | 단위 테스트 `InputActionsTests` 21개(스키마, 모르는 키, 모양 틀린 액션, 형식·버전 거절, 축 합성·자르기, 아날로그 비자르기, 누름·유지·뗌, 같은 프레임 눌렀다 떼기, 같은 액션의 두 키, 마우스 버튼, 프레임 안 읽기 안정성, Pause 프레임, 타입 불일치·없는 이름, 포커스 상실, 엔진 경유 포커스 상실, Rebind, SetActionJson, 한글 경로 저장·왕복, 파일 없음, 깨진 JSON) |
| `InputState`에 프레임 엣지(`WasKeyPressed` 등) 추가. Win32 자동 반복 Down은 엣지가 아니다 | 같은 테스트 + 기존 `InputStateTest`·`CameraRigTest` 통과 |
| `Engine::TickFrame`: 이벤트 적용 → `InputActions::Update` → World 업데이트 순서. `HandleFocusLost`가 `ResetHeldState`도 부른다 | 단위 테스트(`EngineFocusLost_ResetsActionsToo`) |
| **테스트가 잡은 결함**: 같은 프레임에 키를 누르고 바로 포커스를 잃으면, 남아 있던 누름 엣지 때문에 액션이 "한 프레임 안에 눌렀다 뗌"으로 읽혀 `WasReleased`가 나왔다(포커스 상실은 뗌을 만들지 않는다는 규칙 위반). `InputState::ReleaseAll`이 그 프레임의 엣지도 지우도록 고쳤다(코드에 증상·원인 주석) | 단위 테스트 `PressThenFocusLostInSameFrame_ProducesNoEdges` |

- 전체 C++ 테스트: 601/601 → G0b 후 602/602.
- 계획대로 이 단계에서는 Python·Player에 연결하지 않았다. 에디터 회귀 확인(실행 순서 1의 자동화 19개)은 다시 통과했다.

### 5.3 실행 순서 3 — G0b Python·Player 연결 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| 스크립트용 `quarterflying.input`(`get_axis`/`is_down`/`was_pressed`/`was_released`/`has_action`). 어느 엔진의 입력인지는 `InputActions::Current()`로 정한다 — `TickFrame`이 World 업데이트 동안만 설정(thread_local, DLL 쪽 .cpp에 정의). 프레임 밖에서 부르면 원인을 적은 `RuntimeError` | 실제 실행(에디터): 프레임 밖 호출이 `no engine frame is running ...` 예외 |
| `Engine.LoadInputActions`/`SaveInputActions`/`GetInputActionsJson` 바인딩. 에디터는 프로젝트를 열 때와 엔진 연결 때 `_load_input_actions()`로 부른다(거절되면 경고 창, 건너뛴 바인딩은 콘솔) | 실제 실행(에디터 자동화, 한글 경로 임시 프로젝트): Play 중 D/W 누름 → 스크립트가 `MoveX=1.0`, `MoveY=1.0`; Space 누름·유지·뗌 → `jumpPressed`/`jumpReleased`가 각각 한 프레임; Space를 누른 채 포커스 이동 → `jumpDown=False`, `jumpReleased` 없음 |
| 새 프로젝트는 빈 액션 맵(`assets/input/actions.json`)을 만든다. 샘플 프로젝트에 예시 맵(`engine/assets/input/actions.json`: MoveX, MoveY, Jump, Sprint) | 단위 테스트(`test_create_and_open_project_round_trip`) |
| `game.json`의 `inputActions`(선택), Player가 엔진 로더로 읽는다. Build Game이 파일을 복사하고 키를 쓴다(없으면 경고하고 키를 생략). 형식 검사는 하지 않는다 — 파서를 두 벌 만들지 않기 위해 | 단위 테스트(`GameConfigTest.InputActions_OptionalPath`, `test_build_assembles_*`, `test_build_without_input_actions_*`). 실제 실행: `player_template` 재빌드 후 한글 폴더 프로젝트를 빌드해 다른 경로로 옮겨 실행 — 로그 `input actions: 2 loaded`, 스크립트 출력 `input has_action=True axis=0.0 jump=False` |

- `test_game_builder.py` 19/19, `player_template_smoke` 통과.
- Player에서 **실제 키 입력**으로 값이 바뀌는 것은 아직 확인하지 않았다(빌드 스모크 테스트는 키를 누르지 않는다). G3에서 확인한다.
- `GAME_BUILD_PLAN.md` §3.2·§3.3·§3.4에 입력 액션 맵을 추가했다(R4).

### 5.4 실행 순서 4 — X-1a 프리팹 경로 마이그레이션 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| `core/ProjectPath.h`: `ResolveProjectPath`/`NormalizeProjectPath` — 경로 해석·정규화의 유일한 곳. 프로젝트 안의 절대 경로 → `/` 구분 상대 경로, 밖이면 그대로 + 표시, 접두사만 같은 형제 폴더는 밖 | 단위 테스트 `ProjectPathTests` 7개(한글 경로 포함) |
| 파일 로드 업그레이드 훅: `ComponentInfo::upgradeFromFile` + `FileLoadContext`, `ComponentRegistry::SetFileUpgrader`(이름·해시 두 맵 모두), `UpgradeComponentsFromFile`. 부르는 곳은 `SceneIO` 로드와 `PrefabAsset::LoadFromFile` 두 곳뿐 | 단위 테스트 `PieSnapshot_DoesNotRunFileUpgrade`(Play/Stop 스냅샷은 훅을 안 탐) |
| 씬 `kSceneFormatVersion` 1 → 2(결정 2026-10-15). `LoadSceneFromFile(registry, path, projectRoot)`, `UpgradeSceneDocument`. 프로젝트 밖 경로는 경고(`SceneLoadReport.warnings`, 에디터 상태바·콘솔) | 단위 테스트 `SceneIOTests` 6개 추가(v2 저장, v1 절대 → 상대 후 v2로 저장, v2 안의 절대 경로 허용·정규화, 밖 경로 유지+경고, root 없음, PIE 스냅샷) |
| `EditorAPI::SetProjectRoot`, `InstantiatePrefabCommand`는 상대 경로로 기록, `RevertPrefabInstanceCommand`는 root 기준으로 해석하고 실패 시 저장값·root·최종 경로를 에러에 담는다. 에디터는 엔진 연결 때와 프로젝트를 바꿀 때 `_set_editor_project_root()`, 씬을 열 때 `LoadScene(path, project_root)` | 단위 테스트(`InstantiatePrefabCommandTests` 2개, `RevertPrefabInstanceCommandTests` 2개). 실제 실행(에디터 자동화): 샘플 프로젝트 데모 시드의 Barrel/Lamp/Fire가 `assets/prefabs/...`로 기록됨, 한글 경로 프로젝트의 version 1 씬(절대 경로) → 상대 경로로 읽힘 → Revert 동작 → 저장하면 version 2 + 상대 경로, **프로젝트를 다른 폴더로 옮기고 원본을 지운 뒤에도 Revert 동작**(예전 동작이라면 원본 절대 경로라 실패) |
| Player는 `LoadSceneFromFile(..., assetRoot)` | 실제 실행: `player_template` 재빌드 → `player_template_smoke`, `test_game_builder.py`의 실제 템플릿 빌드(새 프로젝트의 씬이 version 2) 통과 |

- **영향 범위 확인 중 발견한 결함**: `editor/gamebuild/game_builder.py`에 씬 버전 사본(`SCENE_VERSION = 1`)이 따로 있어서, 버전을 2로 올리면 에디터가 저장한 모든 씬을 Build Game이 "지원하지 않는 씬 버전"으로 거절했을 것이다. `project.py` 한 곳에서 가져오게 고쳤다(코드에 증상 주석). `project/__init__.py`가 `SCENE_FORMAT`/`SCENE_VERSION`을 내보내지 않던 것도 함께 고쳤다.
- **테스트 작성 중 같은 종류의 실수**: 새 테스트가 `path / "통.prefab.json"`(const char*)으로 경로를 만들어 `No mapping for the Unicode character ...` 예외가 났다. 제품 코드가 아니라 테스트의 결함이었고 `PathFromUtf8`로 고쳤다 — `CLAUDE.md` 경로 규칙이 테스트 코드에도 적용된다는 사례.
- 전체 C++ 테스트 619/619, `test_game_builder.py` 19/19, `test_demo_scene_seed.py` 5/5, `test_scene_instantiation.py` 6/6. 실행 순서 1·3의 에디터 자동화도 다시 통과.

### 5.5 실행 순서 5 — X-1b 데모 월드를 씬 파일로 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| 전환 전 상태에서 에디터를 띄워 데모 월드(코드 시드 + `scene.json`)를 `engine/assets/scenes/Demo.scene.json`(version 2, 111 엔티티)으로 저장. 이 PC의 절대 경로가 하나도 없음을 확인(`prefabPath`·`meshPath`·`texturePath`·`action` 모두 상대 경로) | 실제 실행 |
| `sample_project().start_scene = "assets/scenes/Demo.scene.json"`. 에디터 시작은 샘플·사용자 프로젝트 모두 "시작 씬 파일이 있으면 그것을 연다" 한 경로가 됐다. 코드 시드는 샘플 시작 씬이 없을 때의 대체 경로로만 남는다(원인을 로그로 남김). `scene.json` 파싱도 그 대체 경로에서만 한다 | 실제 실행(자동화): **전환 전후 비교** — 엔티티 111개의 이름별 컴포넌트 값이 전부 같음, 뷰포트 캡처 픽셀 차이 0.01%(파티클 움직임). 대체 경로(파일을 잠시 치움)도 전환 전과 같은 월드를 만듦 |
| 같은 씬을 Player로 | 실제 실행: 샘플 에셋을 복사한 한글 경로 프로젝트에서 Build Game → 게임 실행 로그에 `SceneIO - loaded ... Demo.scene.json (111 entities)`, 메시·텍스처 로드, 에러 없음. 게임 창 캡처로 같은 데모 월드가 그려지는 것을 확인(게임 카메라 시점, 그리드·기즈모 없음) |

- `demo_scene_seed.py`는 지우지 않았다(R10). 대체 경로와 `test_demo_scene_seed.py`(5/5)가 쓴다.
- 이제 샘플 프로젝트에서 Ctrl+S를 누르면 저장소의 `engine/assets/scenes/Demo.scene.json`이 바뀐다(샘플 프로젝트 = `engine/`이라는 기존 규칙의 결과). 데모 월드를 고치는 정상 경로이기도 하다.
- 회귀: `test_game_builder.py` 19/19, `test_scene_instantiation.py` 6/6, 실행 순서 1·3·4의 에디터 자동화 통과.

### 5.6 실행 순서 6 — G1 샘플 캐릭터 조작 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| 엔진: `ecs/ScriptContext`(thread_local, DLL 쪽 .cpp) + `quarterflying.script.registry()`. `ScriptSystem::Update`·`OnStop`이 훅을 부르는 동안만 유효 | 단위 테스트 `ScriptContextTests`(범위·중첩 복원). 실제 실행: 스크립트 밖 호출이 `no script is running ...` 예외 |
| 샘플: `assets/scripts/character_controller.py` — 순수 함수 `step(state, inputs, dt)` + `CharacterController`(입력 읽기 → step → `SetTransformPosition`). 카메라 앞 방향 기준 이동, Sprint, 점프, dt 0.1초 상한, 바닥 = 시작 높이 | 단위 테스트 `test_character_controller.py` 7개(30/60/144/240fps에서 점프 최고 높이 2% 이내·체공 시간 한 프레임 이내, 수평 이동 거리, dt 상한, 대각선, 카메라 기준, 공중 재점프 금지·바닥 고정, Sprint) |
| **계획서 초안 공식의 결함**: 초안의 `v -= g*dt; y += v*dt`(반암시적 오일러)는 최고 높이가 30fps 0.900, 144fps 0.979(정확 값 1.0)로 프레임률마다 8% 달랐다. 등가속도 정확 적분(`y += v*dt - g*dt²/2`)으로 바꿨다(스크립트에 주석) | 위 테스트가 지킨다 |
| `assets/prefabs/Character.prefab.json`(Transform·Renderable·ScriptComponent), Demo 씬에 인스턴스(0, 1.6, 7.5)와 Main Camera의 `CameraOrbitControlComponent`(target = Character). 에디터 경로(`instantiate_prefab`)로 만들고 `World.SaveScene`으로 저장 — `prefabPath`는 상대 경로, target은 UUID | 실제 실행 |
| 에디터 "한 판" | 실제 실행(에디터 자동화, 화면 캡처 4장): Play 후 입력 없이 정지, W 1.5초 → 카메라 앞(-Z)으로 6.27, D 1초 → +X 4.00, Shift+W 1초 → 7.32(약 1.8배), Space → 2.59까지 올랐다가 1.600으로 착지, 대각선·후진 포함 **10.4초 조작**, Stop → 시작 위치 복원. 궤도 카메라가 캐릭터를 따라감(캡처). **스크립트의 `move_speed`를 4→8로 고치고 다시 Play → 1초에 8.13 이동**(에디터는 Play마다 모듈을 다시 읽는다, 확인 후 원래 값으로 복원) |

- 전체 C++ 테스트 620/620, `test_game_builder.py` 19/19, 에디터 자동화(실행 순서 1·4) 회귀 통과.
- 빌드한 게임에서의 같은 조작은 G3(실행 순서 7)에서 확인한다.

### 5.7 실행 순서 7 — G3 빌드한 게임에서 같은 조작 (2026-10-15)

| 한 일 | 검증 수준 |
|---|---|
| 커서 잠금: `IPlatform::SetCursorLockRequested`/`SuspendCursorLock`(요청과 적용 분리, 기본 구현은 아무것도 안 함). `Win32Platform`은 전경일 때만 `ClipCursor`+커서 숨김, 임베드 모드는 무시, 창 클릭 시 요청이 있으면 다시 적용, 크기·위치가 바뀌면 다시 가둠, 종료 시 해제. `Engine::HandleFocusLost`가 `SuspendCursorLock`을 부른다(E-2의 ③) | 실제 실행(아래) |
| 옵션 경로는 `fullscreen`과 같은 길: `project.json` `build.window.cursorLock` → Build Settings 대화상자 체크박스 → `game.json` `window.cursorLock` → `GameConfig` → `EngineConfig::cursorLock`. 스크립트는 `quarterflying.input.set_cursor_locked(bool)` — 요청은 `InputActions`에 쌓이고 `TickFrame`이 World 업데이트 뒤 한 번 꺼내 플랫폼에 전한다(스크립트가 플랫폼에 직접 닿지 않음) | 단위 테스트(`GameConfigTest.CursorLock_DefaultsOff_AndParses`, `InputActionsTest.CursorLockRequest_IsTakenOnce`, `test_game_builder.py`의 저장·game.json 검사) |
| 빌드한 게임에서 같은 조작 | 실제 실행(자동화): 샘플 Demo 씬을 한글 경로 프로젝트로 복사해 커서 잠금을 켜고 Build Game → 게임 실행. **실제 키 입력**(keybd_event): W 1.5초 → z 7.5→1.501, D 1초 → x 0→4.0, Space → 최고 y 2.597. 커서가 클라이언트 영역에 갇힘(`GetClipCursor` = 클라이언트 사각형) → W를 누른 채 창을 최소화하면 캐릭터가 멈추고(E-2 Player 쪽) 잠금이 풀림(화면 전체) → 다시 앞으로 가져와 클릭하면 다시 잠김. Player 로그 `input actions: 4 loaded`, 잠금·해제 로그, 종료 코드 0. 게임 화면 캡처로 궤도 카메라가 캐릭터를 따라가는 것 확인 |

- 한계(IPlatform 주석에 기록): 커서를 가두기만 하고 가운데로 되돌리지 않는다. 창 가장자리에서는 마우스 이동량이 멈춘다 — 마우스로 시점을 무한히 돌리는 게임이 필요해지면 추가한다.
- 깨끗한 PC 확인은 `GAME_BUILD_PLAN.md`의 최종 통과 기준(환경 A/B)을 따른다 — 아직 하지 않았다.
- 회귀: C++ 622/622, `test_game_builder.py` 19/19, 에디터 자동화(실행 순서 1·3·6) 통과.

### 5.8 실행 순서 8 — A-1, A-2, A-3 모션 에디터 스텁 정직화 (2026-10-15)

| 항목 | 한 일 | 검증 수준 |
|---|---|---|
| A-1 | Motion > Import FBX를 끄고 이름에 "(미지원)", 툴팁에 JSON 경로 안내(메뉴 툴팁 표시 켬). `do_import_fbx()`는 지우지 않고 안내 문구만 사실대로 바꿨다(R10). FBX SDK 재배포 라이선스 확인은 외부 형식 임포트를 시작할 때 한다 | 실제 실행(자동화): 메뉴 항목이 꺼져 있고 툴팁에 JSON 안내 |
| A-2 | Motion Graph: sin으로 지어낸 "속도"와 그 차이로 만든 "가속도"를 없애고, 섹션의 `speed` 배율만 계단으로 그린다(섹션 밖 = 1.0x, 1.0x 점선). 섹션이 없으면 "섹션 없음" 안내. 코드에 증상·원인 주석 | 실제 실행: 섹션 0.5x/2.0x 액션과 섹션 없는 액션의 그래프 캡처 |
| A-3 | Motion Editor 진입 시 샘플 자동 로드를 없앴다. 빈 상태로 시작하고 "스켈레톤을 고르세요" 안내. "샘플 불러오기" 버튼은 현재 프로젝트의 `assets/motion`에 샘플 파일이 있을 때만 켜진다(없으면 툴팁에 빠진 파일). **(확인 필요)였던 것**: 샘플 파일이 없는 사용자 프로젝트에서는 진입할 때마다 "샘플 자산 파일을 찾을 수 없습니다" 오류가 떴다(코드 확인 — 이번 변경으로 사라짐) | 실제 실행: 샘플 프로젝트 진입 → 스켈레톤 없음 + 안내, 버튼 클릭 → 스켈레톤 로드. 샘플 없는 프로젝트 → 버튼 꺼짐, 오류 없음 |

- 회귀: 에디터 자동화(실행 순서 1) 19/19, `test_motion_sound_integration.py`·`test_action_playback.py`·`test_scene_action_playback.py` 통과.
- **발견한 기존 문제(고치지 않음)**: 프로젝트를 바꿔도 Motion 프리뷰에 이전 프로젝트의 스켈레톤·레이어가 남는다(`_refresh_project_panels`는 목록만 다시 읽는다). 프로젝트 전환 시 프리뷰를 비울지 후속으로 정한다.

### 5.9 실행 순서 9 — R-1 방향광, R-2 인스턴스 색 (2026-10-15)

| 항목 | 한 일 | 검증 수준 |
|---|---|---|
| R-1 | `DirectionalLightComponent { direction, color, intensity, ambient, priority, enabled }`(리플렉션 등록 → 씬 파일·Inspector·Undo에 자동). 기본값은 라이트 도입 전 고정 조명과 같다. 선택 규칙의 유일한 구현 `SelectSceneLighting`(켜진 것 중 priority 최고 → UUID 최소, direction 0이면 기본 방향 + 경고). 경계 값 `renderer/SceneLighting.h`: RenderSystem이 Renderer 소유의 대상에 써 넣고(`GetSceneLightingTarget`), SceneMeshRenderer는 유니폼(`lightDir`, `lightColor`, `ambientStrength`)으로만 쓴다(R6). 선택이 바뀔 때만 로그(라이트 없음 / 사용 중인 라이트 / 여러 개 켜짐 경고) | 단위 테스트 `LightingMaterialTests` 6개(라이트 없음 = 기본, 기본 컴포넌트 = 라이트 없음과 같은 값, 방향 반전·정규화와 색 x 세기, priority·꺼짐, 동순위 UUID — 저장·불러오기 뒤에도 같은 선택, 방향 0). 실제 실행: 라이트 추가 → 화면 59% 픽셀이 바뀜(옆에서 오는 따뜻한 빛), 같은 씬을 빌드한 게임에서 같은 조명(캡처 비교, 게임 로그 `using DirectionalLight 'Sun'`) |
| R-2 | `RenderableComponent.baseColor`(tint, 새 기본 흰색), `roughness`(0.6)·`metallic`(0.0) — 인스턴스 데이터로만 가고 배치 키는 그대로(`MakeInstanceMaterial`). 옛 파일 보정은 X-1a의 파일 로드 훅(`baseColor` 키가 없으면 텍스처 없을 때 회색, 있을 때 흰색). Inspector 표시 이름에 "(셰이더 미사용)" | 단위 테스트 4개(새 컴포넌트 흰색, 옛 씬 파일 보정, 옛 프리팹 파일 보정, **AddComponent `{}`·일부 필드 JSON은 보정되지 않음**). 실제 실행: X-1b 때 저장한 옛 씬(라이트·baseColor 없음)을 지금 빌드로 열어 그때 캡처와 비교 — 다른 픽셀 0.09%(파티클). 같은 배치의 큐브 3개에 빨강·초록·파랑 → 각자 다른 색(에디터·빌드한 게임 캡처) — `CLAUDE.md` "알려진 미검증 항목"의 인스턴스별 색 전달 검증 |

**계획에서 달라진 점**

- `scene.json`의 lights를 컴포넌트로 옮기지 않았다. 세 개 모두 **점광원**(`type: point`)이라 방향광으로 옮길 수 없고, 예전에도 화면에 쓰인 적이 없다. 대신 샘플 Demo 씬에 기본값 방향광 "Sun"을 넣었다(넣기 전후 다른 픽셀 0.08%).
- `scene.json`의 material 색도 쓰지 않았다. 대체 경로(`scene_instantiation.py`)는 R-2 이전과 같은 색(회색/흰색)을 **명시해서** 넘긴다 — 빼면 새 기본값(흰색)으로 그리드가 하얗게 바뀌고, 대체 경로 화면이 Demo 씬과 달라진다.
- 텍스처 경로가 있는데 불러오기에 실패한 옛 오브젝트는 예전에는 회색, 지금은 흰색으로 그려진다(보정이 경로 유무로 판단하므로). 텍스처 실패는 이미 경고 로그가 난다.

**검증 중 발견해 고친 것**

- **오래된 Player 템플릿을 Build Game이 알아채지 못함**: R-1/R-2 뒤 `player_template`을 다시 만들기 전에 빌드한 게임이 라이트와 `baseColor`를 말없이 건너뛰어 에디터와 다른 화면이 나왔다(에러·경고 없음). 엔진 버전 문자열(`0.1.0`)이 같아서 기존 템플릿 검증을 통과했다. Build Game이 템플릿의 `quarterflying_engine.dll` 해시를 에디터가 쓰는 엔진 DLL과 비교해 다르면 경고하게 했다(`engine_build_mismatch`, 단위 테스트, 실제 템플릿에서 오탐 없음 확인). 설치본(②)에서는 같은 빌드라 경고가 나지 않는다.

- 회귀: C++ 632/632, `test_game_builder.py` 20/20, 에디터 자동화(실행 순서 1·4·6·8) 통과.

### 5.10 실행 순서 10 — G2, E-3, E-4, E-5(나머지), E-6, A-4, A-5, X-2 (2026-10-15)

| 항목 | 한 일 | 검증 수준 |
|---|---|---|
| E-3 | 상태바 모드 표시(■ EDIT / ▶ PLAY / ⏸ PAUSE). Play 화면 위쪽 안내 줄: 뷰포트에 포커스가 있으면 "게임 입력 중", 없으면 "화면을 클릭하면 게임 입력을 받습니다". **계획에서 달라진 점**: 뷰포트 위 오버레이 대신 위쪽 줄 — 뷰포트는 네이티브 창(HWND)이라 Qt 위젯을 그 위에 겹쳐 그릴 수 없다 | 실제 실행(자동화) |
| E-4 | Delete / Ctrl+D를 Scene Hierarchy 트리와 뷰포트에만(위젯 단축키), Edit 모드에서만. 기존 Undo 명령을 거친다 | 실제 실행: 트리·뷰포트에서 삭제·복제·Ctrl+Z, Inspector 입력칸의 Delete는 엔티티를 지우지 않음, Play 중 Delete는 게임 키 |
| E-5 | Help > Keyboard Shortcuts — 메뉴의 QAction 단축키에서 표를 만든다(단축키를 바꾸면 표도 바뀜) + 위젯 단축키·마우스 조작. 게임 조작은 넣지 않는다 | 실제 실행: 표에 Ctrl+Alt+P·Ctrl+Z·Ctrl+S, 게임 액션 없음 |
| E-6 | Inspector "＋ Component"(등록된 컴포넌트 목록에서, 이미 있는 것과 프리팹 메타데이터 제외, EditorAPI 명령). Scene Hierarchy 검색(맞는 항목과 조상만, 500ms 갱신 뒤에도 유지). 다중 선택(Ctrl/Shift) — Delete/Duplicate는 선택 전체를 트랜잭션 하나로(Ctrl+Z 한 번), 조상이 함께 선택된 자손은 따로 처리하지 않는다. 도킹·분할 배치 저장/복원(`config.json` layout, View > Reset Layout은 기본 도킹으로) | 실제 실행 15개 |
| G2 | Input Actions 도크(프리팹 브라우저와 탭). 바인딩 칸은 파일의 bindings 배열 JSON이고, 해석은 엔진(`Engine.SetInputActionJson`/`RemoveInputAction`/`SaveInputActions` 바인딩 추가)이 한다. 엔진이 건너뛴 바인딩은 경고로, 거절은 이전 값 유지 + 이유 표시. 편집은 실행 중인 엔진에 바로 반영 | 실제 실행(한글 경로 새 프로젝트): 액션 추가, 모르는 키 경고, 깨진 JSON 거절, 저장 파일 내용, **Play 중 Jump를 Space→J로 바꾸면 스크립트가 J에 반응하고 Space에는 반응 안 함**, 파일에서 다시 읽기 |
| A-4 | Motion > New Action / Save Action(파일이 있으면 대화상자 없이 덮어씀) / Save Action As / Load Action(기본 폴더 = 프로젝트 `assets/actions`) / Recent Actions(에디터 설정). 타임라인 ←/→(Shift = 10프레임), Delete = 마지막으로 클릭한 이벤트 삭제. 마우스로 이벤트 추가(우클릭)·이동(드래그)·삭제(우클릭)는 **이미 있었다**(코드 확인). 사운드 클립 경로는 **이미** 저장 시 액션 파일 기준 상대 경로로 바뀐다(Sound Lite §6.4, 코드 확인) | 실제 실행 8개(한글 경로). Delete 키 확인은 마커 클릭 대신 선택 상태를 직접 설정해서 했다 |
| A-5 | 반복 토글, 배속(0.5x/1x/2x — 미리보기 재생 속도만), 반복을 끄면 끝에서 멈추고 다시 누르면 처음부터. 2D 폴백 프리뷰의 "Attack" 하드코딩을 역할→스타일 표(`ROLE_PREVIEW_STYLES`)로 바꾸고 모든 섹션의 이름·역할·배율을 표시. 프리뷰·타임라인·그래프의 재생 헤드 동기화는 **이미** 연결돼 있었다(코드 확인) | 실제 실행 6개 + 2D 프리뷰 캡처 |
| X-2 | 코드로 합성한 샘플 효과음 3개(`Hit`, `Whoosh`, `Coin`, 생성 스크립트 `assets/sfx/make_sample_sfx.py`가 출처 기록) + `SoundTest.action.json`이 4가지 소리를 쓰게. 엔티티 이름 "Sound Test (sample)"(Demo 씬과 대체 경로 시드 모두) | 실제 실행: Play에서 4개 클립 재생 호출 성공. **소리가 실제로 들리는지는 사람이 들어야 한다** |

**검증 중 발견해 고친 것**

- **PySide6가 `QShortcut(..., context=Qt.WidgetShortcut)`의 context 키워드를 말없이 무시**한다. 그래서 트리와 뷰포트의 Delete가 둘 다 창 전체 단축키가 되어 "모호한 단축키"로 둘 다 동작하지 않았다(에러·경고 없음). 만든 뒤 `setContext`로 정하는 `widget_shortcut()` 하나로 모았다(코드에 증상 주석).
- **부모와 자식을 함께 선택해 Delete하면 자식만 지워짐**: 하나로 줄어든 선택에서 계산한 루트 대신 마지막으로 클릭한 항목을 썼다. 루트를 쓰게 고쳤다.
- **Input Actions 패널의 오류 문구가 바로 지워짐**: 거절된 값을 되돌리는 다시 그리기가 오류 문구를 덮어썼다. 순서를 바꿨다.
- **오래된 템플릿 검사의 빈틈**: 엔진 DLL만 비교하면 바인딩(.pyd)만 바뀐 경우(예: 이번 G2 바인딩)를 놓친다. `.pyd`도 비교하게 넓혔다(실제로 이번 변경 직후 경고가 났고, 템플릿을 다시 만들자 사라짐).

**남은 것 / 범위 밖으로 둔 것**

- 다중 선택은 Delete/Duplicate에만. Inspector와 드래그 재부모화는 현재 항목 하나만 다룬다.
- E-7(언어 통일)은 사용자 결정 대기. R-4(머티리얼 에셋)는 결정(경로 필드로 통일) 후 구현했다 — §5.11.
- 프로젝트를 바꿔도 Motion 프리뷰에 이전 스켈레톤이 남는 기존 문제(§5.8).
- 깨끗한 PC(VM) 최종 확인.

- 전체 회귀: C++ 632/632, Python 단위 테스트 전부(`test_game_builder.py` 20/20 포함), 에디터 자동화 11종(실행 순서 1·3·4·6·8·10 항목별) 전부 통과, 빌드한 게임 자동화(G3) 12/12.

### 5.11 실행 순서 11 — R-4 머티리얼 에셋, 경로 필드로 통일 (2026-10-15)

| 항목 | 한 일 | 검증 수준 |
|---|---|---|
| 컴포넌트 | `RenderableComponent` 스키마 `_version` 2: `meshPath`, `materialPath`(신규), `texturePath`(인스턴스별 덮어쓰기), `baseColor`(tint), `castShadows`. `meshHandle`·`materialHandle`·`roughness`·`metallic` 삭제. 옛 파일(`_version` 1)은 그 키들을 무시하고 읽으며, `meshHandle`이 0이 아니면 파일 종류·version·값을 적은 경고를 남긴다(파일 로드 훅, `Reflection.cpp`). 씬 파일 version은 그대로(2) — 바뀐 것은 이 컴포넌트 스키마뿐 | 단위 테스트(스키마 2 저장에 핸들·roughness 없음, 옛 스키마 파일 로드 + 경고 + R-2 회색 보정 유지). 기존 테스트의 핸들 필드는 경로 필드로 옮겼다 |
| 머티리얼 파일 | `*.material.json` = `{format: "quarterflying.material", version: 1, baseColor, texture, roughness, metallic}`. 파서는 C++ 한 곳(`asset/MaterialAsset`: `ParseMaterial`·`LoadMaterialFile`). 틀린 형식·범위는 키 이름을 적은 에러, 높은 version은 `NotSupported`. 샘플 `assets/materials/{Gold,RedRubber,Checker}.material.json`, 새 프로젝트에 `assets/materials/` 폴더 | 단위 테스트 `MaterialAssetTests`(필드, 기본값, 잘못된 값, 한글 경로 파일, 없는 파일) |
| 합성·로드 | 합성 규칙은 `MakeInstanceMaterial`·`EffectiveTexturePath` 두 함수에만 있다(색 = 머티리얼 × 컴포넌트, 텍스처 = 컴포넌트 우선, roughness·metallic = 머티리얼 또는 0.6/0.0). `RenderSystem::ResolveMaterial`이 경로당 한 번 읽고 캐시(실패도 다시 시도하지 않고 경고 한 번 → 기본 머티리얼). 로더는 Engine이 주입한다(`SetMaterialLoader`) — `ecs/`가 `asset/`에 의존하지 않는다(R6) | 단위 테스트(합성, 덮어쓰기, 경로당 한 번 로드, 로더 없음) |
| 셰이더 | `SceneMeshRenderer`가 인스턴스 roughness·metallic을 쓴다. 지수 32·2^((0.6−r)·8), 반사 세기 0.75·(1−r), 반사색 = mix(1, 표면색, metallic), 확산 × (1 − 0.9·metallic). 기본값(0.6/0.0)에서 예전 고정값(지수 32, 세기 0.3)과 같다 | 실제 실행(아래) |
| 코드 등록 메시 | `RegisterMesh(이름, 메시)` — `meshPath`에 그 이름을 쓰면 파일 대신 그 메시. 이름은 저장·스냅샷에 안전하고, 에셋 기준 폴더가 바뀌어도 남는다 | 단위 테스트 |
| Build Game | 수집 표에 `materialPath`, 머티리얼 파일 안의 경로 키는 `MATERIAL_PATH_FIELDS` 표 하나(`texture`). 없는 머티리얼·텍스처는 "어느 씬의 어느 엔티티" 형식으로 빌드 실패 | `test_game_builder.py` 21/21(머티리얼과 그 텍스처 수집, 없는 머티리얼 거절) |
| 샘플 | `Character.prefab.json`을 스키마 2로. 샘플 Demo 씬에 머티리얼 큐브 3개("Material Gold/RedRubber/Checker")를 넣고 스키마 2로 다시 저장(116 엔티티, 절대 경로 없음, 옛 회색 104개 유지) | 실제 실행(아래) |

**실제 실행 검증 (에디터 `HAS_ENGINE=True` 자동화 + 빌드한 게임 캡처)**

- 옛 스키마 Demo 씬을 지금 빌드로 연 화면 = R-4 직전 캡처: 다른 픽셀 0.08~0.09%(파티클) → 셰이더 매핑이 기본값에서 예전 화면과 같다.
- 머티리얼 3개가 한 화면에서 서로 다르게 보인다(캡처): Gold는 metallic 1이라 확산이 0.1배로 줄어 어두운 금색, RedRubber는 무광 빨강, Checker는 머티리얼의 텍스처.
- **roughness 전달**: 같은 메시·같은 회색(0.4)에서 roughness만 0.6 → 1.0으로 바꾸면 큐브 윗면의 반사 그라데이션이 사라진다 — 다른 픽셀 5.06%, 같은 머티리얼을 두 번 찍은 잡음은 0.30%. 처음엔 흰색 큐브로 재서 차이가 0이었다 — 확산만으로 이미 1.0에 닿아 반사가 잘려서다. 측정 조건의 문제였고 코드 문제는 아니었다. metallic은 위 Gold의 어두워짐으로 확인. 이것으로 `InstanceData`의 roughness·metallic 필드 GPU 레이아웃도 확인됐다(`CLAUDE.md` 알려진 미검증 항목).
- 스키마 2로 저장한 씬을 다시 열어도 같은 화면(0.08%), 저장된 Renderable 전부 `_version` 2.
- 한글 경로 프로젝트("머티리얼 게임")에서 Build Game: 머티리얼 3개와 Checker의 텍스처가 빌드에 들어갔고, 스모크 로그에 에러 없음. 빌드한 게임 화면이 에디터와 같다(캡처).

**계획에서 달라진 점**

- 계획은 `meshHandle`을 "`RegisterMesh`용 런타임 값으로 남긴다"였다. 컴포넌트에 남기면 리플렉션 밖 필드라도 Undo·Play/Stop 스냅샷을 지나며 사라져 같은 문제가 되풀이된다. 그래서 컴포넌트에서 아예 빼고, 코드 등록은 이름으로 바꿨다(`RegisterMesh(이름)` → `meshPath`).
- R-2 때 Inspector에 붙였던 "(셰이더 미사용)" 표시는 필드(roughness·metallic)가 머티리얼로 옮겨지며 함께 없어졌다.
- 셰이더 파일화는 계획대로 하지 않았다(D5 `builtin/` 결정 대기).

**남은 것 / 범위 밖으로 둔 것**

- 머티리얼 편집 UI가 없다. Inspector에서 `materialPath`는 일반 문자열 칸이고, 머티리얼 내용은 파일을 직접 고친다. 머티리얼 파일을 고쳐도 실행 중인 엔진은 경로당 캐시를 쓰므로 다시 읽지 않는다(프로젝트를 다시 열면 읽음).
- 샘플 `Lamp`·`Barrel` 프리팹은 옛 스키마(`meshHandle: 0`) 그대로 둔다 — 옛 파일 호환 경로가 실제 파일로도 쓰인다.

**검증 중 발견한 것 (제품 코드 문제 아님)**

- 에디터 자동화 일부가 레이아웃을 저장소에 올라가 있는 `engine/editor/config.json`에 써서, 다음 자동화의 뷰포트 크기(509 vs 533px)가 달라져 캡처 비교가 실패했다. `git checkout`으로 되돌린 뒤 통과. 자동화를 돌린 뒤에는 `config.json` 변경 여부를 확인한다.
- G3 자동화 스크립트가 테스트 프로젝트에 `materials/`를 복사하지 않아 Build Game이 "엔티티 'Material Gold' … 파일이 없습니다"로 빌드를 막았다 — 수집 검사가 의도대로 동작한 것이다. 스크립트에 폴더를 추가해 통과.

- 전체 회귀: C++ 643/643, Python 단위 테스트 전부(`test_game_builder.py` 21/21 포함), 에디터 자동화 13종 + R-4 자동화 2종 통과, 빌드한 게임 자동화(G3) 12/12, `player_template_smoke` 통과. Player 템플릿은 마지막 빌드로 다시 만들었고 Build Game의 템플릿 해시 검사에서 불일치 없음.
