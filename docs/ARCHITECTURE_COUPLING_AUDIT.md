# 모듈 결합도 점검 및 책임 분리 (2026-09-25)

목적: 버그가 났을 때 **원인이 있는 모듈을 바로 짚을 수 있도록** 모듈 사이의 의존 방향과
책임 경계를 점검한다. 이 문서는 점검 결과, 이번에 실제로 고친 것, 남은 권장 작업을 기록한다.

점검 방법: C++는 모듈별 `#include` 교차 참조를 전수 집계했고, Python 에디터는 모듈별
import와 엔진 API 직접 호출 지점을 전수 집계했다. 의심 지점은 해당 코드를 직접 읽고 판정했다.

판정 기호: 🔴 원인 추적을 실제로 방해함 / 🟠 구조적 부채 / 🟡 주의 / 🟢 양호

---

## 1. 요약

| 영역 | 판정 | 한 줄 요약 |
|---|---|---|
| 저장소 무결성 | 🔴 | HEAD 커밋이 **커밋 안 된 파일 30개**에 의존한다. 새로 clone하면 빌드가 안 된다 |
| `core` | 🟠 | 기반 유틸(로그/메모리/에러)과 최상위 조립자(`Engine`)가 한 모듈에 섞여 있다 |
| `renderer` ↔ `animation` | 🟠 | 에디터 전용 모션 프리뷰 상태가 렌더러 공개 헤더에 박혀 있다 |
| `ecs` → `renderer` | 🟡 | `RenderSystem`/`ParticleSystem` 두 파일에 국한된다. ECS 코어는 renderer를 모른다 |
| `ecs` ↔ `prefab` | 🟢 | 파일 단위로만 순환한다(`PrefabInstanceComponent.h`는 의존성 없는 헤더). 의도된 결정으로 문서화돼 있다 |
| `animation`, `asset`, `job`, `platform` | 🟢 | `core` 외 의존 없음. `animation`은 `core`조차 의존하지 않는 완전 독립 모듈이다 |
| 에디터: 엔티티 id 변환 | 🔴 → ✅ | 7곳에 중복돼 있었고, 이 지점에서 실제 버그가 2번 났다. **이번에 한 곳으로 통합했다** |
| 에디터: `main._connect_engine_later` | 🔴 → ✅ | 연결과 데모 엔티티 생성이 섞여 실패 로그가 원인을 잘못 가리켰다. **이번에 분리했다** |
| 에디터: 예외 처리/로그 | 🟠 | `print`로 삼키는 패턴이 많고, 로그 출처 태그가 일관되지 않다 |
| 에디터: `core/`, 패널 간 결합 | 🟢 | `core/`는 패널을 모른다. 패널끼리는 import하지 않는다(임베드 1건 제외) |

---

## 2. C++ 엔진

### 2.1 🔴 HEAD가 untracked 파일에 의존

`git show HEAD:engine/renderer/Renderer.h`는 `../animation/MotionPreviewState.h`,
`BoneLineRenderer.h`, `SceneMeshRenderer.h`, `DebugGridRenderer.h`, `RenderMode.h`를 include한다.
`HEAD:engine/CMakeLists.txt`는 `animation/*.cpp`, `ecs/RenderSystem.cpp`를 소스로 나열한다.
그런데 이 파일들(`engine/animation/` 전체, `ecs/RenderSystem.*`, `renderer/BoneLineRenderer.*` 등
30개)은 **한 번도 커밋된 적이 없다**(`git ls-files engine/animation` → 0개).

지금 이 PC의 작업 트리에서는 빌드되지만, 다른 PC에서 clone하거나 작업 트리를 정리하면 빌드가
깨진다. "어느 커밋부터 동작했는가"를 `git bisect`로 추적할 수도 없다. 결합도 이전의 문제라
가장 먼저 처리해야 한다. **이 파일들이 누구의 진행 중 작업인지 확인한 뒤 커밋해야 하므로, 이번
작업에서는 손대지 않았다.**

### 2.2 모듈 의존 그래프 (include 기준)

```
                 bindings ──────────────┐ (경계 모듈 - 전부 의존하는 게 정상)
                    │                   │
   core/Engine ──┬──┼──> ecs ──> job    │
   (조립자)      ├──> renderer ──> animation   ← 🟠 2.4
                 ├──> platform          │
                 └──> animation   ← 🟠 2.3
   ecs/RenderSystem, ParticleSystem ──> renderer   ← 🟡 2.5
   ecs/Reflection.cpp ──> prefab/PrefabInstanceComponent.h ──> (없음)   ← 🟢
   prefab ──> ecs

   모든 모듈 ──> core/{logging, memory, EngineError, Types, UUID}   (기반 계층)
```

