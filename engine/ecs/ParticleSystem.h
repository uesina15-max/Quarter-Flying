#pragma once

#include "System.h"
#include "Components.h"
#include "Entity.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <unordered_map>
#include <random>
#include <memory>
#include <cstddef>

namespace Engine
{
    // Forward declarations — 이 헤더는 렌더러 타입의 정의를 필요로 하지 않는다.
    class InstancedBatchManager;
    class Camera;
    class Mesh;

    // ============================================================================
    // Particle — 개별 파티클 하나 (docs/VFX_LITE_PLAN.md §4.1 / §5.1)
    // ============================================================================

    // 파티클은 ECS 엔티티도 컴포넌트도 아니다. EffectInstance 내부의 연속 배열로만
    // 존재한다(§5.1) — Max Particle=1,000짜리 이펙트 10개가 동시에 터지면 ECS 엔티티가
    // 10,000개 생기는 것을 피하기 위해서다.
    //
    // 56바이트를 유지한다. docs/VFX_LITE_PLAN.md §7.4대로 **64바이트로 패딩하지 않는다**:
    // stride 56이면 파티클 8개가 448바이트 = 정확히 7개 캐시라인에 들어가지만, 64로
    // 패딩하면 8개가 8라인이 되어 오히려 12.5% 더 읽는다. "라인 경계를 걸치니까 정렬하자"는
    // 직관이 선형 순회에서는 틀렸다.
    //
    // 범위 정의서 §4.1의 자료구조 스케치와 한 곳이 다르다: 거기 있던 maxLifetime 대신
    // sizeScale을 둔다. 이유 —
    //   - maxLifetime은 파티클마다 다를 수 없다. §2.6의 랜덤 파라미터는 Speed/Size/Rotation
    //     세 개뿐이고 "Lifetime ±"는 범위에 없으므로, 모든 파티클의 최대 수명은
    //     EffectInstance가 들고 있는 settings.lifetime과 항상 같다. 파티클마다 같은 값을
    //     복사해 두는 것은 56바이트 중 4바이트를 낭비하는 것이다.
    //   - 반대로 Size ±(§2.6)는 파티클마다 달라야 하는데 저장할 자리가 없었다. 크기는 매
    //     프레임 Lerp(startSize, endSize, t)로 새로 계산되기 때문에, 파티클 고유의 배율을
    //     어딘가 들고 있지 않으면 스폰 때 준 변동폭이 다음 프레임에 그대로 덮어써진다
    //     (컴파일도 되고 크래시도 안 나면서 "Size ±만 조용히 안 먹는" 버그가 됐을 자리다).
    // 그래서 낭비되는 4바이트를 실제로 필요한 4바이트로 맞바꿨다. 56바이트와 §7.4의
    // 캐시라인 계산은 그대로 유지된다.
    struct Particle
    {
        glm::vec3 position;   // 12
        glm::vec3 velocity;   // 12
        float     lifetime;   //  4  남은 수명(초). 0 이하가 되면 죽는다
        float     sizeScale;  //  4  Size ±를 반영한 이 파티클 고유의 크기 배율
        float     size;       //  4  보간된 현재 크기
        float     rotation;   //  4  현재 회전각(도)
        glm::vec4 color;      // 16  보간된 현재 rgb + a
    };

    static_assert(sizeof(Particle) == 56,
                  "Particle must stay 56 bytes -- 8 particles == 448 bytes == exactly 7 cache lines "
                  "(docs/VFX_LITE_PLAN.md 7.4). Do NOT pad to 64.");

    // ============================================================================
    // EffectInstance — 이펙트 하나의 런타임 상태
    // ============================================================================

    struct EffectInstance
    {
        EntityID owner{0};

        // 스폰 시점의 설정 스냅샷. 매 프레임 컴포넌트에서 다시 읽지 않는다 — 시뮬레이션
        // 도중에 lifetime 같은 값이 바뀌면 이미 날아가고 있는 파티클의 t(진행도)가
        // 튀어버리기 때문이다.
        ParticleEffectComponent settings;

        // 생성 시 reserve(maxParticle) 한 번. 이후 런타임 내내 재할당이 없어야 한다(§5.5).
        std::vector<Particle> particles;

        // 초당 생성률의 소수부 누적기(구현 계획서 §2.8). dt가 작을 때 spawnRate*dt < 1이라고
        // 매 프레임 0개를 만들면 방출이 아예 멈추기 때문에 소수부를 들고 간다.
        float spawnAccumulator{0.0f};

