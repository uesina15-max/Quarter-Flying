# 인게임 카메라 기능 세분화 계획 (2026-09-30)

"뷰포트 카메라 전환이 안 된다"는 요청을 둘로 나눴다.

- **에디터에서 뷰포트를 둘러보는 것**은 UI 문제라 바로 구현했다(§0).
- **게임 안에서 카메라를 고르고, 전환하고, 따라가게 하는 것**은 엔진 기능이라 아래 단계로 쪼갠다.

각 단계의 "검증" 항목은 CLAUDE.md 관례 2번(컴파일 / 유닛 테스트 / 실제 실행)을 따른다.

---

## 0. 완료: 에디터 카메라 (커밋 `c748131`)

- 게임 카메라(ECS Main Camera를 따름)와 분리된 `editorCamera`를 추가했다. `Engine::SetViewCamera(Game|Editor)`로 전환한다.
- Scene 뷰포트 조작: 우클릭 드래그로 회전, 휠클릭 드래그로 이동, 휠로 줌, F로 리셋. 툴바에서 "게임 카메라로 보기"를 켜고 끌 수 있다.
- 검증: 실제 에디터에 마우스와 휠 이벤트를 보내 단계별로 화면을 캡처해 확인했다.

---

## 1. 현재 상태 (코드로 확인한 사실)

| 항목 | 현재 | 근거 |
|---|---|---|
| Play 모드 화면 | **씬이 보이지 않는다.** Play 버튼은 Scene 뷰포트 엔진의 World를 `Play()`하는데, 화면에 보이는 것은 씬이 없는 별도 Engine(Play 뷰포트)이다. | `main.py _on_play`: `_set_mode(1)` 후 `self._world.Play()` |
| 활성 카메라 결정 | `isMainCamera == true`인 **첫 번째** 엔티티. 여러 개면 경고 없이 첫 번째를 쓴다. | `RenderSystem.cpp` 카메라 sync 루프(`break`) |
| 카메라 투영 | `CameraComponent.fov/nearPlane/farPlane`을 **아무도 읽지 않는다.** 투영은 Engine이 60도, 0.1~1000으로 고정한다. Inspector에서 fov를 바꿔도 반영되지 않는다. | `Engine.cpp` `setProjection(glm::radians(60.0f), …)`, `RenderSystem.cpp:232` |
| 카메라 로직 위치 | 렌더 배치 System(`RenderSystem`) 안에 섞여 있다. | `RenderSystem::Update` 후반부 |
| 게임 입력 | 뷰포트가 마우스와 키 이벤트를 엔진에 보내지만, 엔진은 창 닫기와 리사이즈만 처리한다. | `Engine::TickFrame` 입력 루프 |
| 참조 필드 | 리플렉션이 `EntityRef`(UUID 직렬화)를 지원하고, PIE 스냅샷에서도 UUID가 보존된다. | `Reflection.h` `FieldType::EntityRef` |

---

## 2. 단계별 계획

### C0. Play 모드가 실제 게임 화면을 보여주게 하기 — ✅ 완료 `e6c2596` (실제 실행 캡처로 확인)

카메라 기능을 아무리 만들어도 Play 화면에 씬이 안 나오면 확인할 수 없다.

- **권장안:** Play 시 두 번째 Engine을 쓰지 않는다. Scene 뷰포트(씬 World를 가진 엔진)를 Play 페이지로 옮기고 `ViewCamera::Game`으로 전환한다. Stop하면 되돌린다. Motion Editor가 이미 쓰는 뷰포트 재배치(`_move_viewport_to_*`) 방식을 그대로 재사용한다.
- 부수 효과: 한 프로세스에 GL 컨텍스트가 둘이라 생기던 문제들(컨텍스트 전환, 전역 캐시, `g_PlatformInstance`)의 발생 조건 자체가 사라진다.
- 검증: 에디터에서 Play를 누르면 Main Camera 시점의 씬이 화면에 나오는지 캡처로 확인하고, Stop 후 에디터 카메라 시점으로 복귀하는지 확인한다.

### C1. 카메라 로직을 `CameraSystem`으로 분리 — ✅ 완료 (유닛 테스트 통과)

- `RenderSystem`의 카메라 sync 부분을 새 `CameraSystem`으로 옮긴다(책임 분리). 렌더 배치 버그와 카메라 버그를 로그와 테스트에서 구분할 수 있게 된다.
- 기존 `RenderSystemCameraSyncTest` 5개를 `CameraSystem` 대상으로 옮겨 그대로 통과시킨다.

### C2. `CameraComponent` 투영 반영 — ✅ 완료 (유닛 테스트 + 실제 실행: fov 60→25 변경이 화면에 반영됨을 캡처로 확인)

> 이 검증 중에 frustum 컬링 버그를 발견해 고쳤다. 화각이 좁아져 일부 인스턴스가 컬링되면, 보이는 인스턴스 대신 버퍼 앞쪽의 인스턴스가 그려졌다(`InstancedBatchManager::UpdateVisibility` 주석).

- 활성 카메라의 `fov / nearPlane / farPlane`을 게임 카메라 투영에 적용한다. 종횡비는 Engine이 뷰포트 크기로 제공한다(현재 `HandleWindowResize`가 관리하는 값).
- Engine에 고정된 60도는 "활성 카메라가 없을 때의 기본값"으로만 남긴다.
- 검증: 유닛 테스트(fov 변경 → 투영 행렬 변화), 실제 실행(Inspector에서 fov 변경 → 화면 화각 변화).

