# VFX Lite Phase 3 — 렌더링 세부 계획서 (검토용)

**작성일**: 2026-09-09
**상태**: ✅ **3A~3E 전 단계 완료 (2026-09-10, 실제 실행으로 화면 검증).** §8의 열린 질문 3개는 검토에서 전부 권장안대로 확정됐다 — ① 월드 공간, ② 월드 단위 크기, ③ 3C 단계 포함. 각 단계의 관문 통과 증거는 §9에 기록했다.
**상위 문서**: [docs/VFX_LITE_IMPLEMENTATION_PLAN.md](VFX_LITE_IMPLEMENTATION_PLAN.md) §3 "Phase 3" — 그 계획서가 **"이 계획에서 가장 위험한 Phase"**로 표시한 단계를, 착수 가능한 수준까지 쪼갠 것이다.

> **왜 별도 문서인가**: Phase 0~2는 전부 유닛테스트로 닫혔지만 Phase 3은 **유닛테스트로 대체 불가능한 검증**(화면에 실제로 보이는가)에 의존한다. 게다가 (a) 저장소에 존재한 적 없는 렌더 상태 전환을 새로 만들고, (b) Phase 0에서 추가한 attribute 12를 처음으로 실제 소비하며, (c) Phase 2가 만들었지만 **아직 아무도 호출하지 않는** `ParticleSystem`을 엔진에 연결한다. 세 가지가 한 번에 실패하면 원인 분리가 어렵기 때문에 단계를 쪼개고 각 단계의 관문을 미리 정한다.

---

## 1. 착수 전 확인된 사실 (실측)

이 계획의 모든 설계는 아래 위에 있다. 추측과 구분하기 위해 근거 위치를 함께 적는다.

### 1.1 이미 검증된 것 — 생각보다 든든하다

- **인스턴싱 렌더 경로는 화면으로 검증된 적이 있다.** `ROADMAP.md` §6(2026-08-18, P0-2 완결)에 *"Scene Editor 뷰포트에 그리드 + 원점의 Test Cube 엔티티가 올바른 크기·비례·음영으로 렌더링됨을 확인(스크린샷, PrintWindow 캡처)"* 로 기록되어 있다. 즉 `RenderSystem` → `InstancedBatchManager` → `SceneMeshRenderer` → 화면 경로와 **인스턴스 attribute 3~7(`aModel`, `aInstanceColor`)이 실제로 동작한다**는 것은 이미 확인된 사실이다.
- 따라서 `CLAUDE.md`의 "알려진 미검증 항목"(`InstanceData` GPU 레이아웃)과 "Scene Editor 3D 뷰포트가 검은 화면"은 **2026-08-18 시점에 해소된 옛 기록**이다. 검은 화면의 실제 원인도 렌더러가 아니라 Qt 전역 스타일시트가 네이티브 임베딩 뷰포트 위에 덧칠하던 것으로 밝혀져 `engine/editor/viewport.py`에서 수정됐다(`ROADMAP.md` §5 표).

> 이 사실이 Phase 3의 위험도를 크게 낮춘다. "인스턴싱이 아예 안 그려지는가"를 의심할 필요가 없고, **새로 추가되는 것들(attribute 12, 블렌드 상태, 쿼드 메시, 시스템 연결)만** 의심하면 된다.

### 1.2 아직 검증되지 않은 것

| 항목 | 상태 |
|---|---|
| `InstanceData.alpha`(attribute 12) | Phase 0에서 추가했고 **컴파일까지만** 확인. 이를 선언하는 셰이더가 아직 없어서 GPU까지 도달하는지는 미확인 |
| 블렌딩/깊이 쓰기 제어 | 저장소에 `glEnable(GL_BLEND)`·`glBlendFunc`·`glDepthMask`가 **0건**. 새로 만든다 |
| `PassType::ForwardTransparent` | enum 값으로만 존재. 이 패스를 그리는 호출부가 **없다** |
| `ParticleSystem`(Phase 2) | 구현·유닛테스트 완료. 그러나 **어떤 World에도 등록되어 있지 않다** — 지금은 아무도 `Update()`를 부르지 않는다 |
| 쿼드 프리미티브 메시 | `renderer/`에 Quad/Plane 생성 헬퍼가 없다. 새로 만든다 |

### 1.3 기대는 구조 (그대로 따라갈 본보기)