        // Burst는 이펙트당 한 번만 터진다(§5.11 — 이펙트 자체의 Lifetime 파라미터는 없고,
        // Burst 이펙트는 마지막 파티클이 죽으면 끝난다).
        bool burstFired{false};

        // 살아있는 파티클이 0개이고 지속 방출도 아니면 false. 시스템 순회에서 통째로
        // 빠진다(§5.8) — 빈 배열을 매 프레임 검사하는 것조차 하지 않는다.
        bool active{true};

        // 이펙트마다 독립된 난수원(구현 계획서 §2.9). 전역 rand()를 쓰지 않는다 —
        // 이펙트 사이의 독립성과 스레드 안전성 때문이다.
        std::mt19937 rng;

        // kPreviewMaxParticle 상한이 이 이펙트에 실제로 걸렸다고 이미 로그를 남겼는지.
        // 매 프레임 같은 경고를 쏟아내지 않기 위한 것이다.
        bool previewCapWarned{false};

        // owner 엔티티의 현재 월드 위치. 매 프레임 TransformComponent에서 갱신한다.
        // **시뮬레이션은 월드 공간이다**(Phase 3 계획서 §3.10): 스폰하는 순간의 이미터
        // 위치를 파티클에 구워 넣고, 그 뒤로는 이미터가 움직여도 이미 뿜은 파티클은
        // 그 자리에 남는다. 폭발·연기·피격·이동 잔상처럼 §2가 커버 목표로 삼은 연출
        // 대부분이 그렇게 동작해야 자연스럽다(특히 "이동 잔상"은 파티클이 이미터를
        // 따라다니면 잔상이 아니게 된다).
        glm::vec3 emitterWorldPos{0.0f};

        size_t AliveCount() const { return particles.size(); }
    };

    // ============================================================================
    // ParticleSystem
    // ============================================================================

    /// <summary>
    /// VFX Lite 파티클 시뮬레이션(docs/VFX_LITE_IMPLEMENTATION_PLAN.md Phase 2).
    ///
    /// 이 클래스는 **그리지 않는다**. ECS의 ParticleEffectComponent를 읽어 EffectInstance를
    /// 만들고, 파티클 배열을 스폰/적분/제거하는 것까지만 한다. 인스턴스 버퍼 조립과 렌더
    /// 상태 전환은 Phase 3의 렌더 경로가 이 결과를 읽어서 한다 — 그래서 이 Phase는 GL
    /// 컨텍스트 없이 유닛테스트만으로 전부 검증된다(Phase 3에서 화면이 이상하면 원인이
    /// 렌더링 쪽임을 확정하고 시작할 수 있다는 것이 이 순서의 목적이다).
    ///
    /// RenderSystem과 마찬가지로 내부 상태(effects_)를 가지므로 병렬 스케줄러에 맡기지
    /// 않는다(CanRunInParallel() == false).
    /// </summary>
    class ParticleSystem : public System
    {
    public:
        // 엔진 전역 성능 정책(§5.7). 이펙트 하나가 조절할 값이 아니므로 §2의 18개
        // 파라미터에 추가하지 않고 에디터에도 노출하지 않는다. 파티클 하나하나보다
        // "동시에 떠 있는 이펙트 개수"가 실제 렉의 원인이 되는 경우가 더 흔하다.
        static constexpr size_t kMaxActiveEffects  = 64;
        static constexpr size_t kMaxTotalParticles = 20000;

        // 에디터 Preview 하드 상한(docs/VFX_LITE_PLAN.md §5.10). 사용자가 Max Particle에
        // 무엇을 넣든 이펙트 하나가 이보다 많은 파티클을 갖지 않는다.
        //
        // 이유: Preview가 입력값을 무제한으로 돌리면 Rate=10000/Lifetime=10 같은 오타 한 번에
        // 에디터가 멈춘다 - **죽은 에디터는 디버깅할 수조차 없으니 가장 나쁜 실패 방식이다.**
        // 위의 kMaxTotalParticles(전역 2만)만으로는 이펙트 하나가 그 전부를 독차지하는 것을
        // 막지 못한다.
        //
        // 지금은 이 엔진의 유일한 소비자가 에디터라서 무조건 적용한다. 게임 런타임이
        // 생기면 그때 이 상한을 런타임에서만 풀도록 조건을 넣는다(지금 미리 플래그를
        // 만들어두지 않는 것은 §3 "필요해지면 그때"와 같은 이유다). 상한이 실제로
        // 걸리면 이펙트당 한 번 경고 로그를 남긴다 - 조용히 다르게 동작하지 않도록.
        static constexpr size_t kPreviewMaxParticle = 2048;