### 2.3 🟠 `core`가 기반 계층과 조립자를 겸한다

`core/`에는 모든 모듈이 의존하는 기반 유틸(`logging/`, `memory/`, `EngineError.h`, `Types.h`)과,
모든 모듈을 조립하는 최상위 `Engine` 클래스가 함께 있다. 그 결과 다음 문제가 생긴다.

- `core/Engine.h`가 `platform/IPlatform.h`, `renderer/Camera.h`, `renderer/RenderMode.h`,
  `animation/MotionPreviewState.h`를 **공개 헤더에서** include한다. `Engine.h`를 include하는
  모든 곳(바인딩 전체 포함)이 렌더러와 애니메이션 헤더에 전이적으로 묶인다.
- "core는 최하위 계층"이라는 가정이 성립하지 않는다. 어떤 버그가 core에서 났다고 해도
  그게 기반 유틸 쪽인지 조립 쪽인지 모듈 이름만 보고는 알 수 없다.

**권장:** `Engine`/`PlatformFactory`를 `engine/runtime/`(또는 `app/`)으로 옮긴다. `Engine.h`에서는
`Camera`, `MotionPreviewState`를 전방 선언으로 바꾼다(멤버가 `unique_ptr`라 전방 선언으로 충분하다.
단 소멸자를 `.cpp`에 두어야 한다).

### 2.4 🟠 렌더러가 에디터 전용 모션 프리뷰를 안다

`renderer/Renderer.h`가 `animation/MotionPreviewState.h`를 include하고 `MotionPreviewState*`를
멤버로 들고 있다. 실제로 이 상태를 쓰는 곳은 뼈대 선을 그리는 `BoneLineRenderer` 하나뿐이다.
에디터 Motion Mixer 한 기능 때문에 렌더러 코어 헤더가 애니메이션 모듈 전체(`LayerMixer`,
`KeyframeClip`, …)에 묶여 있다.

**권장:** 렌더러는 "그릴 선분 목록" 같은 순수 데이터만 받게 한다. 예를 들어
`BoneLineRenderer::Submit(const std::vector<LineSegment>&)`로 받게 하고, 포즈를 선분으로 바꾸는
일은 `Engine`(조립자) 또는 animation 쪽에서 한다. 그러면 모션 프리뷰가 이상할 때 "포즈 계산이
틀렸나(animation), 선 그리기가 틀렸나(renderer)"를 경계에서 바로 나눠 볼 수 있다.

> 2.3, 2.4의 대상 파일(`Engine.h`, `Renderer.h`, `animation/`)은 현재 커밋 안 된 진행 중 작업과
> 겹친다(2.1). 그래서 이번에는 권장 사항으로만 남긴다.

### 2.5 🟡 ECS 시스템 → 렌더러

`ecs/RenderSystem.cpp`, `ecs/ParticleSystem.cpp`가 `renderer/InstancedBatchManager.h`,
`Mesh.h`, `Camera.h`를 include한다. ECS 코어(`World`, `Entity`, `ECSRegistry`)는 renderer에
의존하지 않고, 두 시스템 모두 `batchManager == nullptr`를 허용하는 규약이라 렌더러 없이도
돈다. **당장 문제는 아니다.** 시스템이 더 늘어나면 `ecs/systems/`로 분리하는 것을 고려한다.

### 2.6 🟢 양호한 모듈

- `animation/`: 모듈 밖 include가 0건이다. 로직을 단독으로 테스트할 수 있다
  (`AnimationSamplerTests`, `LayerMixerTests` 등). 다른 모듈을 분리할 때 이 모듈을 기준으로 삼는다.
- `asset/`, `job/`, `platform/`: `core` 기반 계층에만 의존한다.
- `ecs` ↔ `prefab`: `PrefabInstanceComponent.h`의 주석과 `docs/PREFAB_IMPLEMENTATION_PLAN.md`
  "모듈 경계 정정"에 이유가 기록돼 있다. 링크 순환도 아니다.

---

## 3. Python 에디터

### 3.1 🟢 계층 구조