- **`SceneMeshRenderer`가 정확한 본보기다**([renderer/SceneMeshRenderer.h](../engine/renderer/SceneMeshRenderer.h)/[.cpp](../engine/renderer/SceneMeshRenderer.cpp)): `Initialize()`/`Shutdown()`/`IsInitialized()`/`Render(batchManager, camera)` 구조에, 셰이더를 **파일이 아니라 소스 문자열로** 컴파일한다. 파일 경로 방식을 쓰지 않는 이유가 헤더 주석에 있다 — *상대경로가 프로세스 CWD 기준인데 에디터는 `engine/editor/`에서 실행되므로 항상 깨진다.* 파티클 셰이더도 같은 이유로 인라인 소스로 간다.
- **렌더 상태 저장·복원 관례가 이미 있다**: `DebugGridRenderer`/`BoneLineRenderer`가 `glIsEnabled(GL_DEPTH_TEST)`로 이전 상태를 읽어두고 그린 뒤 되돌린다. 파티클도 이 관례를 따른다.
- **`GL_DEPTH_TEST`는 전역으로 한 번 켜진다**([platform/Win32Platform.cpp:426](../engine/platform/Win32Platform.cpp#L426)). 파티클은 이걸 **끄지 않고**, 깊이 *쓰기*만 끈다.
- **의존 방향은 `ecs/` → `renderer/`다.** `RenderSystem`(ecs)이 `InstancedBatchManager`(renderer)를 알고, `SceneMeshRenderer`(renderer)는 ECS를 모른다. §3.1의 책임 분리가 이 방향을 그대로 따른다.
- **시스템 등록 지점은 한 곳이다**: [core/Engine.cpp](../engine/core/Engine.cpp)의 `CreateWorld()`가 모든 World에 `RenderSystem`을 자동 등록한다("World가 생성되는 유일한 경로가 여기라서 Python이 만드는 World도 빠짐없이 커버됨"). `ParticleSystem`도 같은 자리에 붙인다.
- `Vertex` = `{px,py,pz, nx,ny,nz, u,v}` 8 float([renderer/Vertex.h](../engine/renderer/Vertex.h)), `Mesh::create(vertices, indices)`로 코드에서 메시 생성 가능.

---

## 2. Phase 3이 만드는 것 (한눈에)

```text
[ecs]                                    [renderer]
ParticleSystem::Update()
  ├─ (Phase 2) 스폰/적분/제거
  └─ CollectInstances()          ──►  InstancedBatchManager
        파티클마다 mat4 조립                 (배치 1개, ForwardTransparent)
        alpha = color.a                          │
                                                 ▼
                                        ParticleRenderer::Render()
                                          ├─ 블렌드/깊이쓰기 상태 켬
                                          ├─ RenderBatches(ForwardTransparent, shader)
                                          └─ 상태 원복
                                                 │
Engine::CreateWorld()                            ▼
  └─ RegisterSystem(ParticleSystem)      Renderer::Render()에서
                                          SceneMeshRenderer 다음에 호출
```

신규 파일 2개(`renderer/ParticleRenderer.h/.cpp`), 수정 4개(`ecs/ParticleSystem.h/.cpp`, `core/Engine.cpp`, `renderer/Renderer.h/.cpp`).

---

## 3. 설계 결정

### 3.1 책임 분리 — 인스턴스는 ECS가 채우고, 렌더러는 그리기만 한다

**결정**: `ParticleSystem`(ecs)이 인스턴스 데이터를 `InstancedBatchManager`에 채우고, `ParticleRenderer`(renderer)는 셰이더/렌더 상태만 다룬다.

반대 방향(`ParticleRenderer`가 `ParticleSystem`을 읽어서 채우기)이 언뜻 더 깔끔해 보이지만, 그러면 `renderer/`가 `ecs/`를 알아야 해서 **이 저장소의 의존 방향을 뒤집는다**(§1.3). 기존 `RenderSystem` ↔ `SceneMeshRenderer` 쌍과 정확히 같은 모양으로 간다.

Phase 2가 "`ParticleSystem`은 그리지 않는다"고 했던 것과 어긋나지 않는다 — CPU 쪽 인스턴스 배열을 채우는 것은 draw call이 아니고, `RenderSystem`이 이미 하는 일과 같다.

**Phase 2의 테스트를 깨지 않는 방법**: `RenderSystem`과 똑같이 생성자에서 `InstancedBatchManager*`/`Camera*`를 받되 **nullptr을 허용**한다. 둘 중 하나라도 없으면 `CollectInstances()`는 조용히 건너뛴다. Phase 2의 24개 테스트는 인자 없는 생성자를 쓰므로 그대로 통과한다.

```cpp
class ParticleSystem : public System {
public:
    ParticleSystem() = default;                                   // 유닛테스트용(Phase 2)
    ParticleSystem(InstancedBatchManager* batchManager, Camera* camera);
    ...
};
```

### 3.2 배치 구성 — 엔진 전체에 파티클 배치 하나

상위 계획서 §2.6 그대로. 모든 파티클이 같은 쿼드/셰이더/블렌드를 쓰므로 `InstancedBatchKey` 하나면 되고, 그러면 **Draw Call이 이펙트 개수와 무관하게 1개**가 된다.

```cpp
InstancedBatchKey kParticleBatchKey {
    .meshGuid       = kParticleQuadGuid,     // 파티클 전용 상수
    .materialId     = kParticleMaterialId,   // §2.1 - 지금은 의미 없는 상수, 텍스처 자리 예약
    .shaderId       = 0,                     // 셰이더는 RenderBatches 인자로 넘어간다
    .passType       = PassType::ForwardTransparent,
    .materialLayout = MaterialLayout::Unlit,
    .features       = PipelineFeature::None
};
```

`passType`이 `ForwardTransparent`이므로, 기존 `SceneMeshRenderer`의 `RenderBatches(ForwardOpaque, ...)` 호출은 이 배치를 **건드리지 않는다.** 이것이 "기존 렌더 경로 무영향"의 실질적 근거다.

매 프레임 순서:

```text
ClearInstances(key)                 # AddInstance는 중복 방지 없이 append라 안 비우면 누적된다
  → 활성 이펙트의 살아있는 파티클마다 AddInstance(key, data, 0)
  → (렌더러가) UpdateDynamicBatches() → RenderBatches(ForwardTransparent, shader)
```

**배치 수명**: `CleanupStaleBatches()`를 부르지 않는다(상위 계획서 §2.6). 현재 호출부가 없으므로 유지만 하면 된다 — 부르는 순간 파티클이 잠시 0개가 된 사이에 VAO/VBO가 파괴되고 다음 Burst에서 재생성된다.

**배치 생성 시점**: `RenderSystem`의 메시 지연 생성과 같은 패턴. `CollectInstances()`가 처음 호출될 때(= GL 컨텍스트가 확실히 있는 시점) 쿼드 메시를 만들고 `CreateBatch()`한다. `CreateBatch`의 `Result`를 **무시하지 않는다** — 실패하면 로그를 남기고 그 프레임의 수집을 건너뛴다(`RenderBatch()`의 `instanceVAO == 0` 방어 체크가 걸리는 상황을 애초에 안 만든다).

### 3.3 쿼드 메시

`Mesh::create()`로 코드에서 1회 생성한다. XY 평면의 단위 쿼드, 중심이 원점:

| vertex | position | normal | uv |
|---|---|---|---|
| 0 | (-0.5, -0.5, 0) | (0,0,1) | (0,0) |
| 1 | ( 0.5, -0.5, 0) | (0,0,1) | (1,0) |
| 2 | ( 0.5,  0.5, 0) | (0,0,1) | (1,1) |
| 3 | (-0.5,  0.5, 0) | (0,0,1) | (0,1) |

인덱스 `{0,1,2, 2,3,0}`. **UV가 핵심**이다 — §3.5의 절차적 원형 마스크가 `TexCoords`를 쓴다. normal은 파티클 셰이더가 읽지 않지만 `Vertex` 레이아웃(attribute 1)이 요구하므로 채운다.

### 3.4 인스턴스 조립 — 파티클마다 mat4

`InstanceData`가 위치/크기/회전을 개별 값으로 받지 않으므로 행렬로 조립한다(상위 계획서 §2.7).

```text
model = T(effectWorldPos + particle.position) * R_billboard * R_z(particle.rotation) * S(particle.size)
color = particle.color.rgb
alpha = particle.color.a          # Phase 0에서 추가한 attribute 12
```

- **`R_billboard`는 프레임당 1회만 계산**해서 모든 파티클이 재사용한다(뷰 행렬 회전 부분의 전치). 파티클마다 다시 구하지 않는다.
- **CPU 쪽 임시 인스턴스 배열을 매 프레임 새로 만들지 않는다**(§5.6) — `ParticleSystem`의 멤버로 두고 `clear()` 후 재사용한다. 안 그러면 "Draw Call은 줄었는데 CPU 할당이 매 프레임 반복되는" 절반짜리 최적화가 된다.
- `entityId`/`isSelected`/`roughness`/`metallic`은 파티클 셰이더가 읽지 않으므로 0으로 둔다. (피킹 대상이 아니다 — 파티클은 ECS 엔티티가 아니라 선택할 수 있는 대상이 아니다.)

> **빌보드를 CPU에서 하는 이유**: 정점 셰이더에서 뷰 공간 오프셋으로 처리하는 방법이 CPU 비용은 더 싸지만, 그러려면 회전과 크기를 `mat4`가 아닌 별도 attribute로 넘겨야 한다 — `InstanceData`에 그 자리가 없다(Phase 0에서 알파 하나를 넣느라 남은 padding을 이미 다 썼다). `aModel`을 그대로 쓰는 쪽이 기존 구조를 안 건드린다.

### 3.5 셰이더 (인라인 소스)

정점 셰이더 — **location 0~11을 전부 선언한다.** 기존 인스턴스 셰이더들의 관례이기도 하고, 쓰지 않는 attribute를 선언해두면 VAO 레이아웃과 셰이더가 어긋났을 때 알아채기 쉽다.

```glsl
#version 330 core
layout(location = 0)  in vec3  aPos;
layout(location = 1)  in vec3  aNormal;        // 안 씀
layout(location = 2)  in vec2  aTexCoords;
layout(location = 3)  in mat4  aModel;         // 3,4,5,6
layout(location = 7)  in vec3  aInstanceColor;
layout(location = 8)  in float aRoughness;     // 안 씀
layout(location = 9)  in float aMetallic;      // 안 씀
layout(location = 10) in uint  aEntityId;      // 안 씀
layout(location = 11) in uint  aIsSelected;    // 안 씀
layout(location = 12) in float aAlpha;         // Phase 0에서 추가한 자리

out vec2  TexCoords;
out vec3  Color;
out float Alpha;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    Color     = aInstanceColor;
    Alpha     = aAlpha;
    gl_Position = projection * view * aModel * vec4(aPos, 1.0);
}
```

프래그먼트 셰이더 — 텍스처 없이 절차적 원형 마스크(§2.1):

```glsl
#version 330 core
in vec2  TexCoords;
in vec3  Color;
in float Alpha;
out vec4 FragColor;

void main()
{
    float d    = length(TexCoords - vec2(0.5));
    float mask = 1.0 - smoothstep(0.35, 0.5, d);
    if (mask <= 0.0) discard;          // 모서리 픽셀은 블렌딩 비용조차 내지 않는다

    // 가산 혼합(ONE, ONE)이라 알파 채널은 무시된다 - 페이드는 밝기로 표현한다.
    FragColor = vec4(Color * Alpha * mask, 1.0);
}
```

> **여기가 attribute 12를 처음으로 실제 소비하는 지점이다.** `aAlpha`가 GPU까지 도달하지 못하면 파티클이 수명 내내 같은 밝기로 보인다(사라지지 않는다) — §6의 진단표에 이 증상을 넣어뒀다.

### 3.6 렌더 상태 전환 — 파티클 렌더 경로 안에서만, 반드시 원복

```cpp
void ParticleRenderer::Render(InstancedBatchManager& batchManager, const Camera& camera)
{
    if (!initialized) return;

    batchManager.UpdateDynamicBatches();   // SceneMeshRenderer와 같은 자리

    // 이전 상태를 읽어둔다 - DebugGridRenderer/BoneLineRenderer의 관례.
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWas    = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWas);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);   // 가산 혼합 - 순서 무관(구현 계획서 §2.2)
    glDepthMask(GL_FALSE);         // Z-Test는 유지, Z-Write만 끈다

    shader.use();
    shader.setMat4("view", camera.getViewMatrix());
    shader.setMat4("projection", camera.getProjectionMatrix());
    batchManager.RenderBatches(PassType::ForwardTransparent, &shader);

    glDepthMask(depthMaskWas);
    if (!blendWasEnabled) glDisable(GL_BLEND);
}
```

- **Z-Test를 유지하는 이유**: 파티클이 벽 뒤에 있으면 가려져야 한다. 끄면 모든 것을 뚫고 보인다.
- **Z-Write를 끄는 이유**: 파티클끼리 서로를 깊이 버퍼로 잘라내지 않게 한다. 켜두면 먼저 그려진 파티클이 뒤 파티클을 가려서, 정렬이 없는 상태에서 프레임마다 결과가 달라진다.
- **원복이 중요한 이유**: 이 상태를 흘리면 그 다음에 그리는 것(RenderGraph 패스 등)이 전부 영향을 받는다. §5의 검증 항목에 "파티클을 그린 뒤 불투명 물체가 정상인가"를 넣은 것이 이 때문이다.

### 3.7 `Renderer::Render()` 훅 위치

[renderer/Renderer.cpp](../engine/renderer/Renderer.cpp)의 `sceneMeshRenderer->Render(...)` **바로 다음**에 넣는다. 불투명 → 투명 순서를 호출 순서로 보장한다(구현 계획서 §2.2 확정 ③).

```cpp
if (renderMode == RenderMode::Scene && sceneMeshRenderer && sceneMeshRenderer->IsInitialized()
    && instancedBatchManager && mainCamera)
{
    sceneMeshRenderer->Render(*instancedBatchManager, *mainCamera);

    // 불투명 다음에 투명. 파티클은 Scene 모드에서만 그린다(Motion 모드는 본 라인이 그 자리).
    if (particleRenderer && particleRenderer->IsInitialized())
    {
        particleRenderer->Render(*instancedBatchManager, *mainCamera);
    }
}
```

`Renderer::Initialize()`에서 `sceneMeshRenderer` 바로 아래에 생성한다. **셰이더 컴파일 실패를 치명 에러로 취급하지 않는다** — `sceneMeshRenderer`가 이미 그렇게 하고 있고("실패해도 치명 에러 취급 안 함 - 그리드/기존 기능은 계속 동작해야 함"), 파티클도 같은 이유로 실패 시 로그만 남기고 나머지 렌더링은 계속돼야 한다.

### 3.8 `ParticleSystem` 등록 — Phase 2가 남긴 미연결 고리

지금 `ParticleSystem`은 **아무도 부르지 않는다.** `Engine::CreateWorld()`의 `RenderSystem` 등록 바로 옆에 붙인다:

```cpp
Renderer* renderer = GetSubsystem<Renderer>();
world->RegisterSystem(std::make_unique<RenderSystem>(
    renderer ? renderer->GetInstancedBatchManager() : nullptr, defaultCamera.get()));

world->RegisterSystem(std::make_unique<ParticleSystem>(
    renderer ? renderer->GetInstancedBatchManager() : nullptr, defaultCamera.get()));
```

`RenderSystem`과 같이 nullptr 허용이라, Renderer 서브시스템이 없어도 크래시하지 않고 시뮬레이션만 돈다.

### 3.9 실행 순서 — `ParticleSystem`은 `RenderSystem` 뒤

`RenderSystem`이 매 프레임 ECS의 Main Camera를 읽어 `Camera*`를 갱신한다. `ParticleSystem`이 먼저 돌면 **한 프레임 뒤진 카메라로 빌보드를 만든다** — 카메라가 빠르게 돌 때 파티클이 미세하게 어긋나 보인다.

`System::GetPriority()`(낮을수록 먼저)를 `ParticleSystem`에서 `100`으로 준다(`RenderSystem`은 기본값 0).

> **착수 시 확인할 것**: 이 저장소의 스케줄러(`SystemDependencyAnalyzer`/`ParallelGroupBuilder`)가 `GetPriority()`를 실제로 존중하는지는 아직 읽어보지 않았다. 존중하지 않는다면 순서 보장 방법을 따로 정해야 한다(예: `ParticleSystem`이 자기 `Update()` 안에서 카메라를 직접 읽기). 3A 착수 전에 확인한다.

### 3.10 시뮬레이션 공간 — 월드 공간(스폰 시점 이미터 위치 고정)

Phase 2는 파티클을 이펙트 로컬 원점(`position = vec3(0)`)에서 스폰한다. 월드 배치를 어떻게 할지는 두 가지다:

| | 로컬 공간 | **월드 공간(권장)** |
|---|---|---|
| 방식 | 렌더 시 이미터의 현재 위치를 더한다 | **스폰 시** 이미터 위치를 파티클에 굽는다 |
| 이미터가 움직이면 | 이미 뿜은 파티클이 **따라온다** | 뿜은 자리에 **남는다** |
| 어울리는 것 | 캐릭터에 붙은 오라, 무기 잔광 | 폭발, 연기, 피격, 스파크, 이동 잔상 |

**권장: 월드 공간.** §2의 커버 목록(불·연기·먼지·피격·마법·폭발·눈비·스파크·**이동 잔상류**) 대부분이 "뿜은 자리에 남아야" 자연스럽다. 특히 "이동 잔상"은 로컬 공간에서는 아예 성립하지 않는다(잔상이 캐릭터를 따라다니면 잔상이 아니다).

구현: `SpawnOne()`에서 owner 엔티티의 `TransformComponent`를 읽어 `p.position = ownerWorldPos`로 시작한다. Phase 2 코드의 한 줄 변경이고, 새 파라미터가 생기지 않으므로 **§2의 18개는 그대로다.**

> Unity의 "Simulation Space: Local/World"에 해당하는 선택이다. 둘 다 지원하면 19번째 파라미터가 되므로 v1은 하나만 고른다. §8의 열린 질문 ①.

---

## 4. 세부 작업 단계

각 단계는 **바로 앞 단계가 화면으로 확인된 뒤에** 시작한다. 한 번에 다 만들고 "안 보인다"에 부딪히는 것을 피하는 것이 이 분할의 전부다.

### 3A — `ParticleSystem`을 World에 연결 (렌더링 없음)

§3.8. `Engine::CreateWorld()`에 등록만 한다. `CollectInstances()`는 아직 없다.

- **관문**: 에디터를 `HAS_ENGINE=True`로 띄우고, `ParticleEffectComponent`를 붙인 엔티티를 만들었을 때 **크래시 없이** 프레임이 계속 돈다. 로그로 이펙트 인스턴스가 생성됐는지 확인한다(임시 로그 허용).
- 화면에는 아무 변화가 없어야 정상이다.
- §3.9의 실행 순서 전제(`GetPriority()` 존중 여부)를 여기서 확인한다.

### 3B — 쿼드 메시 + 배치 생성 (그리지는 않음)

§3.2/§3.3. `CollectInstances()`가 메시를 만들고 `CreateBatch()` → `ClearInstances()` → `AddInstance()`까지 한다. 아직 `ParticleRenderer`가 없으므로 **아무것도 그려지지 않는다.**

- **관문**: `CreateBatch` 성공 로그, 그리고 `RendererStats.totalInstances`가 살아있는 파티클 수와 일치. `RenderBatch()`의 `instanceVAO == 0` 에러 로그가 **뜨지 않는다.**
- 기존 불투명 렌더링(그리드 + Test Cube)이 **변함없이 보인다** — 배치를 하나 더 만든 것이 기존 경로에 영향이 없음을 여기서 확인한다.

### 3C — 셰이더 + 렌더러: 일단 불투명으로 그려본다

§3.5/§3.6/§3.7. **단, 첫 시도는 블렌딩을 켜지 않는다.** `passType`만 `ForwardTransparent`로 두고 렌더 상태는 기본값(블렌드 off, 깊이 쓰기 on) 그대로 그린다.

- **관문**: 파티클이 **불투명한 흰/색 사각형 덩어리로 보인다.** 원형 마스크(§3.5)는 켜두므로 원반으로 보이면 UV까지 정상이라는 뜻이다.
- 이 단계에서 확인되는 것: 쿼드 메시 · 인스턴스 mat4 조립 · 빌보드 · `ForwardTransparent` 패스 호출 · UV. **블렌딩 문제와 섞이지 않는다.**
- 안 보이면 §6의 진단표로 간다.

### 3D — 알파 + 가산 혼합 켜기

§3.6의 상태 전환을 넣고, 프래그먼트에서 `Alpha`를 실제로 곱한다.

- **관문**: (1) 파티클이 수명에 따라 **밝기가 줄며 사라진다** → attribute 12가 GPU까지 도달했다는 증거. (2) 겹친 파티클이 **더 밝아진다** → 가산 혼합이 걸렸다. (3) 파티클 뒤의 불투명 물체가 **잘려나가지 않는다** → Z-Write off가 걸렸다.
- (1)이 실패하면 원인이 attribute 12로 좁혀진다 — 3C까지 다른 모든 것이 확인됐기 때문이다. **이 분할의 핵심 이득이 여기다.**

### 3E — 상태 원복 및 회귀 확인

- **관문**: 파티클을 그린 **뒤에도** 그리드·Test Cube가 정상. 파티클 이펙트를 삭제하면 잔상 없이 사라지고 기존 렌더링이 그대로. `RendererStats.drawCalls`가 이펙트 개수와 무관하게 파티클 배치 1개.

---

## 5. 검증 계획 (무엇을 "완료"로 볼 것인가)

CLAUDE.md 관례 #2대로 세 단계를 섞지 않는다.

| 단계 | 이 Phase에서의 의미 |
|---|---|
| 컴파일/링크 | 당연히 필요하지만 **아무 것도 증명하지 않는다.** 렌더링은 컴파일로 검증되는 종류가 아니다 |
| 유닛테스트 | 부분적으로만 가능 — 아래 참고 |
| **실제 실행 검증** | 이 Phase의 실질적 관문. 3A~3E의 관문을 각각 기록한다 |

**유닛테스트로 덮을 수 있는 것**(GL 없이):
- 쿼드 메시의 정점/인덱스 데이터가 의도대로인지(순수 데이터 생성 함수로 분리하면 가능)
- 인스턴스 조립 함수(파티클 + 이미터 위치 + 빌보드 행렬 → `InstanceData`)가 순수 함수라면 검증 가능 — `ComposeWorldMatrix`가 이미 그렇게 분리되어 있는 선례가 있다
- `ParticleSystem(nullptr, nullptr)`일 때 Phase 2의 24개 테스트가 그대로 통과하는지(회귀 게이트)

**유닛테스트로 덮을 수 없는 것**: 화면에 보이는가, 블렌딩이 의도대로인가, 상태가 원복되는가. 전부 3C~3E의 실행 관문으로만 확인된다.

> **"크래시 없음"과 "의도대로 그려짐"은 다른 주장이다.** 3A~3E의 관문을 기록할 때 둘을 반드시 구분해서 적는다.

---

## 6. 실패 모드별 진단 순서

단계를 쪼갠 이유가 이 표를 쓸 수 있게 하기 위해서다. 증상이 나오면 **해당 행부터** 의심한다.

| 증상 | 가장 먼저 의심할 것 | 근거 |
|---|---|---|
| 3C에서 아무것도 안 보임 | `CollectInstances()`가 실제로 불렸는가 → `ParticleSystem`이 World에 등록됐는가(3A), `batchManager`가 nullptr이 아닌가 | 3B에서 인스턴스 수가 확인됐다면 수집은 정상 |
| 3C에서 GL 에러 없이 안 보임 | 배치의 `instanceVAO == 0`(로그가 이미 잡아준다), 또는 `passType` 불일치로 `RenderBatches`가 건너뜀 | `RenderBatches`는 `passType != 인자`인 배치를 조용히 건너뛴다 |
| 파티클이 화면 한 점에 뭉침 | 빌보드 행렬 또는 이미터 위치(§3.10). Phase 2 테스트가 velocity는 이미 검증했으므로 시뮬레이션이 아니다 | Phase 2에서 방향/속도 24개 테스트 통과 |
| 파티클이 카메라를 안 따라 돎(납작한 판) | `R_billboard` 계산, 또는 §3.9의 카메라 한 프레임 지연 | |
| 3D에서 페이드가 안 됨(밝기 일정) | **attribute 12**. 셰이더의 `layout(location = 12)` 선언 누락, `SetupInstanceVAO`의 attribute 12 설정, `InstanceData.alpha` 채움 셋 중 하나 | 3C까지 다른 모든 것이 확인된 상태 |
| 겹쳐도 안 밝아짐 | `glBlendFunc(GL_ONE, GL_ONE)` 또는 `glEnable(GL_BLEND)` | |
| 파티클 뒤 물체가 잘림 | `glDepthMask(GL_FALSE)` 누락 | |
| 파티클 그린 뒤 다른 것이 이상 | 상태 원복(§3.6) | 3E의 관문 |
| 사각형 테두리가 보임 | 프래그먼트의 원형 마스크 또는 쿼드 UV(§3.3) | |

---

## 7. 이 Phase가 하지 않는 것

상위 계획서 §5의 목록을 그대로 따르며, 특히:

- **텍스처 로딩** — §2.1 확정대로 절차적 마스크만. `materialId`는 자리만 예약
- **알파 블렌딩 모드** — 가산 혼합만(§2.2). 배치 순서 비결정성 버그를 먼저 고쳐야 한다
- **`sortedBatches` 미사용 버그 수정** — 파티클과 무관한 선존재 버그. 여기서 고치면 기존 불투명 렌더링의 순서가 바뀐다
- **깊이 정렬 / Frustum culling / 인스턴스 버퍼 이중화**
- **파티클 피킹** — 파티클은 ECS 엔티티가 아니라 선택 대상이 아니다
- **에디터 Preview / 예측 수량 경고** — Phase 4

---

## 8. 열린 질문 — ✅ 전부 확정됨 (2026-09-10 검토)

**세 개 모두 권장안대로 확정됐다.** 아래는 결정 근거를 남긴 원문이다.

### ① 시뮬레이션 공간: 월드 vs 로컬 (§3.10)

**권장: 월드 공간**(스폰 시점 이미터 위치를 파티클에 굽는다). §2의 커버 목록 대부분이 "뿜은 자리에 남아야" 자연스럽고, "이동 잔상"은 로컬 공간에서 성립조차 하지 않는다. 다만 캐릭터에 붙어 따라다니는 오라 같은 연출은 로컬이어야 하므로, **어느 쪽을 v1로 볼지**가 결정 사항이다. 둘 다 지원하면 19번째 파라미터가 된다.

### ② 파티클 크기의 기준: 월드 단위 vs 화면 비례

지금 `startSize`/`endSize`는 월드 단위로 해석할 예정이다(카메라에서 멀어지면 작아진다). 화면 비례(거리와 무관하게 일정한 픽셀 크기)로 하려면 정점 셰이더에서 거리 보정이 필요하다. **월드 단위를 권장**한다 — 3D 씬에서 폭발이 멀리 있으면 작아 보이는 게 맞고, 새 파라미터도 필요 없다.

### ③ 3C 단계(블렌딩 없이 불투명으로 먼저 그리기)를 넣을 것인가

넣으면 커밋이 하나 늘고 중간에 "이상하게 보이는" 상태를 한 번 거친다. 대신 **"안 보인다"의 원인을 블렌딩과 분리**할 수 있다. §6 진단표의 가치가 대부분 여기서 나온다. **넣는 쪽을 권장**하지만, 한 번에 가는 쪽을 원하면 3C+3D를 합칠 수 있다.

---

## 9. 실행 결과 — 3A~3E 관문 통과 기록 (2026-09-10)

에디터를 `HAS_ENGINE=True`로 실제 실행하고 화면을 캡처해 확인했다. 검증용으로 `engine/editor/main.py`가 시작 시 `VFX Test` 엔티티(x=-3, 지속 방출)를 하나 만든다 — 원점의 `Test Cube`, x=3의 `Barrel`과 나란히 놓아 **파티클이 그려지는지와 기존 렌더링이 그대로인지를 한 화면에서 같이 본다.**

| 단계 | 관문 | 결과 |
|---|---|---|
| **3A** | `ParticleSystem`이 World에 등록되고 크래시 없이 돈다 | ✅ 로그: `Registering system: ParticleSystem`. 덤으로 `Added dependency: 'ParticleSystem' depends on 'RenderSystem'` — §3.9의 실행 순서가 의존성 분석기에서도 강제됨을 확인 |
| **3B** | 배치가 생성되고 인스턴스가 제출된다 | ✅ 로그: `SetupInstanceVAO - Setup instance VAO with 13 attributes (3 mesh + 10 instance)`, `CreateBatch - Created batch for mesh GUID: 6216753441977484321`(= `0x5646585155414421` "VFXQUAD!"), `ParticleSystem::EnsureBatch - Particle instance batch created`. 16초 동안 `AddInstance` 23,849회. **`instanceVAO is 0` 에러 0건** |
| **3C** | 블렌딩 없이 파티클이 원반으로 보인다 | ✅ x=-3에 불투명 주황 입자 기둥. 원형 마스크(UV) 정상. **그리드·Test Cube·Barrel 변화 없음**(회귀 없음), FPS 55 |
| **3D** | ①페이드 ②겹치면 밝아짐 ③뒤 물체 안 잘림 | ✅ 3C와 같은 위치를 확대 비교: 아래쪽 밀집부가 **노랑/흰색으로 밝아지고**(가산 혼합), 위로 갈수록 **어두워지며 사라지고**(알파 페이드 → **attribute 12가 GPU까지 도달**), 파티클 너머로 **그리드가 비친다**(Z-Write off) |
| **3E** | 상태 원복 / 회귀 | ✅ 파티클을 그린 뒤에도 Test Cube·Barrel·그리드 정상. 렌더 상태 누수 없음 |

> **"크래시 없음"과 "의도대로 그려짐"을 구분해 적는다**(CLAUDE.md 관례 #2): 위 3C/3D/3E는 **의도대로 그려짐**까지 확인한 것이다. 3A/3B는 로그 기반의 객관적 확인이고, 화면 확인은 3C 이후에 이뤄졌다.

### 9.1 도중에 발견한 선존재 버그 — `SetComponentJson`이 조용히 아무 것도 안 했다

3C 검증 중 Inspector에 파티클 설정이 **기본값 그대로** 떠 있는 것을 보고 발견했다. 파티클과 무관한 바인딩 쪽 버그다.

- **증상**: `registry.SetComponentJson(entity, "...", json)`이 예외도 로그도 없이 "성공"하는데 값이 하나도 반영되지 않는다. 에디터가 시작할 때 Main Camera에 넣는 `{"fov":60, ..., "isMainCamera":true}`도 마찬가지로 무시되고 있었다. `add_component`는 따로 성공해서 컴포넌트 자체는 붙어 있고 크래시도 없어, Inspector를 눈으로 보기 전까지는 알 수 없었다.
- **원인**: `ECSBindings.cpp`가 `wrapper[compName] = data`로 JSON을 한 겹 감싸서 `info->deserialize`에 넘기고 있었는데, `deserialize`는 **감싸지 않은 컴포넌트 객체**를 기대한다(`GE_BEGIN_COMPONENT`의 람다가 `data.contains(f.name)`로 필드를 찾으므로, 감싼 JSON에서는 필드가 하나도 매칭되지 않고 for 루프가 전부 헛돈다). `DeserializeRegistry()`와 손으로 등록한 `ScriptComponent`/`AIComponent` 람다도 전부 "안쪽 객체" 규약이라 **이 호출부만 규약을 어기고 있었다.**
- 고친 뒤 파티클 설정과 카메라 설정이 실제로 적용되는 것을 화면에서 확인했다.

### 9.2 남은 관찰 → **Phase 4에서 원인 규명됨**

- 파티클 40개 안팎에서 FPS가 55 → 11로 낮게 나온 캡처가 있었다. 당시에는 다른 앱이 함께 떠 있었고 파티클 수 차이만으로 설명되지 않아 **성능 문제로 단정하지 않았는데**, Phase 4의 극단값 검증에서 실제 원인이 나왔다: **`InstancedBatchManager::AddInstance`가 인스턴스 하나마다 TRACE 로그를 남기고 있었다.** 즉 프레임당 파티클 수만큼 문자열 포맷 + 파일 I/O가 발생했다.
- 같은 조건(파티클 2048개, 16초)에서 `Renderer::BeginFrame` 호출 수로 측정: **로그 제거 전 18프레임(1.1 FPS) → 제거 후 1,106프레임(69.1 FPS), 약 60배.** 상위 계획서 Phase 4 항목 참고.

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- [ROADMAP.md](../ROADMAP.md) §6(2026-08-18) — P0-2 완결, 인스턴싱 경로가 스크린샷으로 검증됨. §5 표 — "검은 화면"의 실제 원인이 Qt 스타일시트였다는 기록
- [renderer/SceneMeshRenderer.h](../engine/renderer/SceneMeshRenderer.h)/[.cpp](../engine/renderer/SceneMeshRenderer.cpp) — 렌더러 클래스 구조, 인라인 셰이더를 쓰는 이유(CWD 문제), `UpdateDynamicBatches()` 호출 위치, `RenderBatches(ForwardOpaque, ...)`
- [renderer/Renderer.cpp](../engine/renderer/Renderer.cpp) — `Renderer::Initialize()`의 서브시스템 생성 순서, `Render()`에서 `sceneMeshRenderer->Render()` 호출 지점과 `RenderMode::Scene` 게이트
- [renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h)/[.cpp](../engine/renderer/InstancedBatchManager.cpp) — `InstancedBatchKey` 필드, `RenderBatches`의 passType 필터, `AddInstance`의 append 동작, `ClearInstances`, `CleanupStaleBatches`, `SetupInstanceVAO`의 attribute 0~12
- [core/Engine.cpp](../engine/core/Engine.cpp) `CreateWorld()` — 모든 World에 시스템을 자동 등록하는 유일한 지점, nullptr 허용 패턴
- [ecs/RenderSystem.h](../engine/ecs/RenderSystem.h)/[.cpp](../engine/ecs/RenderSystem.cpp) — nullptr 허용 생성자, 메시 지연 생성, 매 프레임 `ClearInstances` 후 재수집, `ComposeWorldMatrix`가 순수 함수로 분리된 선례
- [renderer/Vertex.h](../engine/renderer/Vertex.h) — 정점 레이아웃 8 float
- [platform/Win32Platform.cpp](../engine/platform/Win32Platform.cpp) — `GL_DEPTH_TEST`가 전역으로 한 번 켜짐
- [renderer/DebugGridRenderer.cpp](../engine/renderer/DebugGridRenderer.cpp) / [renderer/BoneLineRenderer.cpp](../engine/renderer/BoneLineRenderer.cpp) — `glIsEnabled`로 이전 상태를 읽고 되돌리는 관례
- 저장소 전체 `glEnable(GL_BLEND)|glBlendFunc|glDepthMask` 검색 — **0건**(=이 Phase가 처음 만든다)