### C3. 활성 카메라 선택 규칙 명확화 — ✅ 완료 (유닛 테스트 + 실제 실행: priority 변경으로 게임 카메라 시점과 계층의 🎥 표시가 바뀜을 캡처로 확인)

- `isMainCamera` bool을 "먼저 나온 것이 이김"으로 두지 않는다. `priority`(int) 필드를 추가해 **가장 높은 우선순위의 활성 카메라**가 이기게 한다.
- 동점이거나 여러 개가 켜져 있으면 경고를 **한 번** 남긴다(관례 3번).
- API: `CameraSystem::GetActiveCamera()`, 바인딩 `Engine.GetActiveCameraEntity()`. Scene Hierarchy에서 활성 카메라를 표시한다.

### C4. 런타임 카메라 전환 + 블렌드 — ✅ 완료. 블렌드(실제 실행 캡처 확인, C++ 유닛 테스트 4개는 이후 빌드에서 실행되어 통과) + `EventType.CAMERA` 액션 이벤트(Python 유닛 테스트 + 실제 실행: Play 중 이벤트로 Cam B 전환, Stop 시 복원 확인). 이벤트 타입은 1개만 추가하고 근거를 `action_data.py`에 기록했다(사용자 결정 2026-09-30).

- 게임 로직에서 카메라를 바꾸는 방법: 액션 이벤트 `SwitchCamera(target: EntityRef, blendSeconds)`. 이미 있는 액션/이벤트 파이프라인(Sound Lite의 `ActionPlayerComponent`)에 이벤트 타입을 추가한다.
- 블렌드: 이전 카메라와 새 카메라의 위치를 보간하고, 회전은 쿼터니언 slerp, fov는 선형 보간한다. `blendSeconds = 0`이면 즉시 전환(cut)이다.
- 검증: 유닛 테스트(블렌드 중간값, cut), 실제 실행(액션 재생 중 카메라 전환이 화면에 보이는지).

### C5. 추적 카메라 (Follow / LookAt) — ✅ 완료 (유닛 테스트 + 실제 실행: Play 중 대상을 옮기면 카메라가 offset 위치로 따라가며 대상을 바라봄)

- `CameraFollowComponent { target: EntityRef, offset: Vec3, damping: float }`
- `CameraLookAtComponent { target: EntityRef }`
- EntityRef는 UUID로 직렬화되므로 프리팹과 PIE를 그대로 통과한다. 대상이 삭제되면 경고를 한 번 남기고 마지막 위치를 유지한다.
- 계층 컴포넌트(프리팹 Phase 5)가 들어오면 "부모에 붙은 카메라"로 대체할 수 있는지 그때 재검토한다.

### C6. 게임 입력으로 카메라 조작 (입력 시스템 선행) — ✅ 완료 (입력 시스템 `engine/input/InputState` + `CameraOrbitControlComponent`, 유닛 테스트 + 실제 실행: Play 중 우클릭 드래그로 궤도 회전, 휠 줌, 값이 계산과 일치)

- 선행: 엔진이 입력 이벤트를 게임 쪽에 전달하는 입력 시스템(현재 없음)이 필요하다.
- 그 다음: 3인칭 궤도 카메라 같은 플레이어 조작 카메라. 에디터용 `OrbitCamera`의 계산을 C++로 옮겨 재사용한다.

### C7. 에디터 편의 기능 (C3 이후 아무 때나)

> **2026-09-30 상태:** Align to View ✅, 카메라 기즈모 ✅(`CameraGizmoSystem` + `DebugLineBuffer`, 에디터 카메라로 볼 때만 표시, 게임 화면에는 0픽셀), "선택 카메라 시점으로 보기" ✅(Align의 반대). 모두 실제 실행 캡처로 확인했다.
> **카메라 프리뷰(PIP)는 보류.** 오프스크린 렌더 타깃(FBO)과 두 번째 렌더 패스가 필요한데, 현재 렌더러에는 그 경로가 없다(RenderGraph 패스가 비어 있고 씬 렌더러들이 기본 프레임버퍼에 직접 그린다). 그 대신 "선택 카메라 시점으로 보기"로 확인하는 흐름을 제공한다.
> 알려진 외관 문제: 카메라 시점으로 보면 그 카메라 자신의 기즈모 선이 화면 가장자리에 걸쳐 보인다.

- **Align to View:** 에디터 카메라 시점을 선택한 카메라 엔티티의 Transform으로 복사한다.
- **카메라 프리뷰:** 선택한 카메라 시점을 작은 창(PIP)으로 보여준다.
- **카메라 기즈모:** Scene 뷰에 카메라 위치와 시야(frustum)를 선으로 그린다(`BoneLineRenderer` 같은 라인 렌더러 재사용).

---

## 3. 권장 순서

```
C0 (Play 화면) → C1 (CameraSystem 분리) → C2 (투영) → C3 (활성 카메라 규칙)
   → C4 (전환/블렌드) → C5 (추적)          ─┐
   C7 (에디터 편의)는 C3 이후 병행 가능      ├→ C6 (입력 시스템 후 플레이어 카메라)
```

C0~C3는 각각 작고 독립적이다. C4 이후는 액션 이벤트, EntityRef, 입력 시스템 같은 다른 기능과 맞물린다.