```
main.py ──> panels/*, motion_editor, viewport, demo_scene_seed, config_manager
motion_editor ──> panels/(animation_preview, event_timeline, motion_graph, …), core/action_data
panels/* ──> core/*, style/theme, engine_binding
core/*   ──> (core 내부만; sound_player만 QtMultimedia 사용 - 의도된 것)
engine_binding ──> quarterflying (.pyd) ← 확장 모듈을 import하는 유일한 지점
```

역방향 import(core → panels, panels → main)는 없다. 패널 사이의 import는
`animation_preview → motion_mixer`(위젯 임베드) 1건뿐이다.

### 3.2 🔴 → ✅ 엔티티 id 변환이 7곳에 흩어져 있었음 (이번에 수정)

**증상(과거 실제 버그 2건, CLAUDE.md 관례 1번 사례):**
Inspector가 raw int를 그대로 넘겨 `incompatible function arguments`가 났고, 바깥 try가 이를
삼켜서 Inspector가 한 번도 정상 동작한 적이 없었다. Scene Hierarchy는 `Entity` 객체를
`Signal(int)`로 emit해서 id가 조용히 0이 됐다.

**원인(구조):** 에디터 id(`int`)를 엔진 타입(`Entity`)으로 바꾸는 규칙이 `inspector.py`에
5곳, `scene_hierarchy.py`에 2곳(별도 `_to_entity` 헬퍼 포함) 흩어져 있었다. 한 곳이라도
빠뜨리면 같은 종류의 버그가 다시 난다. `_to_entity`의 docstring은 "refresh 항목은 Entity 객체를
들고 있다"고 설명했지만, 실제 코드는 이미 `eid.id`(int)를 저장하고 있어 설명과 코드가 어긋나 있었다.

**수정:** `engine_binding.to_entity()` 하나로 통합했다. 잘못된 입력은 엔진 호출 전에 원인이
적힌 에러로 막는다(CLAUDE.md 관례 3번).

| 입력 | 결과 |
|---|---|
| `5` | `Entity(id=5)` |
| 이미 `Entity` | 그대로 반환 |
| `-1` ("선택 없음" 값이 새어 나옴) | `ValueError: … -1이면 '선택 없음' 상태의 id가 엔진 호출까지 새어 나온 것` |
| `True` (bool은 int의 하위 클래스라 조용히 1이 됨) | `TypeError` |
| `HAS_ENGINE=False`에서 호출 | `RuntimeError: … 호출부에서 HAS_ENGINE을 먼저 확인해야 합니다` |

### 3.3 🔴 → ✅ `_connect_engine_later()`의 책임 혼재 (이번에 수정)

**증상:** 이 함수(150줄)는 ① World 활성화 ② 패널에 registry/EditorAPI 연결 ③ 검증용 데모
엔티티 6개 생성을 바깥 try 하나로 묶고 있었다. 그래서 ③의 Main Camera나 Test Cube 생성이
실패해도 로그에는 **`ECS 연결 실패`**가 찍혔다. 연결은 이미 끝난 뒤였으므로 원인을 엉뚱한
곳에서 찾게 만드는 로그다. 또 카메라 생성이 실패하면 뒤의 무관한 엔티티 5개도 전부 건너뛰었다.

**수정:**
- ①은 `_activate_world()`, ②는 `_wire_panels_to_engine()`으로 나눴다. 각각 실패하면
  `World 활성화 실패: …` / `패널-엔진 연결 실패: …`로 **자기 단계 이름**을 남긴다.
- ③은 새 모듈 `editor/demo_scene_seed.py`로 옮겼다. Qt 의존성이 없다. 엔티티마다 독립된
  단계로 돌기 때문에 하나가 실패해도 나머지는 계속 만들고,
  `[DemoSeed] <이름> 생성 실패 - <예외타입>: <메시지>`를 남긴다.

### 3.4 🟠 예외를 `print`로 삼키는 패턴 (남은 작업)

`main.py`에는 `except`가 13개, `inspector.py`에는 8개 있고 대부분 `print`만 하고 넘어간다.
로그 태그도 섞여 있다(`[Inspector] …`, `Inspect Error: …`, `[Editor] …`).
콘솔에 무엇이 찍혔는지로 출처를 찾기 어렵고, 상태바에는 안 나오는 경우도 많다.

**권장:** 표준 `logging`으로 모듈별 logger(`logging.getLogger("editor.inspector")`)를 두고,
`except`에서는 `logger.exception(...)`으로 스택 트레이스까지 남긴다.

### 3.5 🟠 패널이 엔진 객체를 직접 들고 있음 (남은 작업)

