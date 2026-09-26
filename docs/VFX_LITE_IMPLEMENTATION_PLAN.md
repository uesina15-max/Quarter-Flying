# VFX Lite 파티클 시스템 구현 계획서

**작성일**: 2026-09-09
**목표**: [docs/VFX_LITE_PLAN.md](VFX_LITE_PLAN.md)에서 확정한 18개 파라미터짜리 파티클 시스템을, 기존 인스턴싱 렌더링 경로와 프리팹 직렬화 경로 위에 얹어 구현한다. [docs/MASTER_PLAN.md](MASTER_PLAN.md) §3 "물리 & 게임플레이" 행의 "파티클" 항목(Beta 단계)에 해당.

> **선행 문서**: 범위·철학은 [docs/VFX_LITE_PLAN.md](VFX_LITE_PLAN.md)가 확정했고 이 문서는 그것을 바꾸지 않는다. 특히 §2의 18개 파라미터와 §3의 제외 목록은 그대로 따른다. 이 계획서가 새로 하는 일은 **범위 정의서가 "추후 논의"로 남겨둔 §6.1(텍스처)과 §6.3(블렌딩)을 확정하고, Phase로 쪼개는 것**이다.

> **선행 확정 사항**: 이 계획서의 모든 설계 결정은 [docs/VFX_LITE_PLAN.md §7](VFX_LITE_PLAN.md)의 실제 코드 대조 결과 위에 있다. 특히 다음 4가지는 추측이 아니라 소스에서 확인된 사실이다 — (1) `InstanceData`는 96바이트이고 위치/크기/회전이 `mat4`에 들어간다, (2) 저장소에 블렌딩 GL 호출이 0건이다, (3) `FieldType`에 Vec4/Color가 없고 `Enum`은 직렬화되지 않는다, (4) `materialId`로 텍스처를 bind하는 코드가 없다.

---

## 1. 배경 — 지금 있는 것과 없는 것

전부 실측이다(근거는 부록).

**있는 것**
- `InstancedBatchManager` — 배치 생성/인스턴스 추가/배치 렌더링. `RenderSystem`을 통해 실제로 연결되어 동작 중(`SceneMeshRenderer`가 `ForwardOpaque`로 호출).
- `InstanceData.alpha` — **이 계획의 Phase 0으로 이미 추가됨**(§2.3).
- `ComponentRegistry` + `GE_BEGIN_COMPONENT`/`GE_FIELD` 리플렉션 — JSON 직렬화/역직렬화/필드 스키마.
- `PrefabAsset`의 `CaptureFromEntity`/`SpawnInto`/`ApplyToEntity`/Revert — Phase 1~4 완료.
- `Mesh::create(vertices, indices)` — 코드에서 메시를 만들 수 있다(파일 로딩 불필요).
- 에디터 Inspector — 컴포넌트 필드 스키마를 읽어 위젯을 자동 생성.

**없는 것**
- 파티클 관련 코드 일체(`Particle|파티클|VFX` grep 0건 — 범위 정의서 작성 시점과 동일).
- 블렌딩/깊이 쓰기 제어 — `glEnable(GL_BLEND)`·`glBlendFunc`·`glDepthMask` **0건**. `PassType::ForwardTransparent`는 enum 값으로만 존재하고 그리는 호출부가 없다.
- `Texture`/`Material` 클래스와 로딩 경로. `engine/asset/`는 CMake 빌드에는 들어가 있지만 호출부가 없는 고아 모듈.
- 쿼드(Quad) 프리미티브 메시 — `renderer/`에 Quad/Plane 생성 헬퍼가 없다.
- `FieldType::Vec4`/`Color`, 그리고 `FieldType::Enum`의 직렬화 case.

---

## 2. 설계 결정

### 2.1 §6.1 확정 — v1은 텍스처를 로딩하지 않는다 (절차적 원형 마스크)

**확정**: VFX Lite v1은 **텍스처 파일을 읽지 않는다.** 파티클 스프라이트 모양은 파티클 셰이더 안에서 쿼드의 UV로 절차적으로 만든다:

```glsl
// 파티클 프래그먼트 셰이더 (개념)
float d    = length(vTexCoord - vec2(0.5));
float mask = 1.0 - smoothstep(0.35, 0.5, d);   // 부드러운 원형
FragColor  = vec4(vColor.rgb, vColor.a * mask);
```

범위 정의서 §6.1은 "고아 모듈을 살릴지, VFX 전용 최소 로더를 만들지"의 두 선택지를 놓고 후자로 기울어 있었다. **여기서는 "지금은 둘 다 하지 않는다"로 확정한다.** 근거:

- §6.1 자신이 경고한 함정("파티클 시스템이 조용히 텍스처 파이프라인 프로젝트로 번진다")을 피하는 가장 확실한 방법은 최소 로더조차 만들지 않는 것이다. 최소 로더도 결국 파일 포맷 결정 → 디코더 선택 → 밉맵/필터링 → 수명 관리로 이어진다.
- §2의 커버 목록(불, 연기, 먼지, 피격, 마법, 폭발, 눈/비, 스파크, 픽업)은 대부분 **부드러운 원형 점 + 색상/알파/크기 변화**로 표현된다. 텍스처가 실제로 필요한 건 나뭇잎·파편처럼 고유한 실루엣이 있는 것들인데, 그건 §3에서 이미 제외한 Mesh Particle 쪽에 가깝다.
- 절차적 마스크는 셰이더 두 줄이라 **버릴 때 비용이 0**이다. 나중에 텍스처가 들어오면 `mask`를 `texture(uSprite, vTexCoord).a`로 바꾸는 것으로 끝난다.

