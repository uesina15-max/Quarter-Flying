# VFX Lite — 파티클 시스템 범위 정의서

**작성일**: 2026-08-20 (최종 개정: 2026-09-09)
**문서 상태**: 범위/철학 확정, 구현 계획은 아직 아님 — "추후 논의" 후 `docs/PREFAB_IMPLEMENTATION_PLAN.md`와 같은 형식의 실제 구현 계획서를 별도로 만든다. 이 문서는 그 전 단계, 즉 **"무엇을 만들고 무엇을 만들지 않을 것인가"를 먼저 고정**하기 위한 것이다.

> **개정(2026-09-09)**: §1~§6의 전제를 실제 소스와 대조한 **§7**을 추가했다. 알파 전달 방식(§7.2 — `InstanceData` 확장)과 색상 필드 직렬화(§7.3)가 확정되었고, 대신 투명/블렌딩 렌더 상태(§6.3)가 새 미해결 항목으로 드러났다. 그 §6.1과 §6.3도 같은 날 확정되어(각 절의 ✅ 참고) **이 문서에 남은 미해결 항목은 없다** — 구현은 [docs/VFX_LITE_IMPLEMENTATION_PLAN.md](VFX_LITE_IMPLEMENTATION_PLAN.md)로 넘어간다. 파일 끝의 외부 검토 보고서는 원문 보존용이며, §7과 충돌하는 부분은 §7이 우선한다.

---

## 0. 한 줄 정의

> 파티클 시스템을 범용 VFX 툴이 아니라 **"내 게임에 필요한 이펙트를 몇 개의 값으로 표현할 수 있는가"** 기준으로 설계된 **작은 효과 시스템(VFX Lite)** 하나로 제한한다.

기준이 "파티클이 무엇을 할 수 있는가"가 아니라 "몇 개의 설정값으로 얼마나 커버되는가"라는 게 이 문서 전체의 핵심 결정이다.

---

## 1. 배경 — 지금 있는 것과 없는 것

이 저장소에서 파티클/VFX 관련 코드를 grep한 결과 **0건**이다. `docs/MASTER_PLAN.md` §3(8대 핵심 시스템 요구사항 원장)의 "물리 & 게임플레이" 행에 "파티클"이라는 단어 하나로만 존재하고(§4 로드맵에서도 Beta 단계 항목), 실제 구현은 완전히 신규다 — 이 점은 프리팹이 착수 전 상태였던 것과 같다.

다만 프리팹과 달리 파티클은 **재사용할 수 있는 렌더링 기반이 이미 일부 있다**:

- **`InstancedBatchManager`**([engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h)) — 2026-08-18에 `RenderSystem`을 통해 실제로 연결되어 검증된 인스턴스 렌더링 경로(ROADMAP.md §6). `InstancedBatchKey`(mesh/material/shader 기준 배치)와 `AddInstance`/`ClearInstances`/`RenderBatches`가 이미 있다 — 파티클도 결국 "같은 쿼드 메시를 대량으로, 인스턴스별 위치/크기/색/회전만 다르게" 그리는 것이므로 **새 렌더링 파이프라인을 만들 필요 없이 이 위에 얹을 수 있는 형태**다.

  > **개정(2026-09-09, 실제 코드 대조)**: "얹을 수 있다"는 맞지만 **그대로는 안 된다**. 현재 `InstanceData`는 위치/크기/회전을 개별 값으로 받지 않고 `glm::mat4 model` 하나로 받으며, `glm::vec3 color`에 **알파 채널이 없다** — 즉 §2.4의 Alpha Fade를 실어 보낼 자리가 지금은 없다. 해결 방식은 §7.2에서 확정했다.
- **없는 것**: 실제 `Texture`/`Material` 클래스, 텍스처 로딩 파이프라인. `engine/asset/`(`AssetManager`/`AssetRegistry`/`TextureImporter`)는 `docs/PREFAB_IMPLEMENTATION_PLAN.md` §1에서 이미 확인했듯 **CMake에는 있지만 아무 것도 실제로 호출하지 않는 고아 모듈**이다. 즉 "Start Color/End Color" 같은 순수 색상 값은 지금 당장도 그릴 수 있지만(버텍스/인스턴스 컬러만 있으면 됨 — `SceneMeshRenderer`가 이미 하는 방식과 동일), 스프라이트 **텍스처**를 입히려면 프리팹 작업 때와 마찬가지로 "죽은 파이프라인을 살릴지, 우회할지"를 먼저 결정해야 한다. 아래 §6의 미해결 항목으로 남긴다.
- **프리팹과의 접점**: 방금 완성된 프리팹 시스템(`docs/PREFAB_IMPLEMENTATION_PLAN.md`, Phase 1~4 완료)이 정확히 "에디터에서 값을 채운 컴포넌트 묶음 → JSON으로 캡처 → 파일로 저장 → 런타임에 재생성"을 이미 구현하고 있다. 아래 §4에서 다시 짚지만, **"파티클 이펙트 = 프리팹 하나"로 보면 새 직렬화 포맷을 발명할 필요가 없다** — 이 사용자가 요청한 "에디터 설정 → 직렬화 → 프리팹 → 런타임" 경로 자체가 이미 존재하는 기계 위에 올라탈 수 있다는 뜻이다.

---

## 2. 확정 범위 — 파라미터 목록

아래 5개 그룹, 18개 값이 전체 범위다. **더 늘리지 않는다** — §3의 제외 목록과 쌍을 이룬다.

> **개정(2026-08-20)**: 초안은 "15개 값"이라고 썼지만 실제로 표를 세면 4(생성)+4(이동)+2(크기)+3(색상)+2(회전)+3(랜덤)=**18개**다. 문서가 구현보다 먼저 존재하는 이유가 바로 이런 숫자 오류를 구현 착수 전에 잡기 위한 것이므로 정정한다.

### 2.1 생성 (Spawn)
| 값 | 의미 |
|---|---|
| Spawn Rate | 초당 생성 개수 |
| Max Particle | 동시 존재 가능한 최대 개수 |
| Lifetime | 파티클 하나의 수명(초) |
| Burst | 한 번에 터뜨리는 개수(발동형 이펙트용) |

> **Burst + Spawn Rate 동시 사용 확정**: 같은 이펙트 안에서 지속 방출(Rate)과 1회성 폭발(Burst)을 동시에 허용한다(예: 폭발 순간 파편을 Burst로 뿜으면서 동시에 잔불 연기를 Rate로 계속 방출) — 이펙트당 둘 중 하나만 고르도록 제한하지 않는다. §6의 옛 미해결 항목이 여기서 확정됐다.

### 2.2 이동 (Motion)
| 값 | 의미 |
|---|---|
| Initial Speed | 생성 시 초기 속력 |
| Direction | 발사 방향 + 퍼짐 범위(각도) |
| Gravity | 중력 가속도 |
| Drag | 속도 감쇠 계수 |

> **Direction 표현 확정**: 고정 벡터 하나가 아니라 "기준 방향 + 퍼짐 각도"로 구성한다(각도 0°=한 방향으로만, 180°/360°에 가까울수록 원뿔형·전방위 방출) — 새 파라미터를 추가하는 게 아니라 Direction **하나의 값 안**에서 스파크 같은 좁은 분사부터 폭발/먼지 같은 전방위 방출까지 전부 표현하기 위함이다. §2의 18개 값 개수는 그대로 유지된다.

### 2.3 크기 (Size)
| 값 | 의미 |
|---|---|
| Start Size | 생성 시 크기 |
| End Size | 소멸 시 크기 |

### 2.4 색상 (Color)
| 값 | 의미 |
|---|---|
| Start Color | 생성 시 색상 |
| End Color | 소멸 시 색상 |
| Alpha Fade | 수명에 따른 투명도 변화 |

### 2.5 회전 (Rotation)
| 값 | 의미 |
|---|---|
| Start Rotation | 생성 시 회전각 |
| Rotation Speed | 초당 회전 속도 |

### 2.6 랜덤 (Random)
| 값 | 의미 |
|---|---|
| Speed ± | 속력에 적용할 변동폭 |
| Size ± | 크기에 적용할 변동폭 |
| Rotation ± | 회전에 적용할 변동폭 |

**커버 범위(사용자 판단)**: 불, 연기, 먼지, 피격 효과, 마법, 폭발, 눈/비, 스파크, 픽업 효과, 이동 잔상류 상당수를 이 18개 값만으로 표현 가능.

---

## 3. 명시적으로 만들지 않는 것

이 목록이 §2 목록만큼 중요하다 — "필요해지면 그때 추가"가 원칙이고, 지금 미리 자리를 만들어두지 않는다(프리팹 계획서의 "베리언트 시스템을 미리 상상하다가 베리언트 시스템을 구현하게 되는 것은 명시적으로 금지" 원칙과 동일한 정신).

