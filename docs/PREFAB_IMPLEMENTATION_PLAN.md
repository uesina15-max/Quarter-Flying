# 프리팹 시스템 구현 계획서

**작성일**: 2026-08-19
**목표**: 자주 반복되는 엔티티 구성(예: "Barrel", "Enemy_Grunt")을 디스크에 `.prefab.json`으로 저장해두고, 에디터에서 몇 번이든 다시 인스턴스화할 수 있게 한다. [docs/MASTER_PLAN.md](MASTER_PLAN.md) §6의 "지금 바로 만들 5개" 중 2번(§6.2), 우선순위 매트릭스의 P0-b(런타임 인스턴스)/P1·P2(에디터 저작·베리언트)에 해당.

> **선행 확정 사항**: 이 계획서는 저장소를 직접 grep/read해서 확인한 사실 위에서만 설계를 내린다 — 아래 §1은 전부 실측이다. 특히 "프리팹" 관련 기존 코드는 [engine/asset/AssetHandle.h:32](../engine/asset/AssetHandle.h#L32)의 `AssetType::Prefab` enum 값 하나뿐이고 이를 참조하는 코드가 전혀 없다는 것부터 시작한다.

> **개정(2026-08-19, 리뷰 반영)**: 초안(§2.3/§2.4)이 API 경계와 실패 처리에 대해 산문으로만 설명하고 정확한 함수 시그니처·엣지 케이스를 비워뒀다는 지적을 받아 아래를 구체화했다 — (1) `SpawnInto`/`Revert`가 같은 적용 로직을 쓰도록 `ApplyToEntity()` 공통 Primitive로 통합(§2.4), (2) Revert가 "프리팹에 없는 컴포넌트"를 어떻게 다루는지 미정의였던 것을 확정(§2.5, Definition A: 제거), (3) Capture 시 `PrefabInstanceComponent` 자체가 재귀적으로 직렬화되는 구멍을 막고 `EntityRef` 필드를 가진 컴포넌트는 v1에서 거부(§2.6), (4) `Instantiate` 도중 컴포넌트 적용이 실패했을 때 반쪽짜리 엔티티가 레지스트리에 남는 경로를 막음(§2.7), (5) 프리팹 기본 위치와 `Instantiate(path, position)` 인자의 우선순위를 명시(§2.8), (6) 원본 파일 삭제 후 Revert 시 동작을 명시(§2.9), (7) `PrefabInstanceComponent::sourceVersion` → `sourcePrefabVersion`으로 개명해 파일 포맷 버전과 혼동되지 않게 함(§2.3). Phase 1/2/4의 API 시그니처와 테스트 목록도 이에 맞춰 갱신했다.

| 리뷰 항목 | 반영 위치 |
|---|---|
| ① `ApplyToEntity()` 공통 Primitive | §2.4 |
| ② Revert 정책 확정(Definition A) | §2.5 |
| ③ Capture 시 메타 컴포넌트 제외 | §2.6 |
| ④ `sourceVersion` → `sourcePrefabVersion` | §2.3 |
| ⑤ Instantiate 원자성(롤백) | §2.7 |
| ⑥ Position 적용 우선순위 | §2.8 |
| ⑦ `EntityRef` 필드 거부 | §2.6 |
| ⑧ 원본 파일 삭제 후 동작 | §2.9 |

> **개정(v2.0, 계약서 병합)**: 코드베이스 접근이 없는 세션에서 작성된 "Phase 1 확정 계약서 v2.0"을 받아, 그 문서가 "가정"으로 표시해둔 4가지를 이 저장소 기준으로 재확인하고 나머지 결정을 병합했다.
>
> **§0 가정 재확인 결과**: (1) `SerializeRegistry()` 시그니처 — 가정대로 맞음(`Reflection.cpp:51`). (2) `GE_BEGIN_COMPONENT` 등 매크로 3종 — 가정대로 `Reflection.h`에 있음(등록 자체는 `Components.h`가 아니라 `Reflection.cpp`에서 일어남 — "컴포넌트가 모여 있고 그 자리에서 등록"이라는 가정과 미세하게 다름, 파일이 분리되어 있음). (3) `Components.h` include 구조 — 대체로 맞음. (4) **`EngineError` 정의 위치는 가정(`engine/core/Error.h`)과 다르다 — 실제로는 `engine/core/EngineError.h`이고, `EngineErrorCode` enum + `EngineError` struct + `MakeError()` 헬퍼가 그 안에 있다(`EditorAPI.h`가 이미 이 경로로 include하고 `EditorAPI.cpp`가 `MakeError`를 실제로 쓰고 있음, 직접 확인).** 아래 모든 시그니처는 `engine/core/EngineError.h` 기준으로 고쳐 반영한다. (5) `EntityRef` 필드 타입 — 가정대로 `Reflection.h`에 있음.
>
> 병합된 새 결정: `ApplyToEntity`의 **strong guarantee**(성공→prefab과 완전 동일, 실패→호출 전 상태로 스냅샷 복원, §2.4), `SpawnInto` 원자성과의 **별개 계약** 명시(§2.4/§2.7), `SerializeOptions` 구조체로 캡처 정책을 `SerializeRegistry()`의 기존 동작과 분리(§2.6, 회귀 게이트 포함), `TransformComponent`를 `PrefabInstanceComponent`와 함께 **제거 예외 2종**으로 확정(§2.5), 컴포넌트 정책 6행 표(§2.5), ECS 컴포넌트 추가/제거 도중의 스레드/타이밍 가드(§2.10, 이 저장소에서 직접 확인한 `TypedComponentArray::Remove()`의 swap-and-pop 구현에 근거), `sourcePrefabVersion`이 "파일 포맷 버전 스냅샷"일 뿐 revision 추적 용도가 아니라는 정정(§2.3), `InstantiatePrefab`의 `std::optional<glm::vec3>` 시그니처(§2.8), `meshHandle` 영속성 한계 고지(§2.11).

> **개정(모듈 경계 정정, 구현 착수 직전)**: 구현 착수 직전 검토에서 의존 방향 원칙이 명시적으로 나왔다 — **`engine/ecs/`(Reflection 포함)는 `engine/prefab/`을 몰라야 한다.** 이전 개정에서 "`SerializeEntityComponents`를 `SerializeRegistry()`와 같은 파일(`Reflection.h`)에 헬퍼로 추가해서 공유"라고 적었던 부분이 이 원칙과 충돌한다는 게 드러났다 — `SerializeOptions`는 `prefab/`에 두면서 그 타입을 받는 함수를 `ecs/Reflection.h`에 선언하면, `Reflection.h`가 `prefab/SerializeOptions.h`를 include해야 해서 정확히 금지된 방향(`Reflection → Prefab`)이 생긴다. **정정**: `SerializeOptions`와 `SerializeEntityComponents`는 둘 다 `engine/prefab/`에 둔다(§2.6 재작성). `SerializeRegistry()`(`Reflection.cpp`)는 전혀 건드리지 않고 기존 인라인 루프 그대로 둔다 — `PrefabAsset`의 캡처 루틴은 `ComponentRegistry::GetAllComponents()`(이미 public)를 직접 순회하는 별도 구현으로, 짧은 for-loop 하나가 두 곳에 생기는 건 감수한다(SRP 기준으로 "왜 바뀌는가"의 이유 자체가 다르다 — `SerializeRegistry`는 PIE 스냅샷 사유로, 프리팹 캡처는 프리팹 저작 사유로 바뀐다). 이 덕에 §3의 "회귀 게이트"도 훨씬 단순해진다 — `Reflection.cpp`를 아예 안 건드리므로 리팩터 리스크 자체가 없다(신규 컴포넌트 타입 추가로 인한 *추가적* 출력 변화만 있고, 기존 동작 변경은 없음). `PrefabInstanceComponent`의 리플렉션 등록만은 여전히 `Reflection.cpp`에 둔다(`PrefabAsset`이 자기 컴포넌트 등록까지 책임지지 않도록 — 다른 5종 컴포넌트와 같은 자리에 모아두는 기존 관례를 그대로 따름). 아울러 `ApplyToEntity`의 내부 구현을 `CaptureSnapshot`/`SynchronizeComponents`(`RemoveMissingComponents`+`DeserializePrefabComponents`)/`RestoreSnapshotOnFailure` 5개 private 헬퍼로 명시적으로 쪼갠다(§2.4) — 외부 시그니처는 그대로.

---

## 1. 배경 — 지금 있는 것과 없는 것

**있는 것 (재사용 가능)**

- **컴포넌트 리플렉션 시스템**([engine/ecs/Reflection.h/.cpp](../engine/ecs/Reflection.h)) — `GE_BEGIN_COMPONENT`/`GE_FIELD`/`GE_END_COMPONENT` 매크로로 각 컴포넌트 타입마다 `serialize`/`deserialize`/`patchField`/`remove`/`hasComponent` 함수 포인터가 이미 등록되어 있다(`TransformComponent`/`RenderableComponent`/`CameraComponent`/`ScriptComponent`/`AIComponent` 5종). `SerializeRegistry(ECSRegistry&)`가 바로 이 함수들을 순회해서 **월드 전체**를 JSON으로 덤프하는 데 쓰고 있다(`World::Play()`의 PIE 스냅샷 용도, [World.cpp:97-125](../engine/ecs/World.cpp)). 프리팹은 이 메커니즘을 "월드 전체"가 아니라 "엔티티 1개"로 좁혀 쓰면 된다 — 새로 발명할 게 거의 없다.
- **`FieldType::EntityRef`** — 필드가 다른 엔티티를 UUID로 참조하도록 이미 매크로 레벨에서 지원된다(`Reflection.h:144-148`). 지금은 이 타입을 쓰는 컴포넌트가 하나도 없지만, 나중에 계층(부모-자식) 컴포넌트를 추가할 때 바로 쓸 수 있다.
- **Undo/Redo 트랜잭션 계층** — `EditorAPI`([engine/editor/EditorAPI.h/.cpp](../engine/editor/EditorAPI.h))가 `CreateEntity`/`AddComponent`/`MoveEntity` 등을 전부 `ICommand`로 감싸 `CommandManager`에 넘기고, `BeginTransaction`/`CommitTransaction`으로 여러 `ICommand`를 하나의 Undo 단위(`Transaction : ICommand`, [engine/core/Transaction.h](../engine/core/Transaction.h))로 묶을 수 있다. `Dispatch()`가 "트랜잭션 열려있으면 즉시 Apply 후 누적, 없으면 CommandManager로 바로 실행"을 이미 처리한다([EditorAPI.cpp:43-60](../engine/editor/EditorAPI.cpp)). pybind11로 `editor_api`라는 이름의 싱글턴으로 Python에 노출되어 있다([engine/bindings/EditorBindings.cpp](../engine/bindings/EditorBindings.cpp)). `engine/editor/commands/CreateEntityCommand.h`가 새 `ICommand`를 만들 때 따라야 할 정확한 템플릿이다.
- **`RenderSystem` + `InstancedBatchManager`**(2026-08-18 완성, [ROADMAP.md](../ROADMAP.md) §6) — `TransformComponent`+`RenderableComponent`를 가진 엔티티는 이미 실제로 화면에 그려진다. 즉 프리팹을 인스턴스화한 엔티티가 "진짜로 보이는지" 검증하는 렌더링 경로는 이미 완성돼 있다 — Motion Mixer Phase 4처럼 렌더러부터 새로 만들 필요가 없다.
- **파일 스캔 기반 에셋 탐색 선례** — `engine/asset/`(아래 참고)을 쓰지 않고, `panels/motion_mixer.py`가 `assets/motion/*.skeleton.json`/`*.clip.json`을 직접 `glob`으로 스캔하는 방식으로 이미 한 번 검증됨. 프리팹도 같은 패턴(`assets/prefabs/*.prefab.json`)을 따른다.

**없는 것 (전부 신규 또는 확인된 죽은 코드)**

- **엔티티 계층(부모-자식) 개념이 ECS 어디에도 없다.** [engine/ecs/Entity.h](../engine/ecs/Entity.h)의 `EntityMeta`는 `uuid`+`runtime_id`뿐이고, `Components.h`에는 Hierarchy/Parent 컴포넌트가 없다. 실무 프리팹(예: 메시+콜라이더+이펙트 자식들을 묶은 캐릭터)은 보통 여러 엔티티의 트리인데, 이 트리를 표현할 자리 자체가 지금 엔진에 없다. **이 계획서의 v1은 이 제약을 정면으로 받아들여 "엔티티 1개짜리 프리팹"으로 시작한다** — §2.1에서 근거를 설명한다.
- **`engine/asset/`(`AssetManager.cpp`/`AssetRegistry.cpp`/`AssetHandle.h`)는 CMake에는 들어있지만(컴파일은 됨) 저장소 전체에서 이 파일들을 `#include`하는 다른 `.cpp`가 하나도 없고, `engine/bindings/`의 어떤 파일도 `AssetManager`/`AssetRegistry`/`AssetHandle`을 바인딩하지 않는다(grep 확인).** 즉 "에셋 파이프라인"은 코드가 존재할 뿐 실제로는 아무 것도 하지 않는 고아 모듈이다. **프리팹은 이 모듈 위에 짓지 않는다** — MASTER_PLAN.md §2의 P1 "에셋 import/reimport + 참조 관리"가 실제로 착수될 때 이 모듈을 살릴지 다시 짤지 판단할 문제이고, 지금 프리팹이 그 판단을 대신 내려버리면 안 된다.
- **`scene.json`이 ECS에 전혀 연결되어 있지 않다.** `engine/editor/demo_scene_integration.py::_load_objects()`는 `scene.json`의 오브젝트를 순수 Python dict로만 저장하고(`self.scene_objects`), `ge_python`/ECS API를 호출해 엔티티를 만드는 코드가 없다(grep 확인 — `CreateEntity` 호출이 이 파일에 없음). 따라서 **"프리팹 인스턴스를 씬에 배치하고 저장했다가 다시 열었을 때 살아있는지"는 이 계획서의 범위 밖이다** — 그건 프리팹 문제가 아니라 MASTER_PLAN.md P0-b의 "씬 라이프사이클(`LoadScene`)"이 아직 없다는, 더 큰 선행 과제다. v1은 **에디터를 켜둔 세션 안에서 인스턴스화·수정·되돌리기**까지만 검증하고, 파일로 저장된 씬에 프리팹 인스턴스가 영속되는 것은 씬 라이프사이클 작업이 끝난 뒤 자연히 따라오는 것으로 명시적으로 미룬다.
- **오버라이드 추적(인스턴스가 원본에서 벗어난 필드를 표시/되돌리기)** — Unity/Unreal류 엔진의 필드 단위 diff 하이라이트에 해당하는 개념이 전혀 없다. `Inspector`가 필드 단위로 값을 patch하는 경로(`SetComponentFieldJson` → `patchField`)는 있지만 "이 필드가 프리팹 기본값과 다르다"는 상태를 어디에도 기록하지 않는다.

---

## 2. 설계 결정

### 2.1 v1 스코프: 엔티티 1개짜리 프리팹

계층 컴포넌트가 없는 상태에서 여러 엔티티를 묶은 "진짜" 프리팹을 만들면, 계층 자료구조 설계 + 인스턴스 그룹 선택/이동/삭제 UX를 전부 이번 한 번에 떠안게 된다. Motion Mixer가 "정확한 리그 매칭만 1차 구현, 부분 매칭은 확장 과제"로 범위를 좁혔던 것과 같은 이유로, v1은 **엔티티 하나 = 프리팹 하나**로 좁힌다(Transform + Renderable, 필요하면 Script/AI까지 포함하는 "장식 없는 소품/적 하나" 수준). 여러 엔티티 트리는 §6(확장 과제)에서 계층 컴포넌트 도입을 전제로 다시 다룬다.

### 2.2 파일 포맷: `*.prefab.json`

`ComponentRegistry`의 기존 `serialize`/`deserialize` 함수를 엔티티 1개에 대해서만 호출한 결과를 그대로 담는다 — `SerializeRegistry()`가 만드는 `entityJson["components"]`와 완전히 같은 모양이다.

```json
{
  "version": 1,
  "name": "Barrel",
  "components": {
    "TransformComponent": {
      "_version": 1,
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "scale": [1, 1, 1]
    },
    "RenderableComponent": {
      "_version": 1,
      "meshHandle": 0,
      "materialHandle": 0,
      "castShadows": true
    }
  }
}
```

- `uuid`는 담지 않는다 — 프리팹은 "설계도"이고 인스턴스화할 때마다 새 UUID를 받아야 하므로, 원본 파일에 UUID가 있으면 오히려 실수로 재사용될 위험만 생긴다.
- 각 컴포넌트 블록의 `_version`은 `ComponentRegistry::GetComponentInfo(name)->version`을 그대로 쓴다(기존 `GE_BEGIN_COMPONENT` 매크로가 이미 채워주는 필드) — 스키마가 바뀌면 로드 시점에 버전 불일치를 감지할 수 있다(`SetComponentFieldJson`이 이미 같은 방식으로 버전 체크를 한다, [ECSBindings.cpp:198-213](../engine/bindings/ECSBindings.cpp)).
- 최상위 `"version"`은 프리팹 **파일 포맷 자체**의 버전이다(컴포넌트 스키마 버전과는 별개 축). v2에서 다중 엔티티를 지원하게 되면 `"entities": [...]` 배열로 바뀔 텐데, 그때 `"version": 2`로 분기해서 구버전 파일도 계속 읽을 수 있게 한다 — `ClipJsonIO`/`SkeletonJsonIO`가 이미 같은 패턴(최상위 `version` 필드)을 쓰고 있어 낯선 결정이 아니다.

### 2.3 인스턴스 식별: `PrefabInstanceComponent` (신규)

인스턴스화된 엔티티가 "어느 프리팹에서 왔는지"를 알아야 Inspector에서 되돌리기(Revert)를 할 수 있다. 새 POD 컴포넌트를 기존 5종과 같은 방식(`GE_BEGIN_COMPONENT` 매크로)으로 추가한다:

```cpp
struct PrefabInstanceComponent
{
    std::string prefabPath;                  // "assets/prefabs/Barrel.prefab.json" (인스턴스화 시점 경로)
    uint32_t    sourcePrefabVersion = 1;     // 인스턴스화 시점 PrefabAsset::version 스냅샷
};
```

- `prefabPath` 존재 여부 자체가 "이 엔티티는 프리팹 인스턴스다"의 표시이자, Inspector가 "Prefab: Barrel [Revert]" 헤더를 그릴지 판단하는 조건이 된다.
- **이름을 `sourceVersion`이 아니라 `sourcePrefabVersion`으로 정한 이유**: `PrefabAsset::version`(§2.2 JSON의 최상위 `"version"`)이 이미 "파일 포맷 버전"이라는 이름으로 쓰이는데, 컴포넌트 필드를 `sourceVersion`이라고만 부르면 "파일 포맷 버전"과 "이 인스턴스가 스폰된 시점의 원본 버전"이 헷갈린다. `sourcePrefabVersion`은 후자만 가리킨다.
- **의미 확정(Option A)**: `sourcePrefabVersion`은 **파일 포맷 버전의 스냅샷일 뿐**이다 — v1의 모든 프리팹 인스턴스는 이 값이 항상 `1`이므로, "이 인스턴스가 프리팹 파일의 어느 개정(revision)에서 나왔는지"를 구분하는 용도로는 **쓸 수 없다**(v1에서는 파일 포맷 자체가 안 바뀌므로 값이 전부 같다). v1에서 Revert 자체는 이 값을 게이트로 쓰지 않는다(무조건 다시 읽어서 적용, §2.5) — 지금은 순수 진단 정보(Inspector에 "v1 포맷으로 생성됨" 표시)일 뿐이다. 콘텐츠 개정을 실제로 추적하고 싶다면(Phase 5의 다중 인스턴스 Apply 역전파에서 "어느 버전 기준으로 갈라졌는지" 판단하려면) `sourcePrefabVersion`이 아니라 **별도 필드(`sourceRevision`, 예: 컴포넌트 데이터 해시 또는 증가 카운터)가 필요**하다 — 이건 Option B로 완전히 다른 필드이고, 베리언트 시스템 자체가 아직 설계되지 않은 v1 시점에 미리 만들면 "베리언트를 상상하다가 베리언트를 구현하게" 되므로 명시적으로 Phase 5로 미룬다(§5).

### 2.4 `ApplyToEntity()` — Spawn과 Revert가 공유하는 단일 Primitive

처음 설계에서는 "스폰할 때 deserialize 호출"과 "Revert할 때 deserialize 재호출"을 각자 따로 구현할 뻔했는데, 그러면 두 경로가 슬금슬금 달라질 위험이 있다(예: 한쪽만 컴포넌트 제거 로직이 있고 다른 쪽은 없어지는 식). **하나로 합친다**: `SpawnInto`는 "빈 엔티티를 만들고 `ApplyToEntity`를 호출"하는 것 이상을 하지 않고, Revert(§2.5, Phase 4)도 기존 엔티티에 같은 `ApplyToEntity`를 호출하는 것으로 끝낸다.

```
       [ PrefabAsset ]
              │
     ┌────────┴────────┐
     ▼                 ▼
SpawnInto()       (Phase 4) Revert
     │                 │
CreateEntity      기존 엔티티 그대로
     │                 │
     └───────┬─────────┘
             ▼
     ApplyToEntity(registry, entity)
   (§2.5 Definition A대로 컴포넌트
    제거/deserialize 수행)
```

`ApplyToEntity`가 유일하게 컴포넌트를 추가/제거/덮어쓰는 지점이므로, "Spawn 직후 상태"와 "Revert 직후 상태"는 정의상 항상 똑같다 — 둘 다 같은 함수를 통과했기 때문이다.

**Strong guarantee 계약(한 문장)**: `ApplyToEntity`는 **성공하면 엔티티가 프리팹과 완전히 동일한 컴포넌트 집합/값이 되고, 실패하면 호출 전 상태가 정확히 그대로 유지된다.** 이게 왜 필요한가 하면, `SpawnInto`는 실패 시 방금 만든 엔티티를 통째로 `DestroyEntity`하면 그만이지만(§2.7), **Revert(Phase 4)는 이미 씬에 존재하던 엔티티를 대상으로 하므로 실패했다고 파괴할 수 없다** — 실패 후에도 "Revert 누르기 전 상태"가 멀쩡히 남아있어야 한다.

구현 방식: 외부에 노출되는 건 `ApplyToEntity(registry, entity)` 하나뿐이지만, 내부는 이름이 곧 역할을 설명하는 5개 private 헬퍼로 나눈다 — 이렇게 나눠두면 `ApplyToEntity` 본문 자체가 "strong guarantee를 가진 프리팹 적용"이라는 의미를 코드 그대로 읽을 수 있게 된다:

```
ApplyToEntity(registry, entity)
 ├─ CaptureSnapshot(registry, entity) -> json
 │     SerializeEntityComponents(registry, entity, {})  // 기본 옵션 — 메타 컴포넌트 포함 전체 상태
 ├─ SynchronizeComponents(registry, entity, componentsData) -> Result<void>
 │   ├─ RemoveMissingComponents(registry, entity, componentsData)
 │   │     prefab에 없는 컴포넌트 제거 (제거 예외 2종 제외, §2.5 1번)
 │   └─ DeserializePrefabComponents(registry, entity, componentsData) -> Result<void>
 │         prefab에 있는 컴포넌트 deserialize, try/catch로 실패 포착 (§2.5 2번)
 └─ 실패 시: RestoreSnapshotOnFailure(registry, entity, snapshot)
       내부적으로 SynchronizeComponents(registry, entity, snapshot)를 재사용 —
       "복원"도 "적용"과 같은 동기화 로직이므로 별도 복구 코드를 새로 쓰지 않는다
```

`RemoveMissingComponents`/`DeserializePrefabComponents`는 `SynchronizeComponents`가 대상 JSON(프리팹의 `componentsData`이든, 실패 복구용 `snapshot`이든)에 엔티티를 맞추는 데 쓰는 두 단계일 뿐이라 — 프리팹 JSON을 적용할 때도, snapshot으로 되돌릴 때도 완전히 같은 코드 경로를 탄다. `RestoreSnapshotOnFailure`의 복구 호출 자체가 실패할 가능성은 이론상 배제할 수 없지만(직전에 유효했던 상태를 그대로 재생하는 것이므로 실무적으로는 거의 항상 성공), 타입 시스템이 보장하는 건 아니라는 점은 인지하고 있어야 한다 — Phase 1 구현 시 이 복구 경로 자체의 실패를 어떻게 다룰지(예: 크래시 대신 로그+상태 오염 감수) 결정해야 한다.

**Command 층의 Undo와 같은 메커니즘**: Phase 4의 `RevertPrefabInstanceCommand::Undo()`도 `ApplyToEntity` 호출 직전에 같은 방식으로 스냅샷을 잡아뒀다가 그 스냅샷으로 되돌리는 것으로 구현한다 — "복원" 로직(스냅샷 JSON → 엔티티 재적용)을 한 곳에 모아 Command의 Undo와 `ApplyToEntity`의 내부 실패-복구가 같은 코드를 쓰게 한다.

### 2.5 Revert/Apply 정책: Definition A — 프리팹과 완전히 동일한 컴포넌트 집합으로 동기화

필드 단위로 "이 값이 프리팹 기본값과 다르다"를 표시하는 diff 하이라이트는 v1 범위 밖으로 유지한다(인스턴스마다 "최초 스폰 시 값"을 따로 저장할 공간이 지금 없다). 대신 **`ApplyToEntity`가 하는 일 자체를 명확히 정의한다** — "컴포넌트 값만 덮어쓰기"가 아니라 **엔티티의 컴포넌트 집합을 프리팹 JSON과 완전히 일치시킨다**(Definition A):

1. `ComponentRegistry::GetAllComponents()`를 순회하며, 엔티티가 현재 갖고 있는(`info.hasComponent(reg, entity)`) 컴포넌트 중 프리팹 JSON의 `components`에 **없는** 것을 찾는다. **제거 예외 2종**은 건드리지 않는다 — `PrefabInstanceComponent`(인스턴스 식별 자체를 지워버리면 안 되므로)와 **`TransformComponent`**(엔티티는 항상 위치를 가져야 한다 — v1의 모든 캡처 대상 엔티티가 Transform을 갖는 게 사실상 보장돼 있긴 하지만, 손으로 편집했거나 손상된 `.prefab.json`이 `TransformComponent` 블록을 빠뜨렸다고 해서 살아있는 씬 엔티티에서 위치 정보 자체를 뽑아버리면 `RenderSystem`을 포함한 Transform을 전제하는 코드가 전부 위험해진다 — 방어적으로 제외). 나머지는 `info.remove(reg, entity)`로 제거한다.
2. 프리팹 JSON의 `components`에 있는 각 항목에 대해 `info.deserialize(reg, entity, data)`를 호출한다(없던 컴포넌트면 매크로가 이미 하듯 기본값으로 추가 후 덮어씀, 있던 컴포넌트면 필드 값만 갱신). **이 호출은 `try/catch`로 감싼다** — `GE_BEGIN_COMPONENT` 매크로가 생성하는 `deserialize` 람다는 필드가 아예 없으면 조용히 건너뛰지만(`data.contains(f.name)` 가드), 필드가 있는데 타입이 안 맞으면(`data[f.name].get<float>()`에 문자열이 들어있는 식) `nlohmann::json`이 `json::type_error` 예외를 던진다 — 이 예외를 여기서 잡아 `EngineError`로 변환하는 것이 "알려진 컴포넌트 + 잘못된 JSON → 실패"(아래 표)를 실제로 동작하게 만드는 지점이다.

**컴포넌트 정책 표(확정)**:

| 상황 | 정책 |
|---|---|
| 알 수 없는(등록 안 된) 컴포넌트 이름 | 무시(forward compatibility) |
| 알려진 컴포넌트 + 정상 JSON | 적용 |
| 알려진 컴포넌트 + 잘못된 JSON(타입 불일치 등) | 실패(위 `try/catch`가 `EngineError`로 변환) |
| 프리팹 최상위 JSON 스키마 자체가 잘못됨 | 실패(`LoadFromFile`/`FromJson` 단계) |
| `EntityRef` 필드를 가진 컴포넌트 캡처 시도 | 실패(§2.6) |
| `PrefabInstanceComponent`/`TransformComponent` 캡처·제거 | 캡처 시 제외(§2.6) / 제거 시 예외(위 1번) |

"무시"와 "실패"를 분리해두는 이유: forward compatibility(구버전 에디터가 신버전 프리팹의 낯선 컴포넌트를 만나도 안 죽어야 함)와 데이터 손상 감지(알고 있는 컴포넌트인데 값이 이상하면 조용히 넘어가지 않고 알려야 함)는 서로 다른 목적이라 같은 처리로 뭉뚱그리면 안 된다.

이렇게 하면 "인스턴스화 이후 에디터에서 프리팹에 없던 컴포넌트를 새로 붙였다가 Revert를 누르면 그 컴포넌트는 사라진다"가 명확한 규칙이 된다 — 애매하게 "덮어쓰기만 하고 남겨둔다"는 절반짜리 되돌리기보다 예측 가능하다. "Apply"(인스턴스 값을 원본 파일로 역전파해서 다른 인스턴스에도 퍼뜨리는 것)는 v1에서 아예 만들지 않는다 — 버튼은 두되 비활성화 + 툴팁으로 "다음 단계"라고 명시한다. 이건 이 저장소의 기존 관례이기도 하다(`motion_editor.py`의 `do_import_fbx()`가 메시지박스만 띄우는 자리표시자로 남아있는 것과 같은 방식 — 조용히 없는 것보다 "여기 있고, 아직 안 됨"이 명확한 쪽을 택한다).

### 2.6 Capture 시 제외 규칙: `SerializeOptions`로 정책을 분리

`CaptureFromEntity`가 `ComponentRegistry::GetAllComponents()`를 그대로 순회하면 두 가지 구멍이 생긴다 — **자기 참조 오염**(캡처 대상이 이미 다른 프리팹의 인스턴스라면 `PrefabInstanceComponent`까지 그대로 직렬화되어 "프리팹 A 안에 프리팹 B를 가리키는 `prefabPath`가 박혀있는" 상황이 됨)과 **유실되는 참조**(§1의 `FieldType::EntityRef` — 어떤 컴포넌트든 가질 수 있는 필드 타입인데, 그 UUID는 프리팹을 다른 씬/다른 시점에 인스턴스화하는 순간 의미를 잃는다).

이 두 규칙을 `SerializeEntityComponents` 헬퍼(Phase 1, §3) 안에 하드코딩하면, **같은 헬퍼를 쓰는 기존 `SerializeRegistry()`(월드 전체 직렬화, PIE 스냅샷 용도)의 의미까지 조용히 바뀌어버린다** — PIE 스냅샷은 원래 `PrefabInstanceComponent`를 포함한 전체 상태를 그대로 보존해야 하므로, 프리팹 캡처를 위해 만든 제외 규칙이 여기 새어 들어가면 안 된다. 그래서 **정책을 옵션 구조체로 분리**한다:

```cpp
// engine/prefab/SerializeOptions.h
struct SerializeOptions
{
    bool excludePrefabMetadata = false;   // PrefabInstanceComponent 등 메타 컴포넌트 제외
    bool rejectEntityRefs      = false;   // EntityRef 필드 발견 시 실패
};

// engine/ecs/Reflection.h (SerializeRegistry와 같은 파일에 헬퍼로 추가)
std::expected<nlohmann::json, EngineError>
SerializeEntityComponents(ECSRegistry& registry, Entity entity, SerializeOptions options = {});
```

| 호출자 | 옵션 |
|---|---|
| `SerializeRegistry()`(기존 월드 직렬화, PIE 스냅샷) | 기본값 `{}` — 동작 불변 |
| `PrefabAsset::CaptureFromEntity()` | `{excludePrefabMetadata=true, rejectEntityRefs=true}` |
| `ApplyToEntity`의 내부 스냅샷(§2.4, Undo/실패 복구용) | 기본값 `{}` — 메타 컴포넌트 포함 전체 상태 보존 |

**회귀 게이트(Phase 1 완료 기준에 포함, 하드 조건)**: 이 옵션 분리 이후에도 `SerializeRegistry()`의 출력은 리팩터 이전과 **완전히 동일**해야 한다. 프리팹 작업 때문에 PIE 스냅샷(Play 버튼)의 직렬화 의미가 바뀌면 "프리팹을 건드렸는데 Play 모드 상태 복원이 이상해졌다"는, 원인 추적이 매우 어려운 회귀가 된다 — 이 게이트를 Phase 1 완료 기준(§3)의 필수 항목으로 못박는다.

`rejectEntityRefs=true`일 때: 캡처 대상 컴포넌트의 `ComponentInfo::fields`에 `FieldType::EntityRef`가 있고 그 필드가 유효한 엔티티를 가리키고 있으면 `CaptureFromEntity` 자체가 실패를 반환한다(조용히 끊어진 참조를 저장하는 대신). 지금 등록된 5개 컴포넌트 중 `EntityRef` 필드를 쓰는 게 하나도 없어서 당장은 이론적인 케이스지만, 나중에 추가되는 컴포넌트가 이 규칙을 우회하지 못하도록 지금 정해둔다. `excludePrefabMetadata=true`일 때는 컴포넌트 이름이 `"PrefabInstanceComponent"`인 것을 하드코딩으로 건너뛴다(지금 이 컴포넌트가 유일한 "에디터/인스턴스 메타" 컴포넌트이므로 범용 `IsEditorOnly` 플래그 체계를 지금 만들 필요는 없다 — 나중에 비슷한 컴포넌트가 늘어나면 그때 일반화한다).

### 2.7 Instantiate 원자성: 실패하면 아무것도 남지 않는다

`ApplyToEntity`가 컴포넌트를 여러 개 순서대로 적용하는 도중 하나가 실패할 수 있다(JSON 파싱 자체는 `LoadFromFile` 단계에서 이미 걸러지지만, 예를 들어 `deserialize` 람다가 기대한 필드가 없어 예외를 던지는 경우). 이때 "일부 컴포넌트만 붙은 고아 엔티티"가 레지스트리에 남으면 안 된다. **원자성은 가장 낮은 층(`PrefabAsset::SpawnInto`)에서 보장한다** — 호출자(Command 층)가 매번 롤백을 신경 쓰지 않도록. `SpawnInto` 내부에서 `ApplyToEntity`가 실패를 반환하면, 방금 만든 엔티티를 `registry.DestroyEntity()`로 즉시 제거하고 실패를 전파한다. 즉 `SpawnInto`는 "완전히 성공" 아니면 "레지스트리에 흔적을 남기지 않고 실패" 둘 중 하나만 가능하다.

### 2.8 위치 적용 순서: 프리팹 기본값 → `Instantiate(path, position)` 인자가 최종 승자

프리팹 JSON의 `TransformComponent`에는 캡처 시점의 위치(보통 원점 근처)가 그대로 들어있다. 배치 위치를 인자로 받는 `EditorAPI::InstantiatePrefab`은 다음 시그니처로 확정한다:

```cpp
std::expected<Entity, EngineError>
InstantiatePrefab(const std::filesystem::path& path,
                   std::optional<glm::vec3> position = std::nullopt);
```

`position`을 `Vec3`가 아니라 `std::optional<glm::vec3>`로 받는 이유: "값을 안 줬다"와 "원점(0,0,0)을 명시적으로 줬다"를 시그니처만으로 구분할 수 있어야 한다(매직 넘버 센티널을 안 쓴다). 적용 순서:

1. `SpawnInto` → `ApplyToEntity`가 프리팹 JSON의 `TransformComponent`를 그대로 적용(위치 포함).
2. `PrefabInstanceComponent` 부착(§2.3) — 값 자체와는 무관한, 순서상의 한 단계.
3. `position.has_value()`면 `TransformComponent.position`만 그 값으로 덮어쓴다(`rotation`/`scale`은 프리팹 값 유지) — `EditorAPI`가 이미 갖고 있는 `SetTransformPosition` 한 번 호출로 충분하다. `std::nullopt`면 프리팹 기본 위치를 그대로 둔다.

즉 "위치"에 한해서는 호출자가 넘긴 인자가 주어질 때만 프리팹 기본값을 이긴다. 회전/스케일까지 인자로 받을지는 v1에서는 다루지 않는다(필요해지면 같은 패턴으로 확장). pybind11 바인딩에서도 `std::optional`은 Python의 `position: Vec3 | None = None`으로 자연스럽게 매핑된다.

### 2.9 원본 파일 삭제 후 동작

`.prefab.json`이 삭제된 뒤에도 이미 스폰된 인스턴스 엔티티는 씬 안에서 완전히 독립적으로 남아있어야 한다(§2.1의 "설계도와 인스턴스는 독립적"이라는 전제와 같은 맥락) — 삭제가 기존 인스턴스에 어떤 부수효과도 일으키지 않는다. 다만 그 상태에서 Revert를 시도하면 `PrefabAsset::LoadFromFile`이 실패하므로, `ApplyToEntity`를 아예 호출하지 않고 에러를 표면화한다(Inspector에 "원본 프리팹을 찾을 수 없음" 메시지, 엔티티는 건드리지 않음). "파일이 없으면 조용히 아무 일도 안 일어난다"가 아니라 "왜 안 됐는지 사용자에게 보인다"를 택한다 — §2.5의 예측 가능성 원칙과 같은 이유.

### 2.10 ECS 컴포넌트 추가/제거 타이밍 가드

`ApplyToEntity`(§2.4)는 컴포넌트를 `remove()`/`deserialize()`로 직접 추가·제거한다. 이게 시스템 틱(`World::Update()`) 도중, 특히 병렬 실행(`World::SetParallelExecution`, [World.h:121](../engine/ecs/World.h))이 켜진 상태에서 호출되면 문제가 될 수 있다는 것을 실제로 확인했다 — `TypedComponentArray<T>::Remove()`([ComponentArray.h:37-59](../engine/ecs/ComponentArray.h))는 **swap-and-pop**으로 구현되어 있다(제거 대상을 배열 마지막 원소와 바꾸고 `pop_back`). 어떤 System이 `GetDenseArray()`를 순회하는 도중 다른 경로(에디터 스레드의 `ApplyToEntity` 호출 등)에서 같은 컴포넌트 타입에 `Remove`/`Add`가 끼어들면, 순회 중인 인덱스가 가리키는 원소가 바뀌거나(swap) 벡터가 `push_back`으로 재할당되어 이미 잡아둔 참조/포인터가 무효화될 수 있다.

**v1 확정 규칙**: `ApplyToEntity`는 **메인 스레드에서, `World`가 Edit 또는 Pause 상태일 때만**(`World::EditorState`, [World.h:44](../engine/ecs/World.h) — Play 중이 아닐 때) 동기 실행한다. 에디터의 Inspector/Scene Hierarchy 조작(Phase 3/4)은 전부 이 조건 하에서 일어나므로 v1 범위 안에서는 자연히 만족된다. 런타임(Play 모드) 중 프리팹을 스폰하는 시나리오는 이번 계획 범위 밖이며, 필요해지면 System 업데이트와 직렬화하기 위해 명령을 큐에 쌓았다가 프레임 경계에서 일괄 처리하는 커맨드 버퍼 방식을 별도로 설계한다(Phase 2 이후 과제로 명시).

> **착수 시 확인할 것**: 이 규칙은 `TypedComponentArray::Remove()`의 실제 구현(swap-and-pop)에는 근거하지만, "에디터 스레드의 ECS 호출과 `World::Update()`가 이미 서로 직렬화되어 있는지"(Qt 이벤트 루프 vs. 엔진 틱 스레드 관계)는 이 문서 작성 시점에 실측하지 못했다 — Phase 1/2 착수 시 `Engine::TickFrame`과 에디터 콜백의 호출 스레드 관계를 먼저 확인하고, 이미 직렬화되어 있다면 위 규칙은 "이미 보장된 것의 명시적 문서화"로 격하되고, 아니라면 실제 가드(assert 등)를 코드에 추가해야 한다.

### 2.11 `meshHandle` 영속성 한계 (고지, v1은 해결하지 않음)

`RenderableComponent.meshHandle`은 `uint32_t` 정수 핸들이고, 프리팹은 이 값을 있는 그대로 캡처/복원한다(§2.2). 오늘 시점(§1) `RenderSystem`은 등록되지 않은 `meshHandle`을 전부 절차적 유닛 큐브로 대체하므로([ROADMAP.md](../ROADMAP.md) §6), 지금 나오는 프리팹(예: "Test Cube" 계열, `meshHandle=0`)은 이 한계의 영향을 받지 않는다. 하지만 실제 메시 에셋 파이프라인이 생겨서 `meshHandle`이 세션마다 다른 순서로 할당되는 진짜 핸들이 되는 순간, **`.prefab.json`에 박아둔 정수 핸들 값은 재시작 후 다른 메시를 가리키거나 무효가 될 수 있다.** 이건 프리팹이 만든 문제가 아니라 기존 핸들 시스템의 한계를 프리팹이 그대로 물려받는 것이다 — MASTER_PLAN.md P1의 에셋 파이프라인이 경로/UUID 기반의 안정적 참조를 도입하면, 프리팹의 메시 참조도 그때 같이 옮겨야 한다는 것만 지금 명시해둔다(§5).

---

## 3. Phase 분해

Motion Mixer와 같은 원칙: **C++ 유닛테스트만으로 검증되는 Phase를 먼저 끝내고, 그 위에 에디터 UI를 얹는다.** 이전 Phase가 끝나지 않은 채 다음 Phase에 손대지 않는다.

### Phase 1 — `PrefabAsset` 자료구조 + 파일 I/O (C++, 유닛테스트만) — ✅ 완료(2026-08-19)

> **완료 기록**: 아래 설계 그대로 구현됨(`engine/prefab/SerializeOptions.h`/`PrefabInstanceComponent.h`/`PrefabAsset.h/.cpp`, `engine/ecs/Reflection.cpp`의 `RegisterPrefabComponentsReflection()`). `engine/tests/PrefabAssetTests.cpp` 19개(계획한 16개를 일부 더 세분화 — 예: EntityRef 거부를 "거부되는 케이스"/"통과하는 케이스" 2개로, 파일 I/O 실패를 "malformed JSON"/"존재하지 않는 경로" 2개로) 전부 통과. 전체 스위트 392개(373 기존 + 19 신규) — 387 통과/5 실패(기존 무관 실패 5개와 정확히 동일, 새 회귀 없음). 상세는 [ROADMAP.md](../ROADMAP.md) P1.6 참고.

**대상**: `engine/prefab/SerializeOptions.h`, `engine/prefab/PrefabInstanceComponent.h`, `engine/prefab/PrefabAsset.h/.cpp` (신규 디렉터리, `engine/animation/`과 같은 위치 규칙), `engine/ecs/Reflection.cpp`(`PrefabInstanceComponent` 리플렉션 등록 함수만 추가 — `Reflection.h`의 공개 선언부는 변경 없음)

**모듈 경계(개정 note 참고)**: `SerializeOptions`/`SerializeEntityComponents`는 전부 `engine/prefab/`에 둔다 — `engine/ecs/Reflection.h`는 `SerializeRegistry`/`DeserializeRegistry`/`ComponentRegistry` 공개 선언부를 그대로 유지하고 `prefab/`을 전혀 모른다(의존 방향은 `prefab → ecs`만 허용, 역방향 금지). `PrefabAsset`의 캡처 루틴은 `SerializeRegistry()`의 내부 루프를 공유하지 않고, `ComponentRegistry::GetAllComponents()`(이미 public)를 직접 순회하는 자체 구현이다 — 짧은 for-loop 중복은 감수하되(SRP: 두 코드가 바뀌는 이유 자체가 다름), `SerializeRegistry()`는 이 작업으로 전혀 건드리지 않아 회귀 리스크가 없다. `PrefabInstanceComponent`의 **리플렉션 등록**(다른 5종과 같은 자리)만은 `Reflection.cpp`에 새 함수(`RegisterPrefabComponentsReflection()`, 기존 `RegisterAIComponentReflection()`과 같은 패턴)로 추가한다 — `PrefabAsset`이 자기 컴포넌트 등록까지 책임지지 않게 하기 위함이며, 이 등록은 test 6/15가 의미 있으려면 **Phase 1 안에** 끝나 있어야 한다(구버전 초안은 Phase 2로 미뤘었는데, 그러면 Phase 1 테스트가 등록되지도 않은 컴포넌트를 제외하는 셈이라 아무것도 검증하지 못한다 — 이번 개정에서 Phase 1로 당김).

최종 API 선언(§2.3~§2.10의 결정을 반영, 실패 가능한 연산은 전부 저장소 전역 관례인 `std::expected<T, EngineError>`(**`engine/core/EngineError.h`**, `Error.h` 아님 — 개정 note 참고)로 통일):

```cpp
// engine/prefab/SerializeOptions.h
#pragma once
struct SerializeOptions
{
    bool excludePrefabMetadata = false;
    bool rejectEntityRefs      = false;
};

// engine/prefab/PrefabAsset.h (또는 PrefabAsset.cpp 내부 — SerializeOptions 유일한 소비자이므로
// PrefabAsset과 함께 선언해도 무방. ecs/Reflection.h에는 두지 않는다 — 위 모듈 경계 참고)
std::expected<nlohmann::json, EngineError>
SerializeEntityComponents(ECSRegistry& registry, Entity entity, SerializeOptions options = {});

// engine/prefab/PrefabInstanceComponent.h
#pragma once
#include <cstdint>
#include <string>
struct PrefabInstanceComponent
{
    std::string prefabPath;
    uint32_t    sourcePrefabVersion = 1;   // 파일 포맷 버전 스냅샷(§2.3, Option A) — revision 추적 용도 아님
};
// ComponentRegistry 등록(GE_BEGIN_COMPONENT 매크로)은 이 헤더가 아니라
// engine/ecs/Reflection.cpp의 RegisterPrefabComponentsReflection()에서 한다(Phase 1 범위) —
// "struct 존재" ≠ "등록됨"이며, 등록되어야 SerializeRegistry(PIE 스냅샷)와 향후 씬 저장(P0-b)에서
// 인스턴스 정보가 보존된다. 등록(registration)과 캡처 시 제외(SerializeOptions.excludePrefabMetadata)는
// 서로 다른 축이다.

// engine/prefab/PrefabAsset.h
#pragma once
#include "../core/EngineError.h"
#include "../ecs/ECSRegistry.h"
#include "SerializeOptions.h"
#include <nlohmann/json.hpp>
#include <expected>
#include <filesystem>
#include <string>

class PrefabAsset
{
public:
    std::string    name;
    uint32_t       version = 1;        // 파일 포맷 버전 (§2.2)
    nlohmann::json componentsData;     // §2.2의 "components" 객체와 같은 모양 — 그대로 직렬화

    // §2.6: SerializeOptions{excludePrefabMetadata=true, rejectEntityRefs=true}로 SerializeEntityComponents 호출
    static std::expected<PrefabAsset, EngineError>
    CaptureFromEntity(ECSRegistry& registry, Entity entity);

    // §2.4/§2.7: 새 엔티티 생성 + ApplyToEntity, 실패 시 생성한 엔티티를 스스로 DestroyEntity로 롤백
    std::expected<Entity, EngineError>
    SpawnInto(ECSRegistry& registry) const;

    // §2.4/§2.5: Spawn과 Revert가 공유하는 단일 Primitive.
    // Strong guarantee: 성공 → 컴포넌트 집합이 prefab과 완전히 동일. 실패 → 호출 전 상태로 정확히 복원.
    std::expected<void, EngineError>
    ApplyToEntity(ECSRegistry& registry, Entity entity) const;

    nlohmann::json ToJson() const;
    static std::expected<PrefabAsset, EngineError> FromJson(const nlohmann::json& json);

    std::expected<void, EngineError> SaveToFile(const std::filesystem::path& path) const;
    static std::expected<PrefabAsset, EngineError> LoadFromFile(const std::filesystem::path& path);
};
```

`componentsData`를 `std::unordered_map<std::string, json>`이 아니라 `nlohmann::json`(오브젝트) 하나로 두는 이유: 파일의 `"components"` 키가 이미 이름→값 맵 모양의 JSON 오브젝트이므로, `ToJson()`/`FromJson()`은 사실상 그대로 대입에 가깝고 굳이 중간 컨테이너로 한 번 더 감쌀 이유가 없다.

**완료 기준**: `engine/tests/PrefabAssetTests.cpp` 신규(16개, 사용자 제시 12 + 보완 4를 병합·정리):

1. Transform+Renderable을 가진 엔티티를 `CaptureFromEntity` → JSON round-trip(`ToJson`/`FromJson`) → 새 레지스트리에 `SpawnInto` → 컴포넌트 값이 원본과 일치.
2. `LoadFromFile`/`SaveToFile` round-trip이 디스크를 거쳐도 동일한 결과.
3. `SpawnInto`로 생성된 엔티티가 원본과 컴포넌트 값 기준으로 동등(별도 케이스로 명시).
4. `ApplyToEntity`를 기존 엔티티에 호출하면 값이 갱신됨(Spawn이 아닌 경로 단독 검증).
5. `ApplyToEntity`가 "프리팹에 없는 컴포넌트(예: `AIComponent`)"를 제거함(Definition A) — 단 `TransformComponent`/`PrefabInstanceComponent`는 프리팹에 없어도 제거되지 않음(제거 예외 2종, §2.5).
6. `PrefabInstanceComponent`가 이미 붙은 엔티티를 `CaptureFromEntity`하면 결과 `componentsData`에 `"PrefabInstanceComponent"` 키가 없음(§2.6).
7. `EntityRef` 타입 필드를 가진(테스트 전용) 컴포넌트를 등록해두고 그 필드가 유효한 참조를 가리키는 엔티티를 `rejectEntityRefs=true`로 캡처하면 실패를 반환(§2.6) — 필드가 없거나 비어있으면 성공.
8. 프리팹 JSON에 등록되지 않은(unregistered) 컴포넌트 이름이 있으면 **무시하고 성공**(정책 표 1행) — 크래시 없음.
9. 알려진 컴포넌트인데 필드 타입이 안 맞는 malformed 값(정책 표 3행)이 있으면 `try/catch`로 잡혀 `std::expected`의 에러 분기로 반환됨 — 예외가 호출자까지 새어나가지 않음.
10. 프리팹 최상위 JSON(`"version"` 등) 자체가 잘못된 형식이면 `FromJson`/`LoadFromFile`이 실패.
11. `LoadFromFile`에 애초에 파싱이 실패하는(malformed) 파일을 주면 예외 없이 `std::expected`의 에러 분기로 반환.
12. **`SpawnInto` 원자성** — 컴포넌트 적용 도중 실패를 유도했을 때 레지스트리에 고아 엔티티가 남지 않음(엔티티 수 불변).
13. **`ApplyToEntity` strong guarantee** — 기존 엔티티에 대해 컴포넌트 적용 도중 실패를 유도했을 때, 실패 후 엔티티의 컴포넌트 상태가 호출 전과 정확히 같음(§2.4의 스냅샷 복원 검증). **12와 13은 서로 다른 계약이다** — 12는 "고아 엔티티가 없다", 13은 "기존 엔티티가 보존된다"를 검증한다.
14. `componentsData = {}`(빈 오브젝트)인 프리팹을 `SpawnInto`하면 컴포넌트 없는 엔티티 하나만 생성되고 크래시 없음.
15. `SerializeEntityComponents`에 기본 `SerializeOptions{}`를 넘겼을 때의 출력이 `excludePrefabMetadata=true`를 넘겼을 때보다 `PrefabInstanceComponent` 블록만큼 더 많음(옵션이 실제로 분기하는지 직접 검증) — 이 테스트가 의미 있으려면 `PrefabInstanceComponent`가 실제로 `ComponentRegistry`에 등록되어 있어야 한다(위 "모듈 경계" 참고, Phase 1에서 등록).
16. **회귀 게이트**: `PrefabInstanceComponent` 등록을 `Reflection.cpp`에 추가한 뒤에도, 이 컴포넌트를 갖지 않은 기존 엔티티들에 대한 `SerializeRegistry()`의 출력이 등록 추가 이전과 완전히 동일(새 컴포넌트 타입 자체가 등장하는 것은 정상 동작이고 회귀가 아니다 — 대상은 "기존 5종 컴포넌트의 직렬화 결과가 안 바뀌었는가"). `SerializeRegistry()` 본체는 이번 작업에서 아예 건드리지 않으므로(위 "모듈 경계" 참고) 실질적으로는 낮은 리스크지만, 명시적으로 검증한다.

**Phase 1 완료 조건 요약**(위 16개 테스트가 실제로 검증하는 것을 한눈에 보기 위한 체크리스트 — 항목 자체는 전부 위 §2.3~§2.11/테스트 1~16과 1:1로 대응, 새 결정 아님):

- [ ] `PrefabAsset` 자료구조 + `LoadFromFile`/`SaveToFile` round-trip 통과(테스트 1, 2)
- [ ] `SerializeEntityComponents`가 `std::expected<json, EngineError>`로 동작(테스트 11, 15) — `engine/prefab/`에 위치, `ecs/Reflection.h`는 미변경(모듈 경계 참고)
- [ ] `PrefabInstanceComponent`가 **`ComponentRegistry`에 정상 등록**되고(`Reflection.cpp::RegisterPrefabComponentsReflection()`, Phase 1 범위) `serialize/deserialize/remove/hasComponent`가 모두 동작
- [ ] `CaptureFromEntity`가 `excludePrefabMetadata`로 메타 컴포넌트 제외 + `rejectEntityRefs`로 `EntityRef` 실패(테스트 6, 7)
- [ ] `ApplyToEntity`가 Definition A + strong guarantee(실패 시 상태 보존)로 동작(테스트 5, 13), 내부적으로 `CaptureSnapshot`/`SynchronizeComponents`(`RemoveMissingComponents`+`DeserializePrefabComponents`)/`RestoreSnapshotOnFailure`로 분리
- [ ] `SpawnInto` 원자성(실패 시 고아 엔티티 없음, 테스트 12)
- [ ] §2.5 컴포넌트 정책 표 6행이 전부 테스트로 검증됨(테스트 8, 9, 10)
- [ ] **기존 5종 컴포넌트에 대한 `SerializeRegistry()` 출력 불변(회귀 게이트, 테스트 16) — 하드 조건**
- [ ] `TransformComponent` 제거 예외 동작(테스트 5)
- [ ] `sourcePrefabVersion` = 파일 포맷 버전 스냅샷일 뿐 revision 추적 용도가 아님(§2.3, 문서 서술 확인)

렌더링/에디터 변경 없음. **착수 시 먼저 할 일**: §0(개정 note)에서 확인한 실제 경로(`engine/core/EngineError.h`)와, §2.10에서 확인이 필요하다고 표시한 "에디터 콜백과 `World::Update()`의 스레드 관계"를 먼저 코드에서 확인한다.

### Phase 2 — 런타임 인스턴스화 + Undo 연결 — ✅ 완료(2026-08-19)

> **완료 기록**: 아래 설계 그대로 구현됨. `InstantiatePrefabCommand`는 `CreateEntityCommand`의 Redo 패턴(첫 Apply에서 로드한 `PrefabAsset`을 캐싱해뒀다가 Redo에서 재사용 — 파일이 Undo/Redo 사이에 바뀌거나 삭제돼도 Redo가 항상 같은 결과를 내도록)을 그대로 따랐다. `engine/tests/InstantiatePrefabCommandTests.cpp` 9개 전부 통과, 전체 스위트 401개(392 기존 + 9 신규) — 396 통과/5 실패(기존 무관 실패 5개와 정확히 동일). `main.py`에 실측 검증용 프리팹 인스턴스화 호출(원점의 기존 "Test Cube" 옆, `(3,0,0)`에 `assets/prefabs/Barrel.prefab.json`)을 추가해 실제 에디터 실행 스크린샷으로 두 큐브가 나란히 정상 렌더링되는 것 확인(로그로도 같은 배치에 2개 인스턴스가 업로드됨을 확인). 상세는 [ROADMAP.md](../ROADMAP.md) P1.6 참고.

**대상**: `engine/editor/commands/InstantiatePrefabCommand.h/.cpp`(신규, `CreateEntityCommand.h`와 동일한 틀), `EditorAPI.h/.cpp`(`InstantiatePrefab` 추가), `EditorBindings.cpp`(`instantiate_prefab` 바인딩). `PrefabInstanceComponent`의 struct 정의와 `ComponentRegistry` 등록은 Phase 1에서 이미 끝나 있다(위 모듈 경계 참고) — Phase 2는 그걸 실제로 스폰된 엔티티에 부착하는 소비자일 뿐, 새로 등록하지 않는다.

- `InstantiatePrefabCommand::Apply()` 순서(§2.8 그대로): (1) `PrefabAsset::LoadFromFile(prefabPath)` — 실패하면 여기서 즉시 에러 반환, 아무 것도 만들지 않음. (2) `asset.SpawnInto(registry)` — 실패해도 §2.7에 의해 레지스트리에 흔적이 남지 않으므로 그대로 에러 전파. (3) 성공한 엔티티에 `PrefabInstanceComponent{prefabPath, asset.version}` 부착(`sourcePrefabVersion`은 로드된 `PrefabAsset::version`을 그대로 스냅샷). (4) `position.has_value()`면 `registry.SetTransformPosition(entity, *position)`. `Undo()`는 스폰된 엔티티를 파괴(`CreateEntityCommand::Undo`와 동일한 패턴) — (3)/(4)는 컴포넌트 값 수정일 뿐이므로 엔티티 파괴 하나로 전부 롤백된다.
- `EditorAPI::InstantiatePrefab(const std::filesystem::path& path, std::optional<glm::vec3> position = std::nullopt) -> std::expected<Entity, EngineError>`(§2.8 확정 시그니처) — `Dispatch()`를 그대로 재사용하므로 열린 트랜잭션이 있으면 자동으로 합류하고, 없으면 단독 Undo 단위가 된다(§1의 "Undo/Redo 트랜잭션 계층"을 그대로 상속받음 — 새로 설계하지 않음). pybind11에서 `std::optional`은 Python `position: Vec3 | None = None`으로 자연스럽게 매핑된다.

**완료 기준**: `engine/tests/InstantiatePrefabCommandTests.cpp` 신규(`CreateDestroyEntityCommandTests.cpp`와 같은 스타일):

- Apply 후 엔티티가 올바른 컴포넌트 값 + `PrefabInstanceComponent{prefabPath, sourcePrefabVersion}`를 갖는지.
- Apply → Undo → Redo(`CommandManager`의 재실행 경로)를 거친 뒤 최종 상태가 최초 Apply 직후 상태와 엔티티 컴포넌트 값 기준으로 동등한지(엔티티 ID/UUID 자체는 Redo마다 새로 생성되므로 동일할 필요는 없음 — 값만 비교).
- `BeginTransaction`으로 감싼 상태에서 "인스턴스화 + `MoveEntity`"를 연달아 호출하면 `CommandManager`의 Undo 스택에 트랜잭션 1개만 쌓이고, 그 한 번의 Undo로 이동과 생성이 함께 취소되는지(§1의 `Transaction`/`AddAppliedCommand` 경로 검증).
- 존재하지 않는 경로로 `InstantiatePrefabCommand::Apply()`를 호출하면 엔티티가 전혀 생성되지 않고(`GetEntityCount()` 불변) 에러가 반환되는지.
- Phase 1에서 만든 "일부러 실패하는" 컴포넌트/JSON으로 `SpawnInto`가 중간에 실패하는 상황을 재현해, `InstantiatePrefabCommand::Apply()` 이후에도 레지스트리 엔티티 수가 호출 전과 같은지(§2.7 원자성이 Command 층까지 그대로 전달되는지 확인).

실측 검증: `main.py`에 있는 기존 "Test Cube" 생성 자리(P0-2 검증용으로 이미 있음)를 프리팹 인스턴스화 호출로 바꾸거나 나란히 추가해서, RenderSystem이 이미 그리고 있는 화면에 프리팹에서 스폰된 큐브가 똑같이 나타나는지 스크린샷으로 확인 — 렌더링 경로는 이미 검증된 상태이므로 이번엔 "인스턴스화 결과물이 그 경로를 타는가"만 보면 된다.

### Phase 3 — 에디터 저작: "선택 항목으로 프리팹 만들기" — ✅ 완료(2026-08-19)

> **완료 기록**: `EditorAPI::CapturePrefab`은 ECS를 바꾸지 않는 순수 읽기+파일쓰기라서 다른 메서드와 달리 `Dispatch()`/`CommandManager`를 거치지 않는 것으로 확정(되돌릴 ECS 상태가 없음 — 계획에는 명시돼 있지 않았던, 구현 중 내린 결정). `panels/prefab_browser.py`(신규)는 `QDockWidget`으로 붙여서 어느 모드에서도 접근 가능하게 했다. 라이브 에디터 실행으로 Create Prefab → 원본 삭제(독립성 검증) → Prefab Browser에서 Instantiate → 렌더링까지 전 과정 확인(스크린샷). 상세는 [ROADMAP.md](../ROADMAP.md) P1.6 참고.

**대상**: `panels/scene_hierarchy.py`(우클릭 메뉴), `EditorAPI`(`CapturePrefab` 추가 + 바인딩), 신규 `assets/prefabs/` 디렉터리

- Scene Hierarchy에서 엔티티 우클릭 → "Create Prefab..." → 저장 경로 입력(`assets/prefabs/<name>.prefab.json`) → `editor_api.capture_prefab(entity, path)` 호출(`PrefabAsset::CaptureFromEntity` + `SaveToFile` 래핑).
- 프리팹 목록 UI: `motion_mixer.py`가 `assets/motion/*.skeleton.json`을 `glob`으로 스캔하는 것과 동일하게 `assets/prefabs/*.prefab.json`을 스캔하는 최소 리스트 위젯. **MASTER_PLAN.md §2의 P2 "에셋 브라우저"(범용)와는 다른, 프리팹 전용 최소 구현**이라는 것을 명시 — 범용 에셋 브라우저가 생기면 그쪽으로 흡수하면 된다.
- 목록에서 "Instantiate" → Phase 2의 `instantiate_prefab` 호출(원점 또는 뷰포트 중앙 근처에 배치).

**완료 기준**: 라이브 에디터 스크린샷 — 엔티티 선택 → Create Prefab → `assets/prefabs/`에 파일 생성 확인 → 목록에서 Instantiate → 화면에 새 독립 엔티티가 렌더링되는 것 확인(원본을 지워도 인스턴스는 남아있어야 함 — "설계도와 인스턴스는 독립적"이라는 §2.1 전제의 실제 검증).

### Phase 4 — Inspector 통합: 되돌리기(Revert) — ✅ 완료(2026-08-19, 라이브 검증은 2026-08-20에 완료)

> **완료 기록**: `RevertPrefabInstanceCommand`의 `Apply()`/`Undo()`가 둘 다 "임시 `PrefabAsset`을 만들어 그 `ApplyToEntity`를 호출"하는 같은 원리를 재사용하도록 구현(계획에는 명시돼 있지 않았던, 구현 중 발견한 단순화 — `ApplyToEntity`가 이미 Definition A + strong guarantee를 갖고 있으므로 Undo용 복원 로직을 따로 만들 필요가 없었다). C++ 유닛테스트 9개 전부 통과. **라이브 검증은 최초 시도 시 `ge_python.pyd`를 차단한 Windows 애플리케이션 제어 정책 때문에 막혔었으나, `ge_python`/`ge_engine` → `quarterflying`/`quarterflying_engine` 이름 개명(P1 Phase 6) 이후 재시도해서 통과했다.** 이 과정에서 프리팹 관련 하드코딩·버그 검수 요청에 따라 실제 버그 4건(프리팹 직접 관련 2건: `inspector.py`의 우회 import·C++ `FromJson` 음수 version 미검증, 프리팹과 무관한 선행 버그 2건: `GetEntityName` Entity 미포장·`entity_selected` Signal(int)의 Entity 손실 및 선택 미보존)을 추가로 발견·수정했다. 상세는 [ROADMAP.md](../ROADMAP.md) P1.6 참고.

**대상**: `panels/inspector.py`, `EditorAPI.h/.cpp`(`RevertPrefabInstance` 추가 + 바인딩)

- 선택한 엔티티에 `PrefabInstanceComponent`가 있으면 "Prefab: `<name>` [Revert] [Apply(비활성)]" 헤더 표시.
- Revert 클릭 → `EditorAPI::RevertPrefabInstance(entity)`가 `PrefabAsset::LoadFromFile(prefabPath)` → 성공하면 `ApplyToEntity(registry, entity)` 호출(§2.4/§2.5 Definition A — 프리팹에 없는 컴포넌트는 제거) → Inspector 필드 갱신. `LoadFromFile`이 실패하면 `ApplyToEntity`를 호출하지 않고 에러를 그대로 표면화(§2.9).
- Apply 버튼은 비활성 상태로 존재, 툴팁 "다음 단계에서 지원 예정"(§2.5의 명시적 스텁 정책).

**완료 기준**: `engine/tests/RevertPrefabInstanceCommandTests.cpp`(또는 `EditorAPI` 단위 테스트) 신규:

- 인스턴스에 프리팹에 없는 컴포넌트(예: `AIComponent`)를 추가한 뒤 Revert하면 그 컴포넌트가 제거되고 `PrefabInstanceComponent`는 남아있는지(Definition A 회귀 확인).
- 원본 `.prefab.json`을 삭제한 뒤 Revert를 시도하면 엔티티가 전혀 변경되지 않고 에러가 반환되는지(§2.9).
- 원본 파일을 수동으로 수정해 `PrefabAsset::version`이 인스턴스의 `sourcePrefabVersion`과 달라진 상태에서 Revert하면(v1은 버전을 게이트로 쓰지 않으므로, §2.3) 그래도 정상적으로 최신 파일 내용이 적용되는지 — "버전 불일치를 막지 않는다"는 것 자체가 v1의 의도된 동작임을 테스트로 고정.

라이브 검증: 인스턴스의 Transform을 Inspector에서 바꾼 뒤 Revert를 누르면 뷰포트에서 즉시 원래 위치로 되돌아가는 것을 스크린샷으로 확인.

### Phase 5 (확장 과제, 이번 계획 범위 밖) — 다중 엔티티/계층 프리팹, 베리언트

§1에서 짚은 "엔티티 계층 컴포넌트 부재"를 실제로 메꿔야 하는 지점. `FieldType::EntityRef`(이미 리플렉션 매크로가 지원)를 쓰는 `HierarchyComponent{parent: EntityRef}` 도입이 전제 조건. MASTER_PLAN.md §2의 P2 "프리팹 베리언트"(Apply의 다중 인스턴스 역전파 포함)도 여기서 같이 다룬다. 착수 전에 이 계획서에 별도 절로 다시 설계를 채워 넣는다 — 지금은 "여기서 이어진다"는 지점만 표시해둔다.

---

## 4. 진행 순서 요약

```
Phase 1  PrefabAsset 자료구조 + 파일 I/O (C++, 유닛테스트만)
    │    — SerializeRegistry의 기존 직렬화 함수 재사용
    ▼
Phase 2  런타임 인스턴스화 + Undo 연결 (C++, 유닛테스트 + 스크린샷)
    │    — 기존 RenderSystem/CommandManager/Transaction 위에 얹음, 신규 렌더링 코드 없음
    ▼
Phase 3  에디터 저작: 선택 항목 → 프리팹 저장 + 인스턴스화 UI
    │    — assets/prefabs/*.prefab.json 파일 스캔 (engine/asset/ 미사용)
    ▼
Phase 4  Inspector 되돌리기(Revert) — 전체 컴포넌트 단위, 필드 diff는 확장 과제
    │
    ▼
Phase 5  (확장 과제) 계층 컴포넌트 → 다중 엔티티 프리팹 → 베리언트/Apply 역전파
```

---

## 5. 이 계획이 명시적으로 미루는 것

- **씬 파일에 프리팹 인스턴스가 영속되는 것** — `scene.json`이 ECS에 연결되기 전까지는 원천적으로 불가능(§1). MASTER_PLAN.md P0-b 선행 필요.
- **필드 단위 오버라이드 diff/하이라이트** — §2.5.
- **Apply(인스턴스 → 원본 역전파, 다중 인스턴스 동시 갱신)** — §2.5, §3 Phase 4.
- **다중 엔티티/계층 프리팹, 중첩 프리팹, 베리언트** — §3 Phase 5.
- **`engine/asset/` 모듈을 살리는 것** — 이 계획은 그 모듈을 그냥 우회한다. 살릴지 다시 짤지는 MASTER_PLAN.md P1의 에셋 파이프라인 작업이 결정할 문제.
- **콘텐츠 revision 추적(`sourceRevision`)** — §2.3 Option B. `sourcePrefabVersion`은 파일 포맷 버전일 뿐이라 이 용도로 못 쓴다는 것만 확정하고, 실제 필드 설계는 Phase 5(베리언트/Apply 역전파)로 미룬다.
- **`meshHandle` 등 정수 에셋 핸들의 재시작 안전성** — §2.11. 실제 메시 에셋 파이프라인(경로/UUID 기반 안정 참조)이 생기기 전까지는 프리팹도 이 한계를 그대로 물려받는다.

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- [engine/asset/AssetHandle.h](../engine/asset/AssetHandle.h) — `AssetType::Prefab` 외 프리팹 관련 코드 없음 확인
- [engine/ecs/Reflection.h](../engine/ecs/Reflection.h), [Reflection.cpp](../engine/ecs/Reflection.cpp) — `ComponentRegistry`/`SerializeRegistry`/`DeserializeRegistry`/매크로 3종
- [engine/ecs/Entity.h](../engine/ecs/Entity.h) — 계층(부모-자식) 개념 부재 확인
- [engine/ecs/World.cpp](../engine/ecs/World.cpp) — PIE 스냅샷이 `SerializeRegistry`를 쓰는 실사용 사례
- [engine/editor/EditorAPI.h/.cpp](../engine/editor/EditorAPI.h), [Transaction.h](../engine/core/Transaction.h), [commands/CreateEntityCommand.h](../engine/editor/commands/CreateEntityCommand.h) — Undo/Redo·Command 패턴
- [engine/core/EngineError.h](../engine/core/EngineError.h) — `EngineErrorCode`/`EngineError`/`MakeError()` 실제 정의 위치(`Error.h` 아님, v2.0 계약서의 §0 가정을 이 저장소 기준으로 정정)
- [engine/ecs/ComponentArray.h](../engine/ecs/ComponentArray.h) — `TypedComponentArray::Remove()`가 swap-and-pop임을 확인(§2.10 ECS 타이밍 가드의 근거)
- [engine/bindings/EditorBindings.cpp](../engine/bindings/EditorBindings.cpp), [ECSBindings.cpp](../engine/bindings/ECSBindings.cpp) — `editor_api` 파이썬 노출 방식, `SetComponentJson`류 버전 체크 선례
- [engine/editor/demo_scene_integration.py](../engine/editor/demo_scene_integration.py) — `scene.json`이 ECS에 연결되지 않았음을 확인(순수 dict 저장, `CreateEntity` 호출 없음)
- [engine/editor/panels/motion_mixer.py](../engine/editor/panels/motion_mixer.py) — 파일 스캔 기반 에셋 탐색 선례
- [ROADMAP.md](../ROADMAP.md) §6 — RenderSystem/InstancedBatchManager 완성 기록(프리팹 인스턴스 렌더링의 전제 조건)
- [docs/MASTER_PLAN.md](MASTER_PLAN.md) §6.2 — 이 계획서를 만들게 된 지시 사항 원문
모듈간의 단일책임 원칙으로 