**나중을 위해 지금 지켜둘 것**(§6.1이 요구한 상호운용성):
- `InstancedBatchKey.materialId`는 파티클 전용 상수 하나로 **채워두되 의미를 부여하지 않는다**. 텍스처가 생기면 이 자리에 텍스처 핸들을 넣는다 — 배치 키 구조도, 배치 분리 규칙도 그때 바뀌지 않는다.
- §7.4에서 확인했듯 **`materialId`로 텍스처를 bind하는 코드가 지금 없다.** 텍스처를 도입하는 작업의 실제 단위는 "로더"가 아니라 "배치별 텍스처 바인딩 훅"이라는 점을 여기 적어둔다.

### 2.2 §6.3 확정 — 렌더 패스를 리팩터링하지 않는다. 파티클 전용 상태 전환만 만든다

**확정 ①: 블렌드 상태는 `InstancedBatchManager`가 아니라 파티클 렌더 경로가 관리한다.**

`RenderBatch()`/`RenderBatches()`에 passType별 상태 설정을 넣으면 기존 `ForwardOpaque` 경로의 GL 상태까지 건드리게 된다 — 지금 잘 돌고 있는 유일한 렌더 경로다. 대신 `ParticleRenderer`가 자기 draw 앞뒤로만 상태를 바꾸고 **반드시 원복**한다:

```text
ParticleRenderer::Render()
  glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);   // Additive
  glDepthMask(GL_FALSE);                             // Z-Test 유지, Z-Write만 끔
      batchManager.RenderBatches(PassType::ForwardTransparent, particleShader);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
```

**확정 ②: v1의 블렌드 모드는 가산 혼합(Additive) 하나로 고정한다.**

이것은 게으름이 아니라 **아직 고치지 않은 버그를 건드리지 않기 위한 방어선**이다. §7.4에서 확인했듯 `RenderBatches()`는 정렬 결과(`sortedBatches`)를 쓰지 않고 `unordered_map`을 순회하므로 **배치 사이의 그리는 순서가 비결정적**이다. 가산 혼합은 교환법칙이 성립해 순서와 무관하게 같은 결과가 나오므로, 이 문제를 **회피**할 수 있다. 반대로 알파 블렌딩을 넣는 순간 순서 문제가 실제 증상으로 드러나므로, 그건 순서 버그를 먼저 고친 뒤의 일이다(§5).

이 확정 덕분에 **§2의 18개 파라미터는 그대로 유지된다** — 블렌드 모드가 19번째 값이 되지 않는다.

**확정 ③: `ForwardTransparent`를 그리는 호출부는 파티클 것 하나만 만든다.** 렌더러 전체의 불투명/투명 패스 구조를 정리하는 일은 이 계획의 범위가 아니다. 순서는 호출 순서로만 보장한다 — 기존 불투명 렌더링을 먼저, 파티클을 그 다음.

### 2.3 `InstanceData` 알파 확장 — Phase 0으로 완료됨

[docs/VFX_LITE_PLAN.md §7.2](VFX_LITE_PLAN.md)의 확정안을 그대로 반영했다. 기존 `_padding[1]`(offset 92)을 `float alpha{1.0f}`로 바꾸고 VAO attribute 12를 추가했다. 상세 근거는 §7.2에 있고, 여기서는 결과만 적는다:

- `sizeof(InstanceData)`는 96으로 불변, 앞 필드 offset 전부 불변 → 기존 attribute 3~11과 기존 셰이더 무영향.
- 기본값 1.0f 덕에 `RenderSystem.cpp`의 기존 `InstanceData data{};`가 알파를 몰라도 불투명으로 동작.
- 레이아웃 회귀 방지용 `static_assert` 3종(sizeof==96, offsetof(color)==64, offsetof(alpha)==92) 추가.
- `SetupInstanceVAO()`가 실제로 쓰는 attribute 수와 능력 검사가 어긋나 있던 것(검사는 7, 실사용은 12)을 `kRequiredVertexAttribs = 13` 상수 하나로 통일.