- Collision (콜리전 → 레이어 → 마스크 → 콜라이더 → 이벤트 → 반응 → 프레임별 처리 → 성능, 이어지는 연쇄를 처음부터 피한다)
- Sub-emitter
- Noise
- Mesh Particle
- Trail
- Texture Sheet Animation
- 복잡한 Curve Editor
- GPU Particle
- Particle 간 상호작용
- 이벤트 시스템
- 물리 기반 충돌

---

## 4. 설계 철학

### 4.1 자료구조는 평평하게

```text
Particle
 ├─ position
 ├─ velocity
 ├─ lifetime
 ├─ maxLifetime
 ├─ size
 ├─ rotation
 └─ color

ParticleSystem
 ├─ Spawn()
 ├─ Update()
 └─ Render()
```

업데이트 루프도 예측 가능한 순서 그대로:

```text
velocity += gravity * dt
velocity *= drag
position += velocity * dt

life -= dt
t = 1 - life / maxLife

size  = Lerp(startSize,  endSize,  t)
color = Lerp(startColor, endColor, t)
```

핵심은 "화려한 구조"가 아니라 **"문제가 생겼을 때 어디를 보면 되는지가 명확한 것"**이다.

### 4.2 렌더링은 기존 인스턴싱 경로 재사용 (이 저장소 기준 구체화)

§1에서 확인한 대로, `Particle` 배열을 매 프레임 `InstancedBatchManager`의 인스턴스 데이터(위치/크기/회전/색)로 채워 넣고 쿼드 메시 하나로 그리면 된다 — `RenderSystem`이 ECS 엔티티를 그리는 것과 원리가 같다(다만 파티클은 ECS 엔티티 하나하나가 아니라, 이펙트 하나가 통째로 자기 파티클 배열을 관리하며 한 번에 배치 렌더링). 새 GPU 파이프라인이 필요 없다는 뜻이다.

> **개정(2026-09-09)**: 이 절의 "인스턴스 데이터(위치/크기/회전/색)로 채워 넣고"는 실제 `InstanceData`와 다르다 — 위치/크기/회전은 개별 값이 아니라 **파티클마다 `glm::mat4` 하나로 조립해서** 넘겨야 하고, 알파는 실을 자리가 아예 없다. 전자는 §4.1의 평평한 루프에 행렬 조립 비용이 붙는다는 뜻이고(그래도 파이프라인을 새로 만드는 것보다 싸다), 후자는 §7.2에서 `InstanceData` 확장으로 확정했다.

### 4.3 에디터: 값 입력 폼 하나면 충분

```text
Particle Effect
────────────────
Spawn     Rate [30]  Lifetime [0.8]  Burst [10]
Motion    Speed [3.0]  Gravity [0.0]  Drag [0.1]
Appearance  Start Size [0.2]  End Size [0.0]
            Color: Start [Orange]  End [Red]
Random    Speed [20%]  Size [10%]

[Preview]
```

Motion Editor UI 단순화 때와 같은 원칙 — 값 입력 폼 + 미리보기 이상으로 화려하게 만들지 않는다.

### 4.4 직렬화 경로: 새로 만들지 않고 프리팹에 얹는다

사용자가 요청한 "에디터 설정 → 직렬화 → 프리팹 → 런타임" 경로는 **이미 존재하는 프리팹 메커니즘과 정확히 같은 모양**이다:

- 파티클 18개 값을 담는 `ParticleEffectComponent`(POD) 하나를 `GE_BEGIN_COMPONENT`/`GE_FIELD` 매크로로 등록한다(`engine/ecs/Reflection.cpp`, `TransformComponent`/`AIComponent`와 같은 자리) — 이러면 `ComponentRegistry`의 기존 직렬화/역직렬화/필드 스키마 메커니즘을 그대로 받는다.
- "이펙트 하나 = 프리팹 하나"로 두면, `PrefabAsset::CaptureFromEntity`/`SpawnInto`/`ApplyToEntity`(전부 Phase 1~4에서 이미 구현·테스트 완료)를 그대로 재사용해서 저장/스폰/(나중에) 되돌리기까지 공짜로 얻는다. 새 `.vfx.json` 포맷이나 별도 캡처/스폰 경로를 또 만들 필요가 없다.
- 다만 이건 "이펙트 설정값"의 저장/재현 얘기고, **런타임에 실제로 파티클을 뿜어내는 시뮬레이션 루프(§4.1의 Spawn/Update)는 별개**다 — 그건 컴포넌트 값을 초기 조건으로 읽어가는 새 System(예: `ParticleSystem : System`, `RenderSystem`과 같은 자리)의 몫이다. 프리팹은 "이 이펙트의 설정을 어떻게 저장하고 재현하는가"만 해결해준다.

---

## 5. 성능 철학 — "최적화한다"가 아니라 "비싸질 수 있는 경로를 만들지 않는다"

현재 범위(§2/§3)에서는 GPU Particle·Collision·Trail 같은 기능을 아예 안 넣는 것 자체가 가장 큰 최적화다. 아래는 "만들고 나서 최적화"가 아니라 **처음부터 비용이 커질 수 있는 경로 자체를 막아두는** 확정 정책이다 — 옛 §5(미해결)에 있던 "ECS와의 관계"·"Max Particle 초과 시 정책" 두 항목이 여기서 결정되어 §6으로 옮겨가지 않는다.

### 5.1 파티클은 ECS 엔티티가 아니다 (§4의 암묵적 전제를 확정)

```text
ECS Entity
 └─ ParticleEffectComponent   # 설정값만 보관

ParticleSystem
 └─ EffectInstance
      └─ Particle[]            # 연속 메모리 배열, ECS 밖
```

파티클 하나마다 ECS 엔티티를 만들면 Max Particle=1,000인 이펙트 하나가 ECS 엔티티 1,000개가 되고, 10개가 동시에 터지면 10,000개가 된다. **"이펙트"만 ECS 단위이고, 파티클 자체는 `EffectInstance` 내부의 배열로만 존재한다.**

### 5.2 Max Particle은 게임플레이 옵션이 아니라 하드 리소스 상한

초과 시 정책은 **신규 생성 거부**로 확정한다(순환 버퍼로 오래된 파티클을 강제 소멸시키지 않는다):

```text
if (aliveCount >= maxParticle)
    skip spawn;
```

순환 버퍼 방식은 폭발/피격처럼 짧고 강렬한 이펙트에서 파티클이 수명을 다 채우지 못하고 갑자기 사라지는 부자연스러움을 만든다. "초과분은 생성 자체가 안 됨, 이미 살아있는 파티클은 정상적으로 수명을 마침"이 가장 예측 가능하다.

> **구분해야 할 것**: "Lifetime이 다 돼서 자연 소멸"과 "성능을 이유로 기존 파티클을 조기 강제 제거"는 서로 다른 이야기다 — VFX Lite는 전자만 한다. Max Particle에 도달해도 이미 살아있는 파티클을 강제로 죽이지 않는다(위 정책 그대로) — 신규 Spawn만 막을 뿐, 살아있는 파티클의 Lifetime을 인위적으로 줄이는 조기 제거는 하지 않는다.

### 5.3 Spawn Rate × Lifetime으로 평균 파티클 수를 에디터에서 미리 보여준다

```text
Average Particle Count ≈ Spawn Rate × Lifetime
```

예: Rate=100/sec, Lifetime=2sec → 평균 약 200개. §4.3의 에디터 폼에 "Expected: ~200 / Max: 256"을 같이 보여주고, Max Particle이 이 어림값 대비 지나치게 크면 경고한다(예: Rate 300 × Lifetime 3.0 ≈ 900인데 Max Particle=4096이면 경고). **잘못된 설정을 에디터 단계에서 막는 게 가장 싼 최적화**다.

### 5.4 업데이트는 한 줄의 선형 루프, 외부 시스템 호출 없음

§4.1의 루프를 그대로 유지하되, 이 루프 안에 다음을 절대 넣지 않는다:

- Particle → Collision World
- Particle → Physics
- Particle → Event
- Particle → Script
- Particle → 다른 Particle

이 중 하나라도 넣는 순간 비용이 `파티클 수 × 외부 시스템 비용`으로 커진다(§3에서 Collision을 제외한 이유와 같다). VFX Lite에서 파티클 하나는 독립적으로 "생각"할 일이 없어야 한다.

### 5.5 메모리는 매 프레임 할당하지 않는다

- 초기화 시 `particles.reserve(maxParticle)` 한 번.
- 죽은 파티클은 swap-remove로 O(1) 제거(`p = particles.back(); particles.pop_back();`) — `erase()`로 배열을 매번 밀지 않는다.
- Spawn/Update/Remove 어디에서도 `new`/`delete`/`malloc`/`free`/벡터 재할당이 반복되지 않는다.

### 5.6 렌더링: 파티클 수 증가가 파티클별 Draw Call로 이어지지 않는다

§4.2에서 재사용하기로 한 `InstancedBatchManager`의 목표를 명확히 한다:

> **동일한 Mesh/Material/Shader를 사용하는 살아있는 파티클은 하나의 인스턴스 배치로 렌더링하며, 파티클 수 증가로 인해 파티클별 Draw Call이 발생하지 않는다.**