        // 시뮬레이션만 하는 생성자. 유닛테스트는 이걸 쓴다 — GL 자원도 카메라도 없이
        // 스폰/적분/제거 전부를 검증할 수 있다(Phase 2).
        ParticleSystem() = default;

        // 엔진에 연결할 때 쓰는 생성자. RenderSystem과 같은 규약으로 **둘 다 nullptr을
        // 허용한다** — 그 경우 인스턴스 수집을 조용히 건너뛰고 시뮬레이션만 돈다(시각화가
        // 안 될 뿐 크래시로 이어지지 않는다).
        ParticleSystem(InstancedBatchManager* batchManager, Camera* camera);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "ParticleSystem"; }

        // RenderSystem(기본값 0)보다 뒤에 돌아야 한다. RenderSystem이 매 프레임 ECS의
        // Main Camera를 읽어 Camera*를 갱신하는데, 그 전에 돌면 한 프레임 뒤진 카메라로
        // 빌보드를 만들게 되기 때문이다(카메라가 빠르게 돌 때 파티클이 어긋나 보인다).
        // World::RegisterSystem()이 GetPriority() 오름차순으로 정렬하고
        // UpdateSystemsSequential()이 그 순서대로 돌린다(에디터의 Edit 모드는 항상 순차).
        int GetPriority() const override { return 100; }

        // effects_라는 내부 상태를 가진다 — RenderSystem과 같은 이유로 병렬 실행 대상에서
        // 제외한다.
        bool CanRunInParallel() const override { return false; }

        std::vector<size_t> GetReadComponentTypes() const override;

        // ── 조회 (Phase 3의 렌더 경로와 테스트가 쓰는 읽기 전용 경로) ──────────────
        const EffectInstance* FindEffect(EntityID owner) const;
        size_t GetEffectCount() const { return effects_.size(); }
        size_t GetActiveEffectCount() const;
        size_t GetTotalAliveParticles() const;

        const std::unordered_map<EntityID, EffectInstance>& GetEffects() const { return effects_; }

    private:
        void SyncEffectsWithRegistry(ECSRegistry& registry);

        // 살아있는 파티클을 InstancedBatchManager의 인스턴스 배치로 옮긴다.
        // batchManager_/camera_ 중 하나라도 없으면 아무 것도 하지 않는다.
        void CollectInstances();
        bool EnsureBatch();
        static std::shared_ptr<Mesh> CreateQuadMesh();

        void SpawnParticles(EffectInstance& effect, float deltaTime);
        void SpawnOne(EffectInstance& effect);
        void IntegrateAndRemoveDead(EffectInstance& effect, float deltaTime);
        void UpdateActiveState(EffectInstance& effect);

        // 전역 상한(kMaxTotalParticles)까지 고려해 이 이펙트가 지금 몇 개를 더 만들 수
        // 있는지. 0이면 신규 생성을 거부한다(§5.2 — 순환 버퍼로 기존 파티클을 강제
        // 소멸시키지 않는다).
        // const가 아닌 이유: Preview 상한이 실제로 걸린 순간 한 번만 경고를 남기기 위해
        // effect.previewCapWarned를 세운다.
        size_t SpawnBudget(EffectInstance& effect);

        std::unordered_map<EntityID, EffectInstance> effects_;

        // 둘 다 nullptr일 수 있다(위 생성자 주석 참고).
        InstancedBatchManager* batchManager_{nullptr};
        Camera*                camera_{nullptr};

        // 파티클 배치는 엔진 전체에 하나뿐이다(Phase 3 계획서 §3.2) — 모든 파티클이 같은
        // 쿼드/셰이더/블렌드를 쓰므로 Draw Call이 이펙트 개수와 무관하게 1개가 된다.
        // 메시 생성은 GL 컨텍스트가 확실히 있는 시점(첫 수집)까지 미룬다.
        std::shared_ptr<Mesh> quadMesh_;
        bool batchReady_{false};
    };
}