> **검증 단계**(CLAUDE.md 관례 #2): **컴파일/링크 성공까지 확인됨** — `cmake --build . --config Release --target quarterflying_engine` exit 0, `quarterflying_engine.dll` 생성. 위 `static_assert` 3개가 통과했다는 것은 구조체 레이아웃이 설계대로라는 컴파일 타임 증거다. **실행 검증은 아직 하지 않았다** — 기존 불투명 렌더링이 확장 전과 동일하게 보이는지, attribute 12가 실제로 셰이더에 도달하는지는 Phase 3에서 확인한다.

### 2.4 `ParticleEffectComponent` — 18개 파라미터 → 20개 리플렉션 필드

`FieldType`에 Vec4/Color가 없으므로 색상은 `Vec3` + `Float`로 쪼갠다([§7.3](VFX_LITE_PLAN.md)). 그 결과 **파라미터 개수(18)와 리플렉션 필드 개수(20)가 다르다** — 이 문서가 숫자 오류에 민감했던 전례(15→18 정정)가 있으므로 차이를 표로 못박아둔다.

| 범위 정의서 §2의 값 | 리플렉션 필드 | `FieldType` |
|---|---|---|
| Spawn Rate | `spawnRate` | Float |
| Max Particle | `maxParticle` | Int |
| Lifetime | `lifetime` | Float |
| Burst | `burst` | Int |
| Initial Speed | `initialSpeed` | Float |
| **Direction**(기준 방향 + 퍼짐) | `directionBase` / `directionSpread` | Vec3 / Float |
| Gravity | `gravity` | Float |
| Drag | `drag` | Float |
| Start Size | `startSize` | Float |
| End Size | `endSize` | Float |
| Start Color | `startColor` | Vec3 |
| End Color | `endColor` | Vec3 |
| **Alpha Fade** | `startAlpha` / `endAlpha` | Float / Float |
| Start Rotation | `startRotation` | Float |
| Rotation Speed | `rotationSpeed` | Float |
| Speed ± | `speedVariance` | Float |
| Size ± | `sizeVariance` | Float |
| Rotation ± | `rotationVariance` | Float |
| **합계 18개 값** | **20개 필드** | |

- 필드가 2개인 두 항목(Direction, Alpha Fade)은 범위 정의서 §2.2/§2.4가 이미 "하나의 값 안에서" 표현한다고 정한 것이라, **파라미터를 늘린 게 아니다.**
- **`FieldType::Enum`은 쓰지 않는다** — 직렬화 switch에 case가 없어 값이 조용히 누락된다([§7.3](VFX_LITE_PLAN.md)). §2.2에서 블렌드 모드를 파라미터로 만들지 않기로 한 덕에 enum이 필요한 필드가 하나도 없다.
- 등록 위치는 `engine/ecs/Reflection.cpp` — `TransformComponent`/`AIComponent`/`PrefabInstanceComponent`와 같은 자리(프리팹 계획서가 정한 관례를 그대로 따른다).

### 2.5 자료구조 — `Particle` / `EffectInstance` / `ParticleSystem`

```cpp
// 56 bytes. docs/VFX_LITE_PLAN.md §7.4 - 64바이트로 패딩하지 않는다.
// (56 stride면 파티클 8개가 정확히 7개 캐시라인에 들어간다. 64로 패딩하면 8라인이 되어
//  오히려 12.5% 더 읽는다. "라인 경계를 걸치니까 정렬하자"는 직관이 여기서는 틀렸다.)
struct Particle {
    glm::vec3 position;     // 12
    glm::vec3 velocity;     // 12
    float     lifetime;     //  4  남은 수명
    float     sizeScale;    //  4  Size ±를 반영한 이 파티클 고유의 크기 배율
    float     size;         //  4  보간된 현재 값
    float     rotation;     //  4
    glm::vec4 color;        // 16  보간된 현재 rgb + a
};

struct EffectInstance {
    EntityId              owner;
    ParticleEffectComponent settings;  // 스폰 시점 스냅샷
    std::vector<Particle> particles;   // 생성 시 reserve(maxParticle) 1회
    float                 spawnAccumulator{0.0f};
    bool                  active{true};
    std::mt19937          rng;
};
```

> **개정(2026-09-09, Phase 2 구현 중)**: 범위 정의서 §4.1의 자료구조 스케치와 **두 곳이 다르다.** 둘 다 스케치를 그대로 코드로 옮기면 조용히 잘못 동작하는 지점이었다.
>
> **① `maxLifetime` → `sizeScale`.** §2.6의 랜덤 파라미터는 Speed/Size/Rotation 세 개뿐이고 **"Lifetime ±"는 범위에 없다** — 따라서 모든 파티클의 최대 수명은 `EffectInstance`의 `settings.lifetime`과 항상 같고, 파티클마다 복사해 두는 것은 56바이트 중 4바이트를 낭비하는 것이다. 반대로 **Size ±는 파티클마다 달라야 하는데 저장할 자리가 없었다**: 크기는 매 프레임 `Lerp(startSize, endSize, t)`로 새로 계산되므로, 파티클 고유의 배율을 들고 있지 않으면 스폰 때 준 변동폭이 다음 프레임에 덮어써진다. 컴파일도 되고 크래시도 안 나면서 **"Size ±만 조용히 안 먹는"** 버그가 됐을 자리다. 낭비되는 4바이트를 필요한 4바이트로 맞바꿨고, **56바이트와 §7.4의 캐시라인 계산은 그대로 유지된다.**
>
> **② `velocity *= drag` → dt 비례 감쇠.** §4.1의 스케치를 문자 그대로 쓰면 `drag`의 기본값 0.1에서 매 프레임 속도의 90%가 사라지고, **`drag=0`이면 파티클이 즉시 멈춘다** — §2.2가 정의한 "속도 감쇠 계수"와 정반대다. 게다가 프레임률에 따라 결과가 달라진다. 구현은 `velocity *= clamp(1 - drag*dt, 0, 1)`로 한다: `drag=0`이면 감쇠 없음, `drag=0.1`이면 초당 10% 감쇠.

- **파티클은 ECS 엔티티가 아니다**(§5.1). `ParticleEffectComponent`만 ECS에 있고, `EffectInstance`는 `ParticleSystem`이 소유한다.
- `ParticleSystem : System`은 `RenderSystem`과 같은 자리에 둔다.
- **난수는 `EffectInstance`마다 `std::mt19937` 하나**를 갖는다. 전역 `rand()`를 쓰지 않는다 — 이펙트별 독립성과 스레드 안전성 때문이다. v1은 시드를 저장하지 않는다(재현 가능한 이펙트는 §5로 미룸).

### 2.6 배치 구성 — 엔진 전체에 파티클 배치 **하나**

이펙트마다 배치를 만들지 않는다. 모든 파티클이 같은 쿼드 메시 / 같은 셰이더 / 같은 블렌드 모드를 쓰므로 `InstancedBatchKey`가 하나면 충분하고, 그러면 **Draw Call이 이펙트 개수와 무관하게 1개**가 된다(§5.6이 목표한 것의 최대치).

```text
key = { meshGuid   = kParticleQuadGuid   (파티클 전용 상수)
        materialId = kParticleMaterialId (§2.1 - 지금은 의미 없는 상수, 텍스처 자리 예약)
        shaderId   = 파티클 셰이더
        passType   = ForwardTransparent
        materialLayout = Unlit
        features   = None }
```

매 프레임 순서:

```text
ClearInstances(key)                     # AddInstance는 중복 방지가 없어 안 비우면 누적된다
  → 모든 활성 EffectInstance의 살아있는 파티클마다 AddInstance(key, data, 0)
  → ParticleRenderer가 블렌드 상태 켜고 RenderBatches(ForwardTransparent, shader)
```

- 쿼드 메시는 `Mesh::create()`로 코드에서 1회 생성한다(vertex 4 / index 6). 파일에서 로드하지 않는다 — 로더가 없기도 하고, 만들 이유도 없다.
- `AddInstance`가 호출마다 `visibleCount = instanceData.size()`로 갱신하므로 가시성 처리를 따로 하지 않아도 된다(frustum culling은 §5.9대로 v1 범위 밖).
- **`CleanupStaleBatches()`를 부르지 않는다.** 인스턴스가 빈 배치를 제거하기 때문에, 파티클이 잠시 0개가 되면 VAO/VBO가 파괴되고 다음 Burst에서 재생성된다 — §5.5가 금지한 재할당이 GPU 자원으로 옮겨간 형태다([§7.4](VFX_LITE_PLAN.md)). 현재 이 함수의 호출부가 없으므로 "계속 부르지 않는다"를 유지하면 된다.

### 2.7 인스턴스 데이터 조립 — 파티클마다 `mat4`

`InstanceData`가 위치/크기/회전을 개별 값으로 받지 않으므로([§7.1](VFX_LITE_PLAN.md)) 파티클마다 행렬을 만들어야 한다. 빌보드(카메라를 향하는 회전)까지 포함하면:

```text
model = T(position) * R_billboard * R_z(rotation) * S(size)
color = particle.color.rgb
alpha = particle.color.a          # §2.3에서 추가한 attribute 12
```

`R_billboard`는 뷰 행렬의 회전 부분을 전치한 것으로 프레임당 1회만 계산해 모든 파티클이 재사용한다 — 파티클마다 다시 구하지 않는다. CPU 쪽 인스턴스 임시 배열도 매 프레임 새로 만들지 않고 재사용 버퍼로 둔다(§5.6 마지막 문단).

### 2.8 Spawn 누적 — 소수부를 버리지 않되, 상한에 걸린 분은 버린다

```text
spawnAccumulator += spawnRate * dt
n = floor(spawnAccumulator)
spawnAccumulator -= n
for i in 0..n:
    if aliveCount >= maxParticle: break     # §5.2 신규 생성 거부
    SpawnOne()
```

두 가지가 의도적이다:
- **소수부를 누적한다** — dt가 작을 때 `spawnRate * dt < 1`이라고 매 프레임 0개를 만들면 방출이 아예 멈춘다.
- **상한에 걸려 못 만든 분은 누적하지 않는다**(`break` 후 accumulator를 늘리지 않음) — 안 그러면 파티클이 죽어 자리가 나는 순간 밀린 물량이 한꺼번에 터진다. §5.2가 원한 것은 "초과분은 그냥 생성되지 않음"이다.

### 2.9 이펙트 활성/비활성 — Update 자체를 건너뛴다

`aliveCount == 0 && spawnRate <= 0 && burst 소진` 이면 `active = false`로 두고 시스템 순회에서 제외한다(§5.8/§5.11). Burst 이펙트는 마지막 파티클이 죽는 순간 자동으로 여기 걸린다. **이펙트 자체의 Lifetime 파라미터는 만들지 않는다.**

전역 상한 `Max Active Effects` / `Max Total Particles`는 `ParticleSystem`의 엔진 정책 상수로 두고 **에디터에 노출하지 않는다**(§5.7).

### 2.10 에디터 — 예측 수량 표시와 Preview 하드 상한

- `Average = spawnRate × lifetime`, `Peak = Average + burst`를 폼 옆에 표시한다. **Burst는 감쇠하므로 정상상태 평균에 기여하지 않는다** — 외부 검토 보고서 §4.3이 둘을 합쳐 "평균"이라고 부른 것은 정정된 상태다([§7 배너](VFX_LITE_PLAN.md)).
- `maxParticle`이 `Peak` 대비 지나치게 크거나 작으면 경고를 띄운다(§5.3).
- **Preview에는 2048 하드 상한**을 별도로 건다 — 사용자가 넣은 `maxParticle`이 더 커도 Preview는 여기서 멈춘다(§5.10). 에디터가 죽으면 디버깅조차 못 하기 때문이다.

---

## 3. Phase 분해

각 Phase는 CLAUDE.md 관례 #2에 따라 **컴파일 / 유닛테스트 / 실제 실행 검증**을 구분해 기록한다.

### Phase 0 — `InstanceData` 알파 확장 — ✅ 컴파일 검증 완료 (2026-09-09)

§2.3 참조. 변경 파일: `renderer/InstancedBatchManager.h`, `renderer/InstancedBatchManager.cpp`.

- [x] `_padding[1]` → `float alpha{1.0f}`, 레이아웃 `static_assert` 3종 추가
- [x] VAO attribute 12 추가, `kRequiredVertexAttribs = 13` 상수로 능력 검사 통일
- [x] **컴파일/링크 성공** (Release, exit 0)
- [ ] 실제 실행 검증 — 기존 불투명 렌더링 무변화 확인 (Phase 3에서 함께)

### Phase 1 — `ParticleEffectComponent` + 리플렉션 등록 — ✅ 유닛테스트 통과 (2026-09-09)

§2.4의 20개 필드를 POD 구조체로 만들고 `GE_BEGIN_COMPONENT`/`GE_FIELD`로 등록한다. 이 Phase는 렌더링·시뮬레이션과 무관하므로 **엔진 실행 없이 검증이 끝난다.**

- [x] `ecs/Components.h`에 구조체, `ecs/Reflection.cpp`에 `RegisterParticleComponentsReflection()`으로 등록
- [x] **유닛테스트 8개 통과** (`tests/ParticleEffectComponentTests.cpp`) — 등록 여부 / 필드 20개 / 라운드트립(20개 필드 전부) / `Vec3` 필드가 3원소 배열로 직렬화 / 부분 JSON 역직렬화 시 나머지 필드가 기본값 유지 / `patchField`(Float·Vec3)
- [x] **회귀 게이트** — 컴포넌트가 없는 엔티티의 직렬화 결과에 `ParticleEffectComponent`가 나타나지 않는지
- [x] **Enum 회귀 가드** — 어떤 필드도 `FieldType::Enum`을 쓰지 않는지(§7.3의 "조용히 누락되는" 구멍에 대한 방어)

> **검증 단계**(CLAUDE.md 관례 #2): **단위 테스트 통과**까지. 격리된 직렬화 로직은 검증됐지만 **실제 에디터/엔진에 연결해 돌려본 적은 없다** — Inspector에 20개 필드가 실제로 뜨는지는 Phase 4, 값이 시뮬레이션을 구동하는지는 Phase 2/3에서 확인한다.
>
> 라운드트립 테스트는 기본값과 **전부 다른 값**으로 채워서 돌린다 — 기본 생성자가 우연히 같은 값을 넣어줘서 "역직렬화가 아무 일도 안 해도 통과"하는 것을 막기 위해서다. 복원도 원본과 다른 엔티티에 한다(같은 엔티티에 덮어쓰면 값이 그대로 남아 통과해버린다).
>
> 전체 스위트는 419개 중 414 PASS / 5 FAIL이며, 그 5개(`RendererInstancingTest` 3개, `SystemConcurrencyPropertyTest` 계열)는 **선존재 실패**다 — `InstancedBatchManager.cpp`를 변경 전 버전으로 되돌려 다시 빌드·실행해도 동일하게 실패하는 것을 확인했다. `RendererInstancing`은 GL 컨텍스트 없이 `Renderer::Initialize()`를 부르다 `SetUp()`에서 `0xc0000005`로 죽고, `SystemConcurrency`는 타이밍 계측 값이 0으로 남는 문제다.

### Phase 2 — 시뮬레이션 (`Particle`/`EffectInstance`/`ParticleSystem`) — ✅ 유닛테스트 통과 (2026-09-09)

§2.5/§2.8/§2.9. 화면에 아무것도 그리지 않고 배열 상태만 다룬다. 파일: `ecs/ParticleSystem.h`, `ecs/ParticleSystem.cpp`.

- [x] `SyncEffectsWithRegistry()` / `SpawnParticles()` / `IntegrateAndRemoveDead()`(swap-remove) / `UpdateActiveState()`
- [x] **유닛테스트 24개 통과** (`tests/ParticleSystemTests.cpp`):
  - `reserve(maxParticle)` 이후 200프레임 동안 `particles.capacity()`가 변하지 않는다(런타임 재할당 없음, §5.5)
  - `aliveCount`가 `maxParticle`을 **절대** 넘지 않는다 / Burst가 상한보다 커도 클램프된다(§5.2)
  - 상한에 걸린 뒤 파티클이 죽어 자리가 나도 **밀린 물량이 한꺼번에 생성되지 않는다**(§2.8)
  - 살아있는 파티클이 상한 도달 때문에 **조기 제거되지 않는다**(§5.2의 "순환 버퍼 금지")
  - Burst 이펙트가 마지막 파티클 사망 시 `active == false` / 지속형은 계속 활성 / Burst는 한 번만(§5.8·§5.11)
  - `rate*dt < 1`일 때 소수부 누적으로 방출이 멈추지 않는다(§2.8)
  - 중력 적분 / drag 감쇠(및 `drag=0`에서 멈추지 않음) / `size`·`color`가 `t = 1 - life/maxLife`로 보간(§4.1)
  - **Size ±가 매 프레임 보간에 덮어써지지 않는다** — 위 개정 ①이 막은 버그의 회귀 가드
  - Direction: `spread=0`이면 기준 방향 정확히, `spread=360`이면 전방위로 흩어지되 속력 보존(§2.2)
  - 전역 상한 `Max Active Effects` / `Max Total Particles`(§5.7)
  - 엣지: `maxParticle<=0`, `dt=0`, 영벡터 Direction, 다중 이펙트 독립성
- [x] `sizeof(Particle) == 56` `static_assert` + 동명의 테스트 — §7.4의 "패딩하지 않는다" 결정이 조용히 뒤집히지 않도록

> **검증 단계**(CLAUDE.md 관례 #2): **단위 테스트 통과**까지. 시뮬레이션 로직은 격리 상태로 검증됐지만 **실제 엔진/에디터에 연결해 돌려본 적은 없다** — `ParticleSystem`을 `World`에 등록해 매 프레임 도는지, 파티클이 화면에 보이는지는 Phase 3/4에서 확인한다.
>
> 전체 스위트 443개 중 438 PASS / 5 FAIL이며, 그 5개는 Phase 1에서 확인한 것과 **동일한 선존재 실패**다(`RendererInstancingTest` 1개 + `SystemConcurrencyPropertyTest` 4개). 신규 실패는 없다.

### Phase 3 — 렌더링 (쿼드 메시 + 파티클 셰이더 + 투명 상태 + 배치 연결) — ✅ 실제 실행 검증 완료 (2026-09-10)

> **세부 계획서 별도**: 이 Phase는 [docs/VFX_LITE_PHASE3_RENDERING_PLAN.md](VFX_LITE_PHASE3_RENDERING_PLAN.md)에 착수 단위(3A~3E)와 단계별 관문, 실패 모드별 진단표까지 쪼개 두었다. **3A~3E 전부 완료**되었고 관문 통과 증거는 그 문서 §9에 있다. 열린 질문 3개는 ① 월드 공간 시뮬레이션, ② 월드 단위 크기, ③ 3C 단계 포함으로 확정됐다.
>
> **검증 단계**(CLAUDE.md 관례 #2): **실제 실행 검증**까지. 에디터를 `HAS_ENGINE=True`로 띄워 파티클이 화면에 그려지고, 알파 페이드·가산 혼합·Z-Write off가 의도대로 동작하며, 기존 그리드/Test Cube/Barrel 렌더링에 회귀가 없음을 화면 캡처로 확인했다. 도중에 파티클과 무관한 선존재 버그(`SetComponentJson`이 조용히 아무 것도 안 하던 것)를 하나 발견해 함께 고쳤다(§9.1).
>
> 신규 파일: `renderer/ParticleRenderer.h/.cpp`. 수정: `ecs/ParticleSystem.h/.cpp`(인스턴스 수집 + 월드 공간 스폰), `core/Engine.cpp`(시스템 등록), `renderer/Renderer.h/.cpp`(훅), `bindings/ECSBindings.cpp`(위 버그 수정), `editor/main.py`(검증용 엔티티).

§2.1/§2.2/§2.6/§2.7. **여기가 이 계획에서 가장 위험한 Phase다** — 새 렌더 상태 전환을 만들고, Phase 0의 attribute 12를 처음 실제로 소비하며, Phase 2가 만들었지만 아직 아무도 호출하지 않는 `ParticleSystem`을 엔진에 연결하는 세 가지가 겹치기 때문이다.

> **착수 전 확인된 좋은 소식**: 인스턴싱 렌더 경로 자체는 `ROADMAP.md` §6(2026-08-18, P0-2 완결)에서 **스크린샷으로 검증**됐다 — `RenderSystem` → `InstancedBatchManager` → `SceneMeshRenderer` → 화면 경로와 attribute 3~7이 실제로 동작한다. `CLAUDE.md`의 "검은 화면"·"`InstanceData` 레이아웃 미검증"은 그보다 이른 2026-08-10 기록이라 이미 해소된 상태다. 따라서 이 Phase에서 의심할 것은 **새로 추가되는 것들로 좁혀진다.**

- 쿼드 메시 생성(코드), 파티클 셰이더(vertex: 빌보드 + location 12 알파 / fragment: 절차적 원형 마스크).
- `ParticleRenderer` — 블렌드/깊이 상태 설정과 원복, `RenderBatches(ForwardTransparent, shader)` 호출.
- 인스턴스 조립 버퍼 재사용.
- **실제 실행 검증**(이 Phase는 유닛테스트로 대체 불가):
  1. Phase 0 회귀 — 기존 Scene Editor 뷰포트의 불투명 렌더링이 알파 확장 **전과 동일**한가.
  2. 파티클이 화면에 보이는가. ("크래시 없음"과 "의도대로 그려짐"은 다른 주장이므로 따로 적는다.)
  3. 알파 페이드가 실제로 변하는가 — attribute 12가 셰이더까지 도달했다는 증거.
  4. 파티클을 그린 **뒤** 불투명 물체가 정상적으로 그려지는가(상태 원복 확인).
  5. `RendererStats.drawCalls`가 이펙트 개수와 무관하게 파티클 배치 1개인가(§5.6).

> **여기서 의심할 것**: 증상이 "GL 에러는 없는데 화면에 아무것도 안 보임"이라면 `Mesh::drawInstanced`/`RenderBatch`의 VAO 바인딩 계열 버그와 같은 유형을 먼저 의심한다(CLAUDE.md "알려진 미검증 항목" — 인스턴스 렌더링의 GPU 레이아웃은 아직 화면으로 검증된 적이 없다). Phase 0의 `static_assert`는 CPU 쪽 레이아웃만 보장하지, 셰이더가 그 offset을 같게 해석하는지까지 보장하지는 않는다.

### Phase 4 — 에디터 통합 (Inspector 폼 + 예측 수량 + Preview 상한) — ✅ 실제 실행 검증 완료 (2026-09-10)

§2.10. Inspector가 필드 스키마로 위젯을 자동 생성하므로 **20개 필드 입력 폼은 사실상 공짜**로 나왔다(Phase 3 스크린샷에서 이미 확인). 추가로 만든 것:

- [x] **Average/Peak 계산 + 경고 라벨** (`editor/panels/inspector.py`) — `Average = Rate × Lifetime`, `Peak = Average + Burst`. Burst는 한 번 터지고 사라지므로 **정상상태 평균에는 기여하지 않고 피크에만 더한다**(§7의 정정 반영). 경고 3종: Peak가 Max를 넘음(잘림) / Max가 Peak 대비 과도함(메모리 낭비) / Preview 상한 초과.
- [x] **Preview 하드 상한 2048** (`ParticleSystem::kPreviewMaxParticle`) — 스폰뿐 아니라 **`reserve()` 용량에도** 적용한다. 상한만 스폰에 걸면 Max Particle=100000짜리 오설정이 실제로는 2048개만 쓰면서 100000개 분량(약 5.6MB)을 미리 잡는다. 상한이 실제로 걸리면 **이펙트당 한 번 경고 로그**를 남긴다 — 조용히 다르게 동작하지 않도록.
- [x] 유닛테스트 3개 추가(상한이 이펙트 하나를 제한 / `reserve` 용량도 제한 / 상한 이하 설정은 무영향) → `ParticleSystemTest` 총 **30개 통과**.
- [x] Inspector 라벨 로직을 엔진 없이 검증하는 스크립트로 5개 케이스 확인(정상/잘림/과도/상한초과/Max 0). `Average=Rate×Lifetime`, `Peak=Average+Burst` 단언 통과.

> **검증 단계**(CLAUDE.md 관례 #2): **실제 실행 검증**까지. 극단값(`Rate=10000 / Lifetime=10 / Max=100000`)을 넣고 에디터를 띄워 **멈추지도 죽지도 않는 것**과 상한 경고 로그가 정확히 한 번 뜨는 것을 확인했다.

> **여기서 발견한 성능 버그** — 위 극단값 검증이 아니었으면 못 찾았을 것이다. `InstancedBatchManager::AddInstance`가 **인스턴스 하나마다 TRACE 로그**를 남기고 있었다. 엔티티가 몇 개뿐인 `RenderSystem`에서는 티가 안 났지만, 파티클이 붙으면서 **프레임당 파티클 수만큼** 로그가 쏟아졌다. 같은 조건(파티클 2048개, 16초)에서 측정한 결과 — **로그 제거 전 18프레임(1.1 FPS) → 제거 후 1,106프레임(69.1 FPS), 약 60배.** 인스턴스 단위 추적은 `RendererStats.totalInstances`로 볼 수 있으므로 이 로그는 되살리지 않는다.

### Phase 5 — 프리팹 연동 확인 — ✅ 실제 실행 검증 완료 (2026-09-10)

§4.4대로 "이펙트 하나 = 프리팹 하나"이므로 `CaptureFromEntity`/`SpawnInto`/`ApplyToEntity`/Revert가 그대로 동작해야 한다. **이 Phase의 목적은 구현이 아니라 "정말 공짜로 따라오는지" 확인**이었다.

> **결과: 프로덕션 코드를 한 줄도 고치지 않고 전부 동작했다.** §4.4의 주장이 그대로 성립한다 — 새 `.vfx.json` 포맷도, 별도 캡처/스폰 경로도 필요 없었다. 추가된 것은 검증 테스트와 예제 프리팹 자산뿐이다.

- [x] **캡처 → 스폰**: 20개 필드 전부 보존(`tests/ParticlePrefabTests.cpp`)
- [x] **디스크 왕복**: 기존 `*.prefab.json` 그대로 저장/로드 후 스폰해도 값 동일
- [x] **Revert**: 인스턴스에서 값을 여러 개 바꾼 뒤 `ApplyToEntity`로 프리팹 값 복원
- [x] **§4.4의 계층 구분 확인**: 프리팹은 컴포넌트 값만 복원하고 시뮬레이션을 전혀 모르는데도, 스폰된 엔티티가 손으로 만든 엔티티와 **똑같이** `ParticleSystem`에 잡히고 같은 개수의 파티클을 뿜는다. 프리팹이 `TransformComponent`도 복원하므로 월드 공간 스폰 위치(Phase 3 §3.10)까지 일치한다.
- [x] `PrefabInstanceComponent`가 캡처 결과에 섞이지 않고, 캡처가 원본을 인스턴스로 만들지도 않는다
- [x] **유닛테스트 5개 통과** + **실제 실행 검증**: `engine/assets/prefabs/Fire.prefab.json`을 새로 만들고 에디터가 시작 시 x=6에 인스턴스화한다. 손으로 만든 `VFX Test`(x=-3)와 **나란히 놓아 한 화면에서 비교** — 둘 다 화염으로 그려지고, 프리팹 쪽은 자기 설정값(gravity 0.4 / drag 0.2 / spread 35)대로 모양이 달라 **기본값이 아니라 프리팹 값이 실제로 쓰였음**을 보여준다. Prefabs 패널에도 `Fire`가 등록된다.
  - 이 구성은 `Test Cube`(수동) 옆에 `Barrel`(프리팹)을 놓아 비교했던 기존 관례를 그대로 따른 것이다.

> **검증 단계**(CLAUDE.md 관례 #2): **실제 실행 검증**까지. 다만 Revert는 유닛테스트(`ApplyToEntity`)로만 확인했고, **에디터 Inspector의 Revert 버튼을 눌러본 것은 아니다** — 그 UI 경로는 프리팹 Phase 4에서 이미 라이브 검증된 공용 경로이고 파티클이라고 달라질 지점이 없다.

---

## 4. 진행 순서 요약

```text
Phase 0  InstanceData.alpha            ✅ 컴파일 + static_assert (2026-09-09)
   ↓
Phase 1  ParticleEffectComponent       ✅ 유닛테스트 8개  (2026-09-09)
   ↓
Phase 2  시뮬레이션 루프                ✅ 유닛테스트 30개 (2026-09-09)
   ↓
Phase 3  렌더링                        ✅ 화면 검증 3A~3E (2026-09-10)
   ↓
Phase 4  에디터 폼/Preview             ✅ 화면 검증        (2026-09-10)
   ↓
Phase 5  프리팹 연동 확인               ✅ 유닛테스트 5개 + 화면 검증 (2026-09-10)
```

> **VFX Lite v1 완료 (2026-09-10).** Phase 0~5 전부 끝났고 각 Phase의 검증 단계는 위 절들에 개별 기록되어 있다. 남은 것은 §5의 "명시적으로 미루는 것" 목록뿐이다.

Phase 1과 2는 렌더링 없이 검증이 끝나므로, Phase 3의 렌더링 문제와 시뮬레이션 문제가 **섞이지 않는다**. 이 순서의 목적이 그것이다 — Phase 3에서 화면이 이상하면 원인이 렌더링 쪽임을 이미 알고 시작할 수 있다.

---

## 5. 이 계획이 명시적으로 미루는 것

범위 정의서 §3의 제외 목록(Collision, Sub-emitter, Noise, Trail, Mesh Particle, Texture Sheet, Curve Editor, GPU Particle, 파티클 간 상호작용, 이벤트)은 그대로 유지된다. 그 외에 **이 계획서가 추가로 미루는 것**:

| 미루는 것 | 이유 | 선행 조건 |
|---|---|---|
| 텍스처 로딩 | §2.1 — 파이프라인 프로젝트로 번지는 것을 막는다 | 절차적 마스크로 부족한 이펙트가 실제로 생겼을 때. 작업 단위는 "로더"가 아니라 "배치별 텍스처 바인딩 훅" |
| 알파 블렌딩 모드 | §2.2 — 배치 순서가 비결정적이라 결과가 불안정하다 | `RenderBatches()`가 `sortedBatches`를 실제로 쓰도록 고치는 것이 **먼저** |
| `sortedBatches` 미사용 버그 수정 | 파티클과 무관한 선존재 버그. 여기서 고치면 기존 불투명 렌더링의 순서가 바뀐다 | 별도 이슈로 분리 |
| 깊이 정렬 | §3.3 — Additive는 순서 무관이므로 v1에 필요 없다 | 알파 블렌딩 도입 시 함께 |
| Frustum/Effect culling | §5.9 — 우선순위 6위 밖 | 상한만으로 프레임이 안 나올 때 |
| 인스턴스 버퍼 이중화 | 측정된 스톨이 없는 선제 최적화. §5 철학과 어긋난다 | 실제로 업로드 스톨이 프로파일에 잡혔을 때 |
| 재현 가능한 이펙트(시드 저장) | v1에 필요 없다 | 리플레이/결정적 시뮬레이션이 필요해질 때 |
| `FieldType::Enum` 직렬화 구멍 메우기 | §2.4에서 enum 필드를 안 쓰기로 해서 이 계획은 영향받지 않는다 | enum 필드가 실제로 필요해질 때 |

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- [docs/VFX_LITE_PLAN.md](VFX_LITE_PLAN.md) §2/§3/§5 — 18개 파라미터, 제외 목록, 성능 철학. §7 — 실제 코드 대조 결과(이 계획의 모든 설계 전제)
- [engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h) / [.cpp](../engine/renderer/InstancedBatchManager.cpp) — `InstanceData` 레이아웃(Phase 0으로 변경됨), `SetupInstanceVAO()`의 attribute 0~12, `RenderBatches()`가 렌더 상태를 건드리지 않는다는 것, `AddInstance`의 `visibleCount` 갱신, `ClearInstances`가 필요한 이유, `CleanupStaleBatches()`가 빈 배치를 지운다는 것
- [engine/renderer/Mesh.h](../engine/renderer/Mesh.h) — `create(vertices, indices)`로 코드에서 메시 생성 가능. `renderer/`에 Quad 프리미티브 헬퍼는 없음
- [engine/ecs/Reflection.h](../engine/ecs/Reflection.h) — `FieldType` 7종(Vec4/Color 없음), `GE_BEGIN_COMPONENT`의 직렬화 switch에 `Enum` case 없음
- [engine/ecs/RenderSystem.cpp](../engine/ecs/RenderSystem.cpp) — 엔진 전체에서 `InstanceData`를 구성하는 유일한 지점
- [engine/renderer/SceneMeshRenderer.cpp](../engine/renderer/SceneMeshRenderer.cpp), [engine/assets/shaders/pbr_instanced.vert](../engine/assets/shaders/pbr_instanced.vert) — 인스턴스 셰이더가 location 0~11만 선언. `RenderBatches` 실호출은 `ForwardOpaque` 하나
- [docs/PREFAB_IMPLEMENTATION_PLAN.md](PREFAB_IMPLEMENTATION_PLAN.md) — Phase 분해와 검증 단계 기록 형식, 리플렉션 등록 위치 관례(`Reflection.cpp`)
- 저장소 전체 `glEnable(GL_BLEND)|glBlendFunc|glDepthMask` 검색 — 0건 / `Particle|파티클|VFX` 검색 — 기존 구현 0건