> **개정(2026-08-20)**: 초안은 "파티클 500개든 5개든 Draw Call은 항상 1개"라고 썼는데, 이건 **하나의 이펙트가 하나의 동일한 `InstancedBatchKey`를 쓸 때만** 성립한다 — `InstancedBatchManager`([engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h))의 `InstancedBatchKey` 자체가 `meshGuid`/`materialId`/`shaderId`/`passType`/`materialLayout`/`features` 기준으로 배치를 나눈다. §6.1에서 텍스처/머티리얼을 어떻게 다룰지 정하고 나면, 이펙트 하나 안에서도 파티클마다 다른 Material을 쓰는 경우 배치가 여러 개로 나뉠 수 있다 — "항상 1개"라고 못박아뒀다가 나중에 예외를 만나는 것보다, 처음부터 "같은 배치 키를 쓰는 동안은 1개"로 정확하게 적어두는 게 안전하다.

`InstancedBatchManager`에 넘기는 CPU 쪽 임시 인스턴스 배열도 매 프레임 새로 만들지 않고 재사용 가능한 버퍼로 둔다 — 안 그러면 "인스턴싱으로 배치당 Draw Call은 줄였는데 CPU 쪽 할당이 매 프레임 반복되는" 절반짜리 최적화가 된다.

> **개정(2026-09-09)**: "같은 배치 키를 쓰는 동안은 Draw Call 1개"는 실제 코드에서도 성립한다. 다만 **배치들 사이의 순서는 보장되지 않는다** — `SortBatchesForRendering()`이 만든 `sortedBatches`를 `RenderBatches()`가 쓰지 않고 원본 `unordered_map`을 순회하기 때문이다(§7.4). 파티클처럼 매 프레임 `AddInstance`를 부르는 소비자는 정렬 플래그를 계속 세워서 **쓰이지도 않는 정렬 비용을 매 프레임** 낸다. 반투명을 얹으면 순서 문제까지 겹치므로 §6.3과 함께 다룬다.

### 5.7 이펙트 단위 상한도 둔다 (엔진 정책, 에디터 파라미터 아님)

파티클 하나하나보다 "동시에 떠 있는 이펙트 개수"가 실제 렉의 원인이 되는 경우가 더 흔하다(예: Explosion×20 + Dust×30 + Hit×50 + Magic×20 = 이펙트 120개 × Max Particle 256 = 30,720개). 그래서 이펙트 단위 전역 상한을 별도로 둔다:

```text
ParticleSystem
 ├─ Max Active Effects
 └─ Max Total Particles
```

이 두 값은 **§2의 18개 값에 추가하지 않는다** — 이펙트 하나가 조절할 값이 아니라 엔진 전체의 성능 정책이라 에디터에 노출하지 않는다.

### 5.8 비활성 이펙트는 Update 자체를 스킵

살아있는 파티클이 0개이고 지속 방출(Rate)도 아닌 이펙트는 시스템 순회에서 제외한다. 특히 Burst 이펙트는 "터짐 → 전부 소멸 → 그 이후 매 프레임 빈 배열을 검사하는 것조차 안 함"까지 가야 깔끔하다.

### 5.9 Frustum culling 등은 1차 구현에 필수 아님

카메라 밖 파티클을 위한 정교한 culling은 지금 범위가 아니다(§3의 "지금 만들지 않는 것" 정신과 같음 — 필요해지면 그때). 우선순위는 다음 순서로 본다:

```text
1. Max Particle (하드 상한)
2. Max Total Particles (전역 상한)
3. ECS 미사용 (§5.1)
4. 배열 기반 Update (§5.4/§5.5)
5. Instancing (§5.6)
6. 메모리 재사용 (§5.5)
7. Effect-level culling (나중)
```

### 5.10 에디터 Preview도 런타임과 동일한 상한을 강제한다

Preview가 사용자가 입력한 값을 무제한으로 돌리면, 실수로 Rate=10000/Lifetime=10을 넣는 순간 에디터가 죽는다 — 죽은 에디터는 디버깅할 수조차 없으니 가장 나쁜 실패 방식이다. 그래서:

- Preview에도 하드 상한(예: 2048 파티클)을 별도로 둔다 — 사용자가 입력한 Max Particle이 이보다 커도 Preview는 이 상한에서 멈춘다.
- §5.3의 어림 계산 결과("Estimated Average/Peak")와 경고를 Preview 옆에 같이 보여준다.

### 5.11 이펙트 자체의 종료 조건 — 별도 파라미터 없음

```text
Effect Spawn
    ↓
Burst 30 (또는 지속 Rate)
    ↓
Particle 생성
    ↓
각 Particle Lifetime 감소
    ↓
Particle 사망
    ↓
aliveCount == 0 이고 지속 Spawn도 아님
    ↓
EffectInstance 비활성 → Update 중지 (§5.8)
```

**Burst 이펙트**는 마지막 파티클이 죽는 순간 이펙트도 자동으로 끝난다. **지속형 이펙트**(Rate>0)는 Rate가 꺼지거나 이펙트 자체가 외부에서 제거되기 전까지는 계속 새 파티클을 만들어내므로 무기한 살아있다. 어느 쪽이든 **이펙트 자체의 "Lifetime" 값을 §2에 별도로 추가하지 않는다** — 파티클 하나의 Lifetime(§2.1)이 이미 이펙트 전체의 생사를 결정하므로, 18개 값은 그대로 유지된다.

### 5.12 죽은 파티클은 인스턴스 버퍼에 넣지 않는다

§5.5의 swap-remove가 CPU 쪽 `Particle[]`에서 죽은 항목을 제거하는 것이라면, 이 절은 그 결과를 렌더링까지 그대로 이어간다는 걸 명시한다: `Render()`가 `InstancedBatchManager`에 넘기는 인스턴스 데이터(position/size/rotation/color)는 **그 프레임에 살아있는 파티클만**이다.

```text
Particle[]
 ├─ alive  → 인스턴스로 전달
 ├─ alive  → 인스턴스로 전달
 ├─ dead   → §5.5에서 이미 제거됨, 애초에 여기 없음
 └─ alive  → 인스턴스로 전달
```

Update 단계(§5.5)에서 죽은 파티클이 이미 배열 밖으로 빠지므로, Render 단계는 "살아있는 것만 걸러서 넘기는" 별도 필터링을 할 필요조차 없다 — 배열에 남아있는 게 곧 살아있는 것이다. GPU는 죽은 파티클을 위한 인스턴스를 애초에 받지도, 처리하지도 않는다.

### 5.13 한 프레임의 실행 순서 (§5.1~§5.12 종합)

```text
ParticleSystem::Update()
        │
        ├─ Spawn            (Rate/Burst로 신규 Particle 추가, §5.2 상한 적용)
        ├─ Update Particle[] (§4.1의 물리/보간 루프)
        ├─ Remove Dead      (swap-remove, §5.5/§5.12)
        └─ 활성 상태 재확인   (§5.8/§5.11)

ParticleSystem::Render()
        └─ 그 프레임에 살아있는 Particle만
                 ↓
           InstancedBatchManager (§5.6)
                 ↓
           Quad Mesh × 1, Instance × N
                 ↓
           같은 InstancedBatchKey당 Draw Call × 1 (§5.6)
```

시간이 지날수록 파티클이 자연스럽게 줄고, 그만큼 렌더링 비용도 같이 줄어든다 — 별도의 감쇠 로직 없이 §2.1의 Lifetime 값 하나가 전체 비용 곡선을 결정한다.

### 확정 체크리스트

- [ ] 파티클은 ECS Entity가 아니다 — `EffectInstance` 내부의 연속 메모리 배열로 관리한다(§5.1).
- [ ] Max Particle은 하드 상한이다. 초과 시 신규 Spawn을 거부한다(순환 버퍼 금지, §5.2).
- [ ] Spawn/Update/Remove 과정에서 프레임별 동적 메모리 할당을 하지 않는다(§5.5).
- [ ] Particle은 `InstancedBatchManager`를 통해 배치 렌더링한다 — 같은 Mesh/Material/Shader(`InstancedBatchKey`)를 쓰는 동안은 파티클 수가 늘어도 파티클별 Draw Call이 발생하지 않는다(§5.6).
- [ ] `EffectInstance`가 비활성 상태(파티클 0개 & 지속 방출 아님)면 Update를 수행하지 않는다(§5.8).
- [ ] 엔진 전체에 `Max Active Effects`/`Max Total Particles` 상한을 둔다 — 에디터 파라미터로 노출하지 않는다(§5.7).
- [ ] 에디터 Preview도 동일한(또는 더 낮은) 상한을 적용한다(§5.10).
- [ ] Collision/Physics/Event/Script 등 파티클별 외부 시스템 호출은 지원하지 않는다(§3, §5.4).
- [ ] 이펙트 자체의 Lifetime 파라미터는 없다 — Burst는 마지막 파티클 사망 시 자동 종료, 지속형은 Rate/외부 제거로만 종료(§5.11).
- [ ] 죽은 파티클은 인스턴스 버퍼에 들어가지 않는다 — Update의 swap-remove 결과가 곧 Render의 입력이다(§5.12).
- [ ] Max Particle 초과 시에도 이미 살아있는 파티클을 조기 강제 제거하지 않는다 — 신규 Spawn만 막는다(§5.2).