`inspector`, `scene_hierarchy`, `prefab_browser`가 `registry`와 `editor_api`를 직접 받아
`GetComponentJson`, `SetComponentFieldJson` 같은 엔진 API를 호출한다. 3.2처럼 변환 규칙은
통합했지만, JSON 파싱과 `"{}"` 빈값 판정 같은 엔진 프로토콜 지식은 여전히 패널마다 흩어져 있다.

**권장:** 에디터 쪽 파사드(`editor/core/scene_model.py` 같은 것)를 두고 패널은 거기에만
말하게 한다. 그러면 "엔진이 틀린 값을 줬나, 패널이 잘못 그렸나"를 파사드 경계에서 가를 수
있고, 가짜 파사드로 패널을 테스트할 수도 있다.

### 3.6 🟡 기존 테스트 상태 (이번 변경과 무관, HEAD에서도 동일)

- `test_demo_scene.py`: "Scene Hierarchy Integration Test" 진입 직후 출력 없이 rc=127로 죽는다.
  HEAD 버전 파일로 되돌린 복사본에서도 똑같이 재현된다.
- `test_demo_scene.py`, `test_viewport_compatibility.py`: 기본 cp949 콘솔에서 `✓`/`✗` 출력 시
  `UnicodeEncodeError`가 난다. `PYTHONIOENCODING=utf-8`로 돌리면 뒤쪽 테스트는 5/5 통과한다.

---

## 4. 이번 변경 목록과 검증 단계

변경 파일:
- `engine/editor/engine_binding.py`: `to_entity()` 추가
- `engine/editor/panels/inspector.py`: `ge_python.Entity(...)` 5곳을 `to_entity(...)`로 교체
- `engine/editor/panels/scene_hierarchy.py`: 2곳 교체, 중복 헬퍼 `_to_entity` 삭제
- `engine/editor/main.py`: `_connect_engine_later`를 3단계로 분리
  (`_demo_scene_seeded` 플래그는 옛 `_default_camera_created`를 대체한다)
- `engine/editor/demo_scene_seed.py` (신규)
- `engine/editor/test_demo_scene_seed.py` (신규)

검증 단계(CLAUDE.md 관례 2번):
1. **단위 테스트 통과:** `test_demo_scene_seed.py` 5/5. `to_entity`는 실제 빌드된
   `quarterflying.cp314-win_amd64.pyd`의 `Entity` 타입으로 검증했다. 기존 에디터 테스트
   (`test_action_playback`, `test_scene_action_playback`, `test_sound_*`,
   `test_motion_sound_integration`, `test_viewport_compatibility`)는 회귀 없이 통과한다.
2. **실제 실행 검증 (일부):** 에디터를 `HAS_ENGINE=True`로 25초 동안 띄웠다. 로그에서
   `엔진 ECS 연결 완료`와 `[DemoSeed]` 6개 단계 모두 `생성 완료`를 확인했고, 예외나
   트레이스백은 없었다. Scene Hierarchy의 500ms 갱신에서도 `엔진 동기화 실패`가 나오지 않았다.
   **확인하지 못한 것:** Inspector에서 엔티티를 클릭해 컴포넌트를 편집하는 경로(`to_entity`를
   쓰는 Inspector 호출 5곳)는 마우스 조작이 필요해서 이번 실행에서는 돌지 않았다. 화면에
   무엇이 그려졌는지도 이번에는 확인하지 않았다.

---

## 5. 남은 권장 작업 (우선순위 순)

1. 🔴 **2.1:** untracked 30개 파일과 수정 중인 파일의 주인을 확인하고 커밋한다(clone 빌드 복구).
2. 🟠 **3.4:** 에디터 로그를 `logging`으로 통일한다. 파일 수가 적고 위험이 낮으며, 원인 추적에
   효과가 가장 크다.
3. 🟠 **2.4:** 렌더러와 `MotionPreviewState`를 분리한다(선분 데이터 인터페이스). 1번 이후에 한다.
4. 🟠 **2.3:** `Engine`을 `core/`에서 `runtime/`으로 옮기고 공개 헤더는 전방 선언으로 바꾼다.
5. 🟠 **3.5:** 에디터 파사드를 도입한다. Inspector와 Scene Hierarchy부터 시작한다.
6. 🟡 **3.6:** `test_demo_scene.py`의 rc=127을 조사하고 테스트 출력을 UTF-8로 고정한다.