> **결론**: VFX Lite의 성능 철학은 "파티클을 최적화한다"가 아니라 **"파티클이 비싸질 수 있는 경로를 만들지 않는다"**이다.

---

## 6. 미해결 — 추후 논의할 것

구현 계획서를 쓰기 전에 결정이 필요한 지점들. (프리팹 계획서가 §2.1/§2.6 같은 절에서 이런 걸 미리 결정하고 시작했던 것과 같은 이유 — 착수 후에 발견하면 재작업 비용이 더 크다.) ECS 관계·Max Particle 초과 정책(§5.1/§5.2), Burst+Rate 동시 사용 여부(§2.1), Direction 표현(§2.2), 이펙트 종료 조건(§5.11), Draw Call 범위(§5.6)까지 전부 결정되어 여기서 빠졌다.

> **개정(2026-09-09)**: 초안은 여기서 "**남은 건 사실상 §6.1 하나뿐이다**"라고 썼지만, §7의 실제 코드 대조에서 **미해결 항목이 하나 더 있다는 게 드러났다** — 이 저장소에는 블렌딩 관련 GL 호출이 아예 없어서, 반투명 파티클을 그리려면 투명 렌더 상태를 만드는 일이 먼저다(§6.3). 반대로 알파를 어떻게 실을지(§7.2)와 색상 필드를 어떻게 직렬화할지(§7.3)는 이번에 확정되어 미해결에서 빠졌다. **남은 결정은 §6.1과 §6.3, 두 개다.**
>
> **개정(2026-09-09, 2차)**: 그 두 개도 [docs/VFX_LITE_IMPLEMENTATION_PLAN.md](VFX_LITE_IMPLEMENTATION_PLAN.md) §2.1/§2.2에서 확정되었다 — **§6.1은 "v1은 텍스처를 로딩하지 않는다"(셰이더 절차적 원형 마스크), §6.3은 "렌더 패스를 리팩터링하지 않고 파티클 전용 상태 전환만 만든다 + 블렌드는 가산 혼합 하나로 고정"**. 이로써 **이 문서의 미해결 항목은 없다.** 각 결정의 근거는 구현 계획서에 있고, 아래 §6.1/§6.3에는 결론만 적어둔다.

### 6.1 텍스처/머티리얼 파이프라인 — ✅ 확정(2026-09-09)

> **확정**: **v1은 텍스처 파일을 읽지 않는다.** 고아 모듈(`engine/asset/`)을 살리지도, VFX 전용 최소 로더를 만들지도 않고, 파티클 스프라이트 모양을 셰이더 안에서 절차적으로 만든다(쿼드 UV의 중심 거리로 부드러운 원형 마스크). 아래 절이 경고한 "파티클 시스템이 텍스처 파이프라인 프로젝트로 번지는" 함정을 피하는 가장 확실한 방법이 **최소 로더조차 만들지 않는 것**이기 때문이다. §2의 커버 목록은 대부분 "부드러운 원형 점 + 색/알파/크기 변화"로 표현되고, 절차적 마스크는 셰이더 두 줄이라 나중에 버릴 때 비용이 0이다. `materialId`는 텍스처 자리로 예약만 해둔다. 상세 근거는 [구현 계획서 §2.1](VFX_LITE_IMPLEMENTATION_PLAN.md).

아래는 확정 전 논의 기록이다.

§1에서 확인했듯 실제 `Texture`/`Material` 로딩 경로가 없다(`engine/asset/`는 고아 모듈). "단순 Sprite 렌더링"이 색상만 있는 쿼드(텍스처 없음)로 시작할지, 아니면 파티클을 위해서만 최소한의 텍스처 로딩을 먼저 만들지 결정이 필요하다 — 후자를 택하면서 범위를 잘못 잡으면 "파티클 시스템"이 조용히 "텍스처 파이프라인 프로젝트"로 번질 위험이 있다(§3의 경고와 같은 종류의 함정).

> **현재 검토 중인 방향(확정 아님)**: 고아 상태인 `engine/asset/`(`AssetManager`/`AssetRegistry`/`TextureImporter`) 전체를 살리는 대신, **VFX Lite에 필요한 최소한의 Sprite/Texture 경로만 별도로 만드는 쪽을 먼저 검토한다** — "작게 만들고, 비싸질 수 있는 경로를 만들지 않는다"는 이 문서의 §5 철학과 가장 잘 맞는 방향이기 때문이다. 다만 이건 아직 §2~§5처럼 "확정"된 게 아니라 **다음으로 검토할 방향**이라는 점을 구분해서 남겨둔다 — 실제로 파고들면(예: 최소 텍스처 로딩이라도 `InstancedBatchKey`의 `materialId`에 어떻게 연결할지, 파일 포맷은 무엇으로 할지) 새로운 트레이드오프가 나올 수 있다.

> **추가 확인(2026-09-09)**: `materialId`를 텍스처 핸들에 매핑하는 방향 자체는 유효하지만, **그 `materialId`로 텍스처를 실제로 bind하는 코드가 없다** — `RenderBatch(key, shader)`는 `materialId`를 배치 분리 키로만 쓰고, `RenderBatches(passType, shader)`는 모든 배치에 셰이더 하나를 그대로 쓴다. 따라서 이 결정에는 "배치별 텍스처 바인딩 훅"을 어디에 둘지가 함께 포함된다.

**다음 단계**: ✅ 완료 — §6.1/§6.3 확정 후 [docs/VFX_LITE_IMPLEMENTATION_PLAN.md](VFX_LITE_IMPLEMENTATION_PLAN.md)를 작성했다(Phase 0~5 분해 포함). 이 문서는 여기서 범위 정의서로서의 역할을 마치고, 이후 구현 관련 결정은 계획서 쪽에 기록한다.

### 6.2 Revert/Apply와의 연결 (참고용, 결정 사항 아님)

프리팹 Phase 4의 Revert가 이미 있으니 파티클 프리팹도 자동으로 Revert를 상속받는다 — "프리팹에 얹은 덕에 공짜로 따라오는 것"이라는 점만 확인해두면 됨.

### 6.3 투명/블렌딩 렌더 상태 — ✅ 확정(2026-09-09, 신설 당일 확정)

> **확정**: **렌더러의 패스 구조를 리팩터링하지 않는다.** (1) 블렌드 상태는 `InstancedBatchManager`가 아니라 파티클 렌더 경로가 자기 draw 앞뒤로만 설정하고 원복한다 — 지금 유일하게 잘 도는 불투명 경로를 건드리지 않기 위해서다. (2) **v1의 블렌드 모드는 가산 혼합 하나로 고정한다.** 이건 게으름이 아니라 방어선이다 — §7.4에서 확인한 배치 순서 비결정성 때문에 알파 블렌딩은 결과가 불안정한데, 가산 혼합은 교환법칙이 성립해 순서 문제를 **회피**한다. 덕분에 블렌드 모드가 19번째 파라미터가 되지 않아 **§2의 18개 값이 그대로 유지된다.** (3) `ForwardTransparent`를 그리는 호출부는 파티클 것 하나만 만든다. 상세 근거는 [구현 계획서 §2.2](VFX_LITE_IMPLEMENTATION_PLAN.md).

아래는 확정 전 문제 정의다.

§7.1에서 확인했듯 이 저장소에는 `glEnable(GL_BLEND)`·`glBlendFunc`·`glDepthMask`가 **하나도 없다**. `PassType::ForwardTransparent`는 enum 값으로만 존재하고 실제로 그리는 호출부가 없다. 따라서 "가산 혼합을 기본으로 하고 Z-Write를 끈다"는 방침은 **설정을 고르는 문제가 아니라 투명 패스를 새로 만드는 문제**다. 최소한 다음이 필요하다:

- `RenderBatch()`/`RenderBatches()`가 `passType`에 따라 블렌드·깊이 쓰기 상태를 설정하고 원복하는 자리
- `PassType::ForwardTransparent` 배치를 실제로 그리는 호출부(지금은 `SceneMeshRenderer.cpp`가 `ForwardOpaque` 하나만 그린다)
- 불투명 → 투명 순서 보장(§7.4의 배치 순서 비결정성과 같이 풀어야 한다)

§6.1과 같은 종류의 함정이 여기에도 있다 — 범위를 잘못 잡으면 "파티클 시스템"이 조용히 **"렌더 패스 리팩터링 프로젝트"**로 번진다. 파티클이 쓰는 최소한만 만들지, 렌더러의 패스 구조를 정리할지를 착수 전에 정한다.

---

## 7. 실제 코드 대조 검증 (2026-09-09)

§1~§6은 2026-08-20 시점의 코드 이해를 바탕으로 쓰였다. 이 절은 그 전제들을 `engine/renderer`·`engine/ecs`·`engine/editor`의 실제 소스와 대조한 결과이며, **틀린 전제를 정정하고 그 결과 필요해진 결정을 확정한다.**

> **검증 단계 명시**(CLAUDE.md "코드 품질 관례" #2): 이 절 전체는 **소스 코드 정적 확인** 단계다. 컴파일·링크도, 단위 테스트도, 실제 실행도 아직 아니다. §7.2의 확장안 역시 "이렇게 하면 안전하다"는 근거를 코드에서 확인한 것이지 빌드해본 것이 아니다.

### 7.1 전제 검증 결과

| 이 문서의 전제 | 실제 코드 | 판정 |
|---|---|---|
| 인스턴스 데이터에 위치/크기/회전/색을 넘긴다 | `InstanceData` = `mat4 model`(64) + `vec3 color`(12) + roughness/metallic(8) + entityId/isSelected(8) + `_padding[1]`(4) = **96바이트** | **정정** |
| 색상에 알파를 실을 수 있다 | `color`가 `vec3` — 알파 채널 없음 | **정정 → §7.2에서 해결** |
| 인스턴싱 경로를 "그대로" 재사용 | 위치/크기/회전을 파티클마다 `mat4`로 조립해야 함 | 조건부 성립 |
| 투명/가산 혼합은 렌더 상태만 지정하면 됨 | 블렌딩 GL 호출 **저장소 전체 0건**, `ForwardTransparent` 미사용 | **미해결 신설 → §6.3** |
| 프리팹 리플렉션으로 18개 값을 그대로 직렬화 | `FieldType`에 **Vec4/Color 없음**. `Enum`은 직렬화 switch에 **case가 없어 조용히 누락됨** | **정정 → §7.3** |
| 같은 배치 키면 Draw Call 1개 | 성립. 단 배치 **사이의** 순서는 비결정적 | 별도 이슈 → §7.4 |
| `Particle` 56B가 64B 캐시라인에 "완벽 수용" | 배열 stride 56에서는 경계를 걸침. 단 8개 = 448B = **정확히 7라인** | 결론 유지, **근거 정정** |

### 7.2 확정 — 알파는 `InstanceData`를 확장해서 싣는다

§2.4의 Alpha Fade는 18개 확정 파라미터 중 하나이므로 "렌더 경로에 자리가 없다"를 이유로 뺄 수 없다. 선택지는 셋이었다:

1. `roughness`/`metallic`을 알파로 재활용 — 코드 변경은 0이지만 필드 이름이 거짓말이 된다. 나중에 머티리얼이 실제로 들어오는 순간 조용히 충돌한다.
2. 파티클 전용 인스턴스 경로를 따로 만든다 — §4.2의 "새 파이프라인을 만들지 않는다"에 정면으로 어긋난다.
3. **`InstanceData`에 알파 필드를 추가한다.** ← **확정**

3번을 택하되 **기존 렌더링에 미치는 영향이 0이 되는 방식**으로 한다. 핵심은 새 바이트를 붙이는 게 아니라 **이미 있는 `_padding[1]`을 알파로 바꾸는 것**이다:

```cpp
// 현재 (engine/renderer/InstancedBatchManager.h)
uint32_t isSelected;      // offset 88
float _padding[1];        // offset 92  -> sizeof = 96

// 확정안
uint32_t isSelected;      // offset 88 (불변)
float    alpha{1.0f};     // offset 92 (padding 자리를 그대로 씀) -> sizeof = 96 (불변)
```

"안정적"이라고 부르는 근거는 전부 코드에서 확인된 사실이다:

- **`sizeof(InstanceData)`가 96으로 불변** → 기존 `static_assert(sizeof % 16 == 0)`을 그대로 통과하고, VBO stride(`GLsizei stride = sizeof(InstanceData)`)도 바뀌지 않는다.
- **기존 필드의 offset이 전부 불변**(model 0 / color 64 / roughness 76 / metallic 80 / entityId 84 / isSelected 88) → `SetupInstanceVAO()`의 attribute 3~11이 **한 줄도 바뀌지 않는다**.
- **추가되는 건 attribute 12 하나뿐**(`offsetof(InstanceData, alpha)`, divisor 1). 총 13개(0~12)이고 OpenGL이 보장하는 최소 attribute 수는 16이라 여유가 있다.
- **기존 셰이더를 손대지 않아도 된다.** `assets/shaders/pbr_instanced.vert`와 `SceneMeshRenderer.cpp`의 인라인 셰이더는 location 0~11만 선언하는데, 활성화됐지만 선언되지 않은 attribute는 GL이 그냥 무시한다. 파티클 셰이더만 `layout(location = 12) in float aAlpha;`를 선언한다.
- **기존 호출부가 알파를 몰라도 정상 동작한다.** 엔진 전체에서 `InstanceData`를 구성하는 곳은 `RenderSystem.cpp`의 `InstanceData data{};` **한 곳뿐**이고, 이 형태는 기본 멤버 초기자를 그대로 사용하므로 알파가 자동으로 `1.0`(완전 불투명)이 된다 — 즉 기존 불투명 렌더링 결과가 바뀌지 않는다. (`CMAKE_CXX_STANDARD 23`이라 집합체 기본 멤버 초기자 사용에 제약이 없다.)
- 이전에는 `_padding[1]`이 **초기화되지 않은 값**인 채로 GPU에 업로드되고 있었다(읽는 쪽이 없어 무해했을 뿐). 이 변경은 그 4바이트에 정의된 의미와 기본값을 준다.

**같이 심을 방어 장치**(CLAUDE.md 관례 #3 — 가정이 깨지는 지점에 컴파일 타임/실행 시 확인을 심는다):

```cpp
static_assert(sizeof(InstanceData) == 96,                  "...");
static_assert(offsetof(InstanceData, color) == 64,         "셰이더 location 7과 짝");
static_assert(offsetof(InstanceData, alpha) == 92,         "셰이더 location 12와 짝");
```

C++ 구조체 레이아웃과 셰이더 `layout(location=...)`이 어긋나는 순간은 **GL 에러 없이 화면만 이상해지는** 전형적인 케이스(`Mesh::drawInstanced` 버그와 같은 유형)이므로 컴파일 타임에 잡는다. 더불어 `Initialize()`의 능력 검사가 지금 `maxVertexAttribs < 7`로만 막고 있는데 실제로는 12개(확장 후 13개)를 쓰므로, **13으로 올리고 어떤 가정이 왜 깨졌는지 알 수 있는 메시지를 남긴다** — 지금 상태로는 attribute가 모자라는 환경에서 조용히 잘못 그려진다.

**구현 시 확인할 3단계**(섞지 않고 각각 기록한다): (1) 빌드 통과 (2) 기존 Scene Editor 뷰포트의 불투명 렌더링이 확장 전과 동일한지 (3) 파티클 알파가 실제 화면에서 변하는지.

### 7.3 색상 필드 직렬화 — `Vec3` + `Float`로 쪼갠다

`FieldType`에 Vec4/Color가 없으므로, Start/End Color를 `ParticleEffectComponent`에 등록할 때 **`Vec3` 색상 + `Float` 알파로 분리**한다. 새 `FieldType`을 만들지 않는 이유:

- `FieldType`을 늘리면 `GE_BEGIN_COMPONENT`의 직렬화/역직렬화/`SetField` switch 세 곳과 에디터 인스펙터를 동시에 건드려야 한다.
- 특히 `engine/editor/panels/inspector.py`가 필드 타입을 **정수 인덱스로 하드코딩**(`0: Int … 6: Enum`)하고 있어서, 중간에 삽입하면 기존 컴포넌트의 필드 타입이 전부 어긋난다. 꼭 추가해야 한다면 **반드시 enum 맨 끝에** 붙여야 한다.
- 파티클 하나 때문에 리플렉션 시스템을 확장하는 건 §5의 "비싸질 수 있는 경로를 만들지 않는다"에 어긋난다.

부수적으로 확인된 구멍: **`FieldType::Enum`은 열거만 되어 있고 직렬화 switch에 case가 없다.** enum 필드를 등록하면 예외도 에러도 없이 값이 저장되지 않는다. 따라서 **VFX Lite는 enum 필드를 쓰지 않는다** — 나중에 블렌드 모드 같은 걸 넣게 되면 이 구멍부터 메워야 한다.

### 7.4 정정된 수치와 근거

- **대역폭**: GPU 업로드는 파티클당 36B가 아니라 **96B**다. 10,000 파티클 × 60Hz면 `10,000 × 96 × 60` = **57.6 MB/s**(21.6 MB/s 아님). CPU 업데이트분을 더해도 약 91 MB/s로 PCIe 대비 여전히 무시할 수준이므로, **"대역폭은 병목이 아니다"라는 결론 자체는 유지된다** — 숫자만 정정한다.
- **캐시라인**: "56B가 64B 라인 안에 완벽 수용"은 배열 순회에서 성립하지 않는다(stride 56이라 파티클 8개 중 7개가 경계를 걸친다). 다만 8개 = 448B = **정확히 7라인**이라 64B로 패딩했을 때(8라인)보다 12.5% 적게 읽는다. 결론은 유지되지만 **이 근거를 오해해서 64B로 패딩하면 오히려 손해다 — 패딩하지 않는다.**
- **배치 렌더 순서**: `SortBatchesForRendering()`이 매 프레임 `sortedBatches`를 만들지만 `RenderBatches()`는 그걸 쓰지 않고 원본 `unordered_map`을 순회한다. 정렬 결과가 버려지므로 순서는 사실상 비결정적이고, 매 프레임 `AddInstance`를 부르는 파티클은 정렬 플래그를 계속 세워 **쓰이지 않는 정렬 비용**까지 낸다. §6.3과 함께 다룬다.
- **`CleanupStaleBatches()`**: 인스턴스가 빈 배치를 제거한다. 현재 호출부는 없지만 나중에 호출하게 되면, §5.8대로 잠시 파티클 0개가 된 이펙트의 VAO/VBO가 파괴되고 다음 Burst에서 재생성된다 — §5.5가 금지한 재할당이 GPU 자원 쪽으로 옮겨간 형태다. **파티클 배치는 이 정리 대상에서 제외한다.**

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- 저장소 전체 `Particle|파티클|VFX` grep — 기존 코드 0건
- [docs/MASTER_PLAN.md](MASTER_PLAN.md) §3, §4 — "파티클"이 요구사항 원장에만 존재, Beta 단계 항목
- [engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h) — 재사용 가능한 인스턴스 렌더링 경로(2026-08-18 검증됨, ROADMAP.md §6)
- `Texture`/`Material` 클래스 부재, `engine/asset/`가 고아 모듈이라는 사실 — [docs/PREFAB_IMPLEMENTATION_PLAN.md](PREFAB_IMPLEMENTATION_PLAN.md) §1에서 이미 확인된 것과 동일
- [docs/PREFAB_IMPLEMENTATION_PLAN.md](PREFAB_IMPLEMENTATION_PLAN.md) — "이펙트 = 프리팹" 접점의 근거(Phase 1~4 완료, `PrefabAsset`/`ComponentRegistry` 메커니즘)
- **§7의 근거(2026-09-09 소스 정적 확인)**:
  - [engine/renderer/InstancedBatchManager.h](../engine/renderer/InstancedBatchManager.h) `InstanceData` — 96바이트 레이아웃, `color`가 `vec3`(알파 없음), `_padding[1]`이 offset 92에 존재
  - [engine/renderer/InstancedBatchManager.cpp](../engine/renderer/InstancedBatchManager.cpp) `SetupInstanceVAO()` — instance attribute 3~11이 `offsetof` 기반으로 고정("12 attributes (3 mesh + 9 instance)"), `RenderBatch()`가 렌더 상태를 전혀 건드리지 않음, `RenderBatches()`가 `sortedBatches` 대신 원본 맵을 순회, `Initialize()`의 능력 검사가 `maxVertexAttribs < 7`
  - [engine/ecs/RenderSystem.cpp](../engine/ecs/RenderSystem.cpp) — 엔진 전체에서 `InstanceData`를 구성하는 유일한 지점(`InstanceData data{};` 뒤 필드별 대입). 정규식 검색으로 다른 구성 지점이 없음을 확인
  - [engine/renderer/SceneMeshRenderer.cpp](../engine/renderer/SceneMeshRenderer.cpp), [engine/assets/shaders/pbr_instanced.vert](../engine/assets/shaders/pbr_instanced.vert) — 인스턴스 셰이더가 location 0~11만 선언(12는 비어 있음). `RenderBatches` 실호출은 `ForwardOpaque` 하나뿐
  - [engine/ecs/Reflection.h](../engine/ecs/Reflection.h) — `FieldType`에 Vec4/Color 없음, `GE_BEGIN_COMPONENT`의 직렬화/역직렬화 switch에 `Enum` case 없음
  - [engine/editor/panels/inspector.py](../engine/editor/panels/inspector.py) — 필드 타입을 정수 인덱스(`0: Int … 6: Enum`)로 하드코딩
  - 저장소 전체 `glEnable(GL_BLEND)|glBlendFunc|glDepthMask` 검색 — **0건**
- §5(성능 철학)는 사용자가 참고자료로 제시한 최적화 원칙 10가지를 이 문서의 기존 구조(§4의 렌더링/직렬화 결정)에 맞춰 재정리한 것이다 — ECS 미사용·Max Particle 하드 상한·신규 spawn 거부 정책은 옛 §5(미해결)의 항목 2/5를 그 자리에서 확정한 것이기도 하다.









---

# [외부 검토 보고서] VFX Lite 파티클 시스템 아키텍처 및 성능 최적화 타당성 검토

> **이 아래는 외부에서 받은 검토 보고서 원문이며, 이 문서의 확정 사항이 아니다.** 원문 보존을 위해 내용은 손대지 않았다. 다만 아래 항목은 §7의 실제 코드 대조에서 **사실과 다른 것으로 확인**되었으므로, 충돌하는 경우 **§7이 우선한다**:
>
> - **§3.1 인스턴스 데이터 36바이트 표** — 실제 `InstanceData`는 96바이트이고 위치/크기/회전은 `mat4`에 들어간다. 알파를 실을 자리가 없다(→ §7.1, §7.2)
> - **§3.2 GPU 업로드 21.60 MB/s** — 실제로는 57.6 MB/s. 총합도 55.20이 아니라 약 91 MB/s. **"병목이 아니다"라는 결론만 유효하다**(→ §7.4)
> - **§2.2 / §6.1 표 "64바이트 캐시라인 내 완벽 수용"** — 배열 순회에서는 성립하지 않는다. 결론은 유지되나 근거가 틀렸고, **이 근거로 64B 패딩을 하면 오히려 손해다**(→ §7.4)
> - **§3.3 / §6.2-1 "Z-Write Off·가산 혼합을 기본 렌더 모드로 제공"** — 렌더 상태를 고르는 문제가 아니라 **투명 패스를 새로 만드는 문제**다. 저장소에 블렌딩 GL 호출이 0건이다(→ §6.3)
> - **§4.1 "별도 파서 개발 없이 직렬화 확보"** — 절반만 맞다. `FieldType`에 Vec4/Color가 없다(→ §7.3)
> - **§5.2 `materialId` 매핑** — 방향은 맞지만 `materialId`로 텍스처를 bind하는 코드가 없다(→ §6.1의 추가 확인)
> - **§6.2-2 인스턴스 버퍼 이중화** — 1차 범위에서 제외한다. 측정된 스톨이 없는 선제 최적화이고 §5의 철학과 어긋난다
> - **§4.3 예측 수식** — `Rate × Lifetime + Burst`는 평균이 아니라 **피크**다(Burst는 감쇠하므로 정상상태 평균에 기여하지 않는다). §5.10의 "Average/Peak" 구분대로 `Average = Rate × Lifetime`, `Peak = + Burst`로 나눈다
>
> 보고서가 **놓친** 실재 이슈(정렬 결과 폐기, `Enum` 직렬화 누락, `CleanupStaleBatches()`의 GPU 자원 재생성)는 §7.4에 있다.

## 1. 서론 및 시스템 설계 개요

게임 엔진 아키텍처에서 파티클 시스템은 시각적 연출의 핵심을 담당하지만, 동적으로 생성되는 다량의 객체 특성으로 인해 CPU 시뮬레이션 오버헤드 및 GPU 메모리 대역폭 병목을 유발하는 주요 원인이 된다. 본 보고서는 파티클 관련 구현 코드가 전무한 상태에서 제시된 'VFX Lite 파티클 시스템 범위 정의서'의 기술적 타당성을 검증하고, 제안된 데이터 아키텍처, 렌더링 파이프라인, 에디터 및 메모리 관리 정책의 적합성을 체계적으로 분석한다.
VFX Lite 사양서는 파티클 시스템을 범용적인 VFX 제작 툴이 아닌, 게임 연출에 필요한 최소한의 설정값으로 제한된 특화 효과 시스템으로 정의한다. 생성 4개, 이동 4개, 크기 2개, 색상 3개, 회전 2개, 랜덤 3개 등 총 18개의 정밀하게 선별된 파라미터만을 허용하며, 콜리전, 서브 이미터, 노이즈, 트레일, 메시 파티클, 커브 에디터, GPU 파티클 등 고비용 연쇄 반응을 일으키는 기능들은 범위에서 엄격히 제외한다. 이러한 범위 제약의 핵심 철학은 "만들고 나서 최적화한다"가 아니라 "비싸질 수 있는 기술적 경로 자체를 처음부터 차단한다"는 정책에 기반한다. 이하에서는 데이터 지향 아키텍처(Data-Oriented Design)와 인스턴스 렌더링(Instanced Rendering)을 결합한 VFX Lite의 제안 구조가 실행 가능성 및 성능 보장 측면에서 타당한지 정밀 검토한다.

## 2. 데이터 중심 아키텍처 및 메모리 최적화 타당성

### 2.1 ECS 엔티티 분리 정책의 기술적 타당성

VFX Lite 사양서는 개별 파티클을 ECS(Entity Component System)의 엔티티로 생성하지 않고, ECS 프레임워크 상에는 이펙트 전체의 설정값만 보관하는 단일 ParticleEffectComponent를 배치한다. 개별 파티클 데이터는 ECS 관리 영역 외부의 EffectInstance 내부 연속 메모리 배열(Particle[])로 독립 관리된다.
개별 파티클마다 ECS 엔티티를 할당할 경우, 파티클 생성 시마다 엔티티 ID 발급, 컴포넌트 매핑, 아키타입(Archetype) 재조정 및 관합성 검사 오버헤드가 발생한다. 더욱이 시스템 순회 시 메모리 불연속성으로 인한 CPU L1/L2 캐시 미스가 극심해질 수 있다. VFX Lite가 채택한 ECS와 파티클 데이터의 분리 정책은 ECS의 유연성을 이펙트 단위의 관리에만 활용하고, 내부 시뮬레이션은 연속된 C++ 구조체 배열을 선형 순회하는 데이터 지향 아키텍처를 구현함으로써 CPU 캐시 적중률을 극대화한다.

### 2.2 파티클 구조체 레이아웃 및 캐시 라인 정렬 분석

CPU 메모리 읽기 성능을 최적화하기 위해서는 파티클 구조체의 크기와 필드 배치 상태가 현대 프로세서의 캐시 라인 크기인 64바이트 환경에 적합한지 검증해야 한다. 사양서에 명시된 32비트 부동소수점(float) 기준의 파티클 구조체 메모리 레이아웃 분석 결과는 다음과 같다.

| 필드명 | 데이터 타입 | 바이트 크기 | 기능 및 비고 |
| :---- | :---- | :---- | :---- |
| position | float3 (x, y, z) | 12 바이트 | 월드/로컬 공간 좌표 |
| velocity | float3 (vx, vy, vz) | 12 바이트 | 속도 및 이동 방향 |
| lifetime | float | 4 바이트 | 현재 경과 수명 |
| maxLifetime | float | 4 바이트 | 파티클 전체 수명 |
| size | float | 4 바이트 | 선형 보간 처리된 현재 크기 |
| rotation | float | 4 바이트 | 회전각 |
| color | float4 (r, g, b, a) | 16 바이트 | 선형 보간 처리된 현재 색상 및 투명도 |
| 합계 | - | 56 바이트 | 64 바이트 캐시 라인 내 수용 (8 바이트 여유) |

단일 Particle 구조체는 총 56바이트의 용량을 차지한다. 이는 표준 64바이트 캐시 라인 하나의 범위 안에 안정적으로 수용되며 8바이트의 여유 공간을 남긴다. 파티클 시뮬레이션 루프가 연속 배열을 선형으로 순회할 때, CPU 하드웨어 스트리밍 프리페처(Hardware Streaming Prefetcher)는 다음 파티클 블록을 L1 데이터 캐시로 적시에 사전 로드할 수 있어 포인터 추적 방식에서 발생하는 메모리 대기 시간(Latency)을 제거한다.

### 2.3 O(1) 스왑-리무브 및 메모리 재할당 배제

수명을 다한 파티클을 배열에서 제거할 때 std::vector::erase와 같은 메모리 이동 연산을 사용하면 $O(N)$ 시간 복잡도가 유발되며 연속 메모리의 밀집도가 흐트러진다. VFX Lite는 수명이 다한 파티클의 인덱스 위치로 배열 최외곽(Tail)에 존재하는 살아있는 파티클을 복사한 후 배열의 유효 크기 변수를 1 감소시키는 스왑-리무브(Swap-Remove) 방식을 적용한다.
이 방식은 파티클 순서가 시뮬레이션 결과에 영향을 주지 않는다는 점을 활용하여 제거 연산을 $O(1)$의 정수 시간 복잡도로 즉시 처리한다. 또한 이펙트 생성 시점에 particles.reserve(maxParticle)를 단 1회 수행하여 공간을 확보함으로써, 런타임 수명주기 전반에서 malloc, free, new, delete 및 동적 배열 재할당을 완전히 배제한다. 이는 프레임 지연을 일으키는 힙 파편화와 메모리 할당자 오버헤드를 원천적으로 방지한다.

## 3. 렌더링 파이프라인 및 CPU-GPU 대역폭 분석

### 3.1 InstancedBatchManager 연동 및 인스턴스 데이터 레이아웃

VFX Lite는 기존 엔진에 구현된 인스턴싱 렌더링 경로인 InstancedBatchManager를 재사용한다. 단일 쿼드(Quad) 메시를 기반으로 각 파티클의 변형 정보를 인스턴스 버퍼로 변환하여 GPU로 전송하는 구조를 취한다.
렌더링 단계에서 GPU로 전송되는 파티클당 인스턴스 데이터의 구성을 분석하면 다음과 같다.

| 필드명 | 데이터 타입 | 바이트 크기 | GPU 버텍스 셰이더 전달 정보 |
| :---- | :---- | :---- | :---- |
| instancePosition | float3 | 12 바이트 | 인스턴스 월드 위치 |
| instanceSize | float | 4 바이트 | 인스턴스 스케일 |
| instanceRotation | float | 4 바이트 | Z축 회전 각도 |
| instanceColor | float4 | 16 바이트 | 버텍스/인스턴스 컬러 및 알파 값 |
| 인스턴스 합계 | - | 36 바이트 | 파티클당 GPU 버퍼 전송량 |

### 3.2 CPU-GPU 메모리 대역폭 정량 평가

10,000개의 활성 파티클이 초당 60프레임으로 구동되는 환경에서 발생 시뮬레이션 및 GPU 버퍼 전송 시 요구되는 메모리 대역폭의 수학적 계산식은 다음과 같다.
$$\text{Data Transfer Rate} = N_{\text{particles}} \times \left( S_{\text{update}} + S_{\text{instance}} \right) \times \text{FPS}$$
상기 수식에서 $S_{\text{update}}$는 CPU 시뮬레이션 구조체 크기(56바이트)이며, $S_{\text{instance}}$는 GPU 인스턴스 버퍼 구조체 크기(36바이트)이다.
$$\text{Bandwidth}_{\text{CPU Update}} = 10,000 \times 56 \text{ Bytes} \times 60 \text{ Hz} = 33.60 \text{ MB/s}$$
$$\text{Bandwidth}_{\text{GPU Upload}} = 10,000 \times 36 \text{ Bytes} \times 60 \text{ Hz} = 21.60 \text{ MB/s}$$
$$\text{Bandwidth}_{\text{Total}} = 33.60 \text{ MB/s} + 21.60 \text{ MB/s} = 55.20 \text{ MB/s}$$
정량 분석 결과, 10,000개의 파티클을 매 프레임 업데이트하고 GPU로 전송하는 데 소요되는 총 대역폭은 초당 약 55.20 MB(52.64 MiB) 수준에 불과하다. 이는 현대 PCIe 3.0/4.0 x16 인터페이스가 제공하는 대역폭(16~32 GB/s)의 0.3% 미만에 해당하므로, 데이터 전송이 GPU 파이프라인의 병목 요인으로 작용할 가능성은 극히 낮다.

### 3.3 깊이 정렬 부재에 따른 투명도 렌더링 한계 분석

사양서의 범위 제약 조건에 따라 CPU 기반의 정교한 깊이 정렬(Depth Sorting) 연산은 명시적으로 포함되지 않는다. 알파 블렌딩(Alpha Blending)이 적용되는 투명 스프라이트를 정렬 없이 렌더링하는 경우, 카메라와의 거리에 따른 원근 순서(Back-to-Front)가 보장되지 않아 뒤쪽 파티클이 앞쪽 파티클을 덮어버리거나 깊이 버퍼(Z-Buffer) 가림 현상으로 인한 시각적 아티팩트가 유발될 수 있다.
CPU에서 매 프레임 파티클을 정렬할 경우 $O(N \log N)$의 연산 비용이 발생하고 연속 메모리 접근 순서가 흐트러져 성능 철학에 위배된다. 이에 따라 다음과 같은 표준 가이드라인을 설정하여 정렬 부재의 한계를 보완하는 것이 기술적으로 타당하다.

1. 가산 혼합(Additive Blending) 표준화: 불꽃, 스파크, 마법 이펙트 등 주요 연출에 Blend One One 형태의 가산 혼합을 적용한다. 가산 연산은 교환법칙이 성립하므로 렌더링 순서와 무관하게 동일한 결과값을 생성한다.
2. 깊이 쓰기 비활성화(Z-Write Off): 반투명 연기 및 먼지 효과 렌더링 시 깊이 테스트(Z-Test)는 유지하되 깊이 기록(Z-Write)을 비활성화함으로써, 뒤쪽 물체의 픽셀이 렌더링 단계에서 마스킹되어 잘려 나가는 엄격한 투과 오류를 방지한다.

## 4. 직렬화, 에디터 및 생명주기 관리 정책

### 4.1 프리팹 시스템 연동 및 직렬화 경로

사양서는 새로운 .vfx.json 포맷을 생성하지 않고 기존 엔진의 프리팹 메커니즘을 그대로 활용하는 방식을 채택한다. 파티클의 18개 설정값을 보관하는 ParticleEffectComponent POD 구조체를 리플렉션 매크로(GE_BEGIN_COMPONENT, GE_FIELD)로 등록함으로써, 별도의 파서 개발 없이 ComponentRegistry 기반의 JSON 직렬화 및 역직렬화 기능을 확보한다.
이 설계는 기존 프리팹 시스템의 캡처(CaptureFromEntity), 스폰(SpawnInto), 적용(ApplyToEntity), 되돌리기(Revert) 엔진 파이프라인에 그대로 연결된다. 런타임에 파티클 시뮬레이션을 담당하는 ParticleSystem은 프리팹을 통해 복원된 컴포넌트의 초기값을 읽어 EffectInstance를 구동하므로 저장 및 복원 계층과 시뮬레이션 계층이 명확히 분리된다.

### 4.2 자원 상한 및 신규 생성 거부 정책

상한 초과 시 오래된 파티클을 강제 소멸시키는 순환 버퍼(Ring Buffer) 방식 대신, 신규 생성 거부(Skip Spawn) 정책이 확정되었다.
순환 버퍼 방식은 폭발이나 피격 연출처럼 수명이 짧은 고밀도 이펙트에서 기존 파티클의 수명을 채우지 못하고 갑자기 화면에서 사라지는 부자연스러운 시각 왜곡을 발생시킨다. 반면 신규 생성 거부 정책은 이미 생성된 파티클의 생명주기를 완결성 있게 보장하며, 지정된 메모리 공간 내에서 시뮬레이션을 제어하여 정해진 리소스 상한을 절대 초과하지 않도록 한다.
엔진 전역 차원에서 관리되는 이펙트 상한 정책(Max Active Effects, Max Total Particles) 역시 개별 이펙트의 무분별한 중복 발동으로 인해 전체 시스템 프레임이 급격히 저하되는 충격 전파를 방지하는 실용적인 방어선 역할을 수행한다.

### 4.3 에디터 예측 수량 계산 및 안전 상한 강제

에디터 입력 폼에서 파티클의 평균 존재 수량을 사전에 계산하여 실시간으로 안내하는 수식의 타당성을 검증한다.
$$\text{Expected Average Count} \approx \text{Spawn Rate} \times \text{Lifetime} + \text{Burst Count}$$
예를 들어 초당 생성률(Spawn Rate)이 300, 수명(Lifetime)이 3.0초, 일시 방출량(Burst)이 50으로 설정된 경우, 예측 평균 파티클 수는 950개로 산출된다. 설정된 Max Particle 값이 상기 어림값 대비 지나치게 낮거나 과도하게 크게 설정되었을 때 경고를 출력함으로써, 무거운 파티클 설정이 게임 빌드에 반영되는 것을 제작 단계에서 차단한다. 또한 에디터 프리뷰 환경에도 하드 상한(예: 2,048개)을 강제 적용하여 오설정으로 인한 에디터 멈춤 현상을 선제적으로 방지한다.

## 5. 텍스처 및 머티리얼 파이프라인 우회 전략

### 5.1 고아 아키텍처 문제 및 미해결 항목 분석

현재 저장소의 자산 관리 모듈(engine/asset/)은 코드 베이스에 포함되어 있으나 실질적인 호출 경로가 차단된 고아 모듈 상태이다. 사양서는 거대한 자산 관리 시스템 전체를 복구하는 위험을 피하기 위해, VFX Lite 전용의 최소 스프라이트/텍스처 로딩 파이프라인을 단독 구축하는 방향을 검토 과제로 남겨두었다.

### 5.2 우회 파이프라인의 구조적 설계 가이드라인

VFX Lite 전용의 단독 텍스처 로더를 구현할 경우, 추후 정식 자산 관리 시스템이 구현되었을 때 대규모 재작업이 발생하지 않도록 상호운용성을 고려한 인터페이스 설계가 필요하다.
VFX Lite의 최소 텍스처 로더는 이미지 파일로부터 픽셀 데이터를 직접 읽어 GPU 텍스처 핸들을 생성하되, 생성된 ID를 InstancedBatchKey의 materialId 필드에 매핑하도록 구성한다. 이러한 구조는 추후 정식 자산 관리 모듈이 정상화되더라도 배치 키 기반의 인스턴스 렌더링 레이어 변경 없이 텍스처 공급자만 유연하게 교체할 수 있는 확장성을 제공한다.

## 6. 결론 및 종합 실행 권고사항

### 6.1 최적화 타당성 평가 요약

본 보고서에서 사양서의 주요 아키텍처 결정 사항과 기술적 타당성을 분석 및 정량 평가한 결과는 다음과 같이 요약된다.

| 평가 항목 | 제안된 설계 방식 | 기술적 타당성 평가 | 핵심 효과 및 근거 |
| :---- | :---- | :---- | :---- |
| 메모리 구조 | non-ECS, 연속 C++ 배열(Particle[]) | 우수 (Verified) | 개별 엔티티 오버헤드 완벽 제거, L1 데이터 캐시 효율 극대화 |
| 캐시 정렬 | 56 Byte 구조체 레이아웃 | 우수 (Verified) | 64 Byte 캐시 라인 내 완벽 수용, 프리페치 효율 최적화 |
| 파티클 삭제 | $O(1)$ Swap-Remove 연산 | 우수 (Verified) | 메모리 이동 연산 최소화 및 프레임 내 동적 할당 배제 |
| 렌더링 방식 | InstancedBatchManager 재사용 | 우수 (Verified) | Draw Call 최소화, 10k 파티클 기준 55.20 MB/s의 낮은 대역폭 소요 |
| 깊이 정렬 | 정렬 비활성화 (No Depth Sorting) | 조건부 승인 | 알파 블렌딩 오류 가능성 존재. Z-Write Off 및 가산 혼합 방식 적용 필수 |
| 직렬화 | 기존 Prefab 시스템 100% 재활용 | 우수 (Verified) | 별도 포맷 개발 없이 컴포넌트 리플렉션 시스템 호환성 확보 |
| 자원 제어 | 신규 생성 거부 (Skip Spawn) | 우수 (Verified) | 시각적 완결성 보장 및 메모리 한계 상한 엄격 준수 |

### 6.2 차기 구현 계획서 작성 지침

'VFX Lite 파티클 시스템 범위 정의서'는 데이터 중심 설계와 인스턴스 렌더링 기법을 결합하여 복잡도를 최적으로 제한한 실용적인 아키텍처 문서이다. "비싸질 수 있는 경로를 만들지 않는다"는 설계 철학은 시스템의 장기적인 성능 안정성을 보장한다.
추후 작성될 docs/VFX_LITE_IMPLEMENTATION_PLAN.md 구현 계획서에서는 다음 사항이 구체적으로 반영되어야 한다.

1. 렌더 상태 처리의 명시적 고정: 렌더링 시 Z-Write를 비활성화하고 가산 혼합(Additive Blend) 방식을 기본 렌더 모드로 제공하여 깊이 정렬 부재로 인한 시각적 왜곡을 제어한다.
2. 인스턴스 버퍼 이중화(Double Buffering): CPU에서 인스턴스 데이터를 구성하는 동안 GPU가 이전 프레임의 버퍼를 독립적으로 렌더링할 수 있도록 이중 버퍼 구조를 적용하여 CPU-GPU 동기화 병목을 방지한다.
3. 텍스처 핸들 구조의 추상화: 우회 텍스처 로더 구현 시 생성된 식별자를 InstancedBatchKey의 materialId와 직접 연동하여, 향후 자산 관리 모듈 정비 시 파괴적 코드 수정이 발생하지 않도록 보호한다.

이상의 조건들이 충실히 이행된다면, VFX Lite 파티클 시스템은 최소한의 개발 자원 투입으로 높은 성능과 안정적인 시각적 연출력을 성공적으로 제공할 것이다.

---

## 부록 A. 구현 초안 요약

- 데이터 중심 구조: 파티클은 ECS 엔티티가 아니라 이펙트 인스턴스 내부 배열에 저장한다.
- 렌더링: InstancedBatchManager의 인스턴스 경로를 재사용한다.
- 제외 범위: 콜리전, 서브 이미터, 노이즈, 트레일, 메시 파티클, GPU 파티클, 커브 에디터 등은 제외한다.
- 메모리 정책: reserve 1회, swap-remove, 동적 할당 금지.
- 안전장치: Max Particle, Max Total Particles, Skip Spawn, 에디터 예측 수량 경고.
- 핵심 결론: 비용이 비싼 경로를 사전에 차단하는 것이 VFX Lite의 본질적 가치다.

