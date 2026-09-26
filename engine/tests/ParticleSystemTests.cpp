// VFX Lite Phase 2 — 파티클 시뮬레이션 테스트.
// docs/VFX_LITE_IMPLEMENTATION_PLAN.md §3 "Phase 2"의 검증 항목.
//
// 이 Phase는 렌더링을 하지 않으므로 GL 컨텍스트 없이 전부 검증된다. 계획서가 Phase 2를
// Phase 3(렌더링)보다 앞에 둔 이유가 이것 — 여기서 시뮬레이션이 옳다는 걸 확정해두면,
// Phase 3에서 화면이 이상할 때 원인이 렌더링 쪽임을 알고 시작할 수 있다.

#include <gtest/gtest.h>
#include "../ecs/ParticleSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include <glm/geometric.hpp>
#include <algorithm>

using namespace Engine;

namespace
{
    // 지속 방출 없이 Burst만 쓰는 이펙트. 상한/수명/종료 조건 테스트의 기본형.
    ParticleEffectComponent BurstOnly(int burst, int maxParticle, float lifetime)
    {
        ParticleEffectComponent s;
        s.spawnRate       = 0.0f;
        s.burst           = burst;
        s.maxParticle     = maxParticle;
        s.lifetime        = lifetime;
        s.gravity         = 0.0f;
        s.drag            = 0.0f;
        s.speedVariance   = 0.0f;
        s.sizeVariance    = 0.0f;
        s.rotationVariance = 0.0f;
        return s;
    }

    // 지속 방출만 쓰는 이펙트.
    ParticleEffectComponent RateOnly(float rate, int maxParticle, float lifetime)
    {
        ParticleEffectComponent s = BurstOnly(0, maxParticle, lifetime);
        s.spawnRate = rate;
        return s;
    }

    Entity MakeEffectEntity(ECSRegistry& registry, const ParticleEffectComponent& settings)
    {
        Entity e = registry.CreateEntity();
        registry.AddComponent(e, settings);
        return e;
    }
}

// ── 자료구조 계약 ──────────────────────────────────────────────────────────────

TEST(ParticleSystemTest, ParticleStaysFiftySixBytes)
{
    // docs/VFX_LITE_PLAN.md §7.4의 결정이 나중에 조용히 뒤집히는 것을 막는다.
    // 56 stride면 파티클 8개가 448바이트 = 정확히 7개 캐시라인이지만, "라인 경계를
    // 걸치니까 정렬하자"며 64로 패딩하면 8개가 8라인이 되어 오히려 더 읽는다.
    // (헤더의 static_assert와 중복이지만, 의도를 테스트 이름으로도 남겨둔다.)
    EXPECT_EQ(sizeof(Particle), 56u);
}

// ── 이펙트 생성/동기화 ─────────────────────────────────────────────────────────

TEST(ParticleSystemTest, CreatesOneEffectInstancePerComponentAndReservesOnce)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, BurstOnly(10, 64, 1.0f));
    system.Update(registry, 0.016f);

    ASSERT_EQ(system.GetEffectCount(), 1u);
    const EffectInstance* effect = system.FindEffect(e.id);
    ASSERT_NE(effect, nullptr);

    // §5.5: 생성 시 reserve(maxParticle) 한 번.
    EXPECT_GE(effect->particles.capacity(), 64u);
}

TEST(ParticleSystemTest, DropsEffectWhenComponentIsRemoved)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(50.0f, 64, 1.0f));
    system.Update(registry, 0.1f);
    ASSERT_EQ(system.GetEffectCount(), 1u);

    registry.RemoveComponent<ParticleEffectComponent>(e);
    system.Update(registry, 0.1f);

    EXPECT_EQ(system.GetEffectCount(), 0u);
    EXPECT_EQ(system.FindEffect(e.id), nullptr);
}

// ── 메모리 정책 (§5.5) ─────────────────────────────────────────────────────────

TEST(ParticleSystemTest, CapacityNeverChangesDuringRuntime)
{
    // 런타임 수명주기 전반에서 벡터 재할당이 없어야 한다. reserve 이후 capacity가
    // 한 번이라도 변하면 프레임 중에 힙 할당이 일어났다는 뜻이다.
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(500.0f, 128, 0.5f));
    system.Update(registry, 0.016f);

    const size_t capacityAfterReserve = system.FindEffect(e.id)->particles.capacity();
    ASSERT_GE(capacityAfterReserve, 128u);

    for (int frame = 0; frame < 200; ++frame)
    {
        system.Update(registry, 0.016f);
        EXPECT_EQ(system.FindEffect(e.id)->particles.capacity(), capacityAfterReserve)
            << "frame " << frame << " — 런타임 중 벡터가 재할당됐다(§5.5 위반)";
    }
}

// ── Max Particle 하드 상한 (§5.2) ──────────────────────────────────────────────

TEST(ParticleSystemTest, AliveCountNeverExceedsMaxParticle)
{
    ECSRegistry registry;
    ParticleSystem system;

    // 어림값(Rate x Lifetime = 5000)이 상한(100)을 한참 넘도록 일부러 과하게 설정한다.
    Entity e = MakeEffectEntity(registry, RateOnly(1000.0f, 100, 5.0f));

    for (int frame = 0; frame < 300; ++frame)
    {
        system.Update(registry, 0.016f);
        EXPECT_LE(system.FindEffect(e.id)->particles.size(), 100u)
            << "frame " << frame;
    }
}

TEST(ParticleSystemTest, BurstLargerThanMaxParticleIsClampedNotQueued)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, BurstOnly(500, 50, 10.0f));
    system.Update(registry, 0.016f);

    EXPECT_EQ(system.FindEffect(e.id)->particles.size(), 50u);
}

TEST(ParticleSystemTest, LivingParticlesAreNeverKilledEarlyWhenCapIsReached)
{
    // §5.2의 핵심 — "신규 생성 거부"이지 "순환 버퍼"가 아니다. 상한에 도달해도 이미
    // 살아있는 파티클은 수명을 정상적으로 마쳐야 한다. 순환 버퍼로 구현했다면 상한
    // 도달 이후 가장 오래된 파티클이 lifetime을 못 채우고 사라졌을 것이다.
    ECSRegistry registry;
    ParticleSystem system;

    // lifetime 10초짜리 파티클을 상한까지 채운 뒤 계속 방출을 시도한다.
    Entity e = MakeEffectEntity(registry, RateOnly(1000.0f, 20, 10.0f));

    system.Update(registry, 0.05f);  // 첫 프레임에 상한(20)까지 채워진다
    ASSERT_EQ(system.FindEffect(e.id)->particles.size(), 20u);

    // 이후 2초 동안(파티클 수명 10초의 한참 이전) 개수가 20에서 내려가면 안 된다.
    for (int frame = 0; frame < 40; ++frame)
    {
        system.Update(registry, 0.05f);
        EXPECT_EQ(system.FindEffect(e.id)->particles.size(), 20u)
            << "frame " << frame << " — 상한 도달 후 살아있는 파티클이 조기 제거됐다";
    }
}

TEST(ParticleSystemTest, BlockedSpawnsAreDiscardedNotBackloggedIntoABurst)
{
    // 구현 계획서 §2.8. 상한에 걸려 못 만든 몫을 누적해두면, 파티클이 죽어 자리가 나는
    // 순간 밀린 물량이 한꺼번에 터진다. 그건 §5.2가 원한 "초과분은 그냥 생성되지 않음"이
    // 아니라 "초과분이 나중에 몰아서 생성됨"이다.
    ECSRegistry registry;
    ParticleSystem system;

    // 상한 10, 수명 0.5초, 방출률 1000/s — 상한에 계속 걸리는 상황.
    Entity e = MakeEffectEntity(registry, RateOnly(1000.0f, 10, 0.5f));

    for (int frame = 0; frame < 30; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    // 파티클이 대량으로 죽는 시점을 지나도 상한을 넘는 순간이 없어야 한다.
    for (int frame = 0; frame < 60; ++frame)
    {
        system.Update(registry, 0.05f);
        EXPECT_LE(system.FindEffect(e.id)->particles.size(), 10u)
            << "frame " << frame << " — 밀린 스폰이 몰아서 생성됐다";
    }
}

// ── 수명과 종료 조건 (§5.8 / §5.11) ────────────────────────────────────────────

TEST(ParticleSystemTest, BurstEffectBecomesInactiveWhenLastParticleDies)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, BurstOnly(8, 64, 0.5f));

    system.Update(registry, 0.1f);
    ASSERT_EQ(system.FindEffect(e.id)->particles.size(), 8u);
    EXPECT_TRUE(system.FindEffect(e.id)->active);

    // 수명(0.5초)을 넘겨 돌린다.
    for (int frame = 0; frame < 10; ++frame)
    {
        system.Update(registry, 0.1f);
    }

    const EffectInstance* effect = system.FindEffect(e.id);
    ASSERT_NE(effect, nullptr);
    EXPECT_EQ(effect->particles.size(), 0u);
    EXPECT_FALSE(effect->active) << "마지막 파티클이 죽었는데 이펙트가 비활성화되지 않았다(§5.11)";
    EXPECT_EQ(system.GetActiveEffectCount(), 0u);
}

TEST(ParticleSystemTest, ContinuousEffectStaysActiveIndefinitely)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(20.0f, 64, 0.2f));

    for (int frame = 0; frame < 100; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    // Rate > 0이면 파티클이 잠깐 0개가 되더라도 이펙트는 계속 살아있어야 한다(§5.11).
    EXPECT_TRUE(system.FindEffect(e.id)->active);
}

TEST(ParticleSystemTest, BurstFiresOnlyOnce)
{
    ECSRegistry registry;
    ParticleSystem system;

    // 수명을 길게 줘서 죽지 않게 한 뒤, 여러 프레임 돌려도 Burst가 반복되지 않는지 본다.
    Entity e = MakeEffectEntity(registry, BurstOnly(7, 256, 100.0f));

    for (int frame = 0; frame < 20; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    EXPECT_EQ(system.FindEffect(e.id)->particles.size(), 7u)
        << "Burst가 매 프레임 반복 발동했다(§5.11 — 이펙트당 한 번)";
}

// ── 스폰 누적 (§2.8) ───────────────────────────────────────────────────────────

TEST(ParticleSystemTest, FractionalSpawnRateAccumulatesInsteadOfStalling)
{
    // rate * dt < 1인 상황. 소수부를 버리면 방출이 아예 멈춘다.
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(10.0f, 256, 100.0f));

    // 10/s x 0.016s = 0.16개/프레임 — 매 프레임 floor하면 영원히 0개다.
    for (int frame = 0; frame < 100; ++frame)
    {
        system.Update(registry, 0.016f);
    }

    // 1.6초 동안 약 16개.
    const size_t alive = system.FindEffect(e.id)->particles.size();
    EXPECT_GE(alive, 14u) << "소수부가 버려져 방출이 멈췄다(§2.8)";
    EXPECT_LE(alive, 18u);
}

// ── 적분과 보간 (§4.1) ─────────────────────────────────────────────────────────

TEST(ParticleSystemTest, GravityAccumulatesIntoVelocity)
{
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(1, 8, 100.0f);
    s.initialSpeed    = 0.0f;
    s.gravity         = -10.0f;
    s.drag            = 0.0f;
    s.directionSpread = 0.0f;
    Entity e = MakeEffectEntity(registry, s);

    system.Update(registry, 0.1f);   // 스폰 + 1회 적분
    system.Update(registry, 0.1f);

    const Particle& p = system.FindEffect(e.id)->particles[0];
    EXPECT_NEAR(p.velocity.y, -2.0f, 1e-4f);   // -10 * 0.1 * 2회
    EXPECT_LT(p.position.y, 0.0f);
}

TEST(ParticleSystemTest, DragDecaysVelocityAndZeroDragDoesNotStopParticles)
{
    // 범위 정의서 §4.1의 스케치는 `velocity *= drag`지만, 그대로면 drag=0에서 파티클이
    // 즉시 멈춘다 — "감쇠 계수"(§2.2)라는 이름과 정반대다. dt 비례 감쇠로 구현했다.
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent noDrag = BurstOnly(1, 8, 100.0f);
    noDrag.initialSpeed    = 10.0f;
    noDrag.gravity         = 0.0f;
    noDrag.drag            = 0.0f;
    noDrag.directionBase   = Vec3(0.0f, 1.0f, 0.0f);
    noDrag.directionSpread = 0.0f;

    ParticleEffectComponent withDrag = noDrag;
    withDrag.drag = 2.0f;

    Entity a = MakeEffectEntity(registry, noDrag);
    Entity b = MakeEffectEntity(registry, withDrag);

    for (int frame = 0; frame < 10; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    const Particle& pa = system.FindEffect(a.id)->particles[0];
    const Particle& pb = system.FindEffect(b.id)->particles[0];

    EXPECT_NEAR(pa.velocity.y, 10.0f, 1e-3f) << "drag=0인데 속도가 줄었다";
    EXPECT_LT(pb.velocity.y, pa.velocity.y)  << "drag>0인데 속도가 줄지 않았다";
    EXPECT_GT(pb.velocity.y, 0.0f)           << "감쇠가 속도를 뒤집었다";
}

TEST(ParticleSystemTest, SizeAndColorInterpolateOverLifetime)
{
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(1, 8, 1.0f);
    s.initialSpeed = 0.0f;
    s.startSize    = 1.0f;
    s.endSize      = 0.0f;
    s.startColor   = Vec3(1.0f, 0.0f, 0.0f);
    s.endColor     = Vec3(0.0f, 0.0f, 1.0f);
    s.startAlpha   = 1.0f;
    s.endAlpha     = 0.0f;
    Entity e = MakeEffectEntity(registry, s);

    // 스폰 후 0.5초 경과 → t = 0.5
    system.Update(registry, 0.25f);
    system.Update(registry, 0.25f);

    const Particle& p = system.FindEffect(e.id)->particles[0];
    EXPECT_NEAR(p.size,    0.5f, 1e-4f);
    EXPECT_NEAR(p.color.r, 0.5f, 1e-4f);
    EXPECT_NEAR(p.color.b, 0.5f, 1e-4f);
    EXPECT_NEAR(p.color.a, 0.5f, 1e-4f);
}

TEST(ParticleSystemTest, SizeVarianceSurvivesPerFrameInterpolation)
{
    // sizeScale을 파티클마다 들고 있지 않으면, 스폰 때 준 Size ± 변동폭이 다음 프레임의
    // Lerp(startSize, endSize, t)에 그대로 덮어써진다 — 컴파일도 되고 크래시도 안 나면서
    // "Size ±만 조용히 안 먹는" 버그가 됐을 자리다(ParticleSystem.h의 Particle 주석 참고).
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(64, 64, 100.0f);
    s.initialSpeed  = 0.0f;
    s.startSize     = 1.0f;
    s.endSize       = 1.0f;   // 보간을 상수로 만들어 변동폭만 남긴다
    s.sizeVariance  = 0.5f;
    Entity e = MakeEffectEntity(registry, s);

    system.Update(registry, 0.05f);
    system.Update(registry, 0.05f);   // 보간이 한 번 이상 돈 뒤에도 변동폭이 남아야 한다

    const auto& particles = system.FindEffect(e.id)->particles;
    ASSERT_EQ(particles.size(), 64u);

    float minSize = particles[0].size;
    float maxSize = particles[0].size;
    for (const Particle& p : particles)
    {
        minSize = std::min(minSize, p.size);
        maxSize = std::max(maxSize, p.size);
    }

    EXPECT_GT(maxSize - minSize, 0.1f)
        << "Size ±가 사라졌다 — 파티클 크기가 전부 같아졌다";
    EXPECT_GE(minSize, 0.5f - 1e-3f);
    EXPECT_LE(maxSize, 1.5f + 1e-3f);
}

// ── Direction (§2.2) ───────────────────────────────────────────────────────────

TEST(ParticleSystemTest, ZeroSpreadEmitsExactlyAlongBaseDirection)
{
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(16, 32, 100.0f);
    s.initialSpeed    = 5.0f;
    s.gravity         = 0.0f;
    s.drag            = 0.0f;
    s.directionBase   = Vec3(0.0f, 0.0f, 1.0f);
    s.directionSpread = 0.0f;
    Entity e = MakeEffectEntity(registry, s);

    system.Update(registry, 0.01f);

    for (const Particle& p : system.FindEffect(e.id)->particles)
    {
        EXPECT_NEAR(p.velocity.x, 0.0f, 1e-4f);
        EXPECT_NEAR(p.velocity.y, 0.0f, 1e-4f);
        EXPECT_NEAR(p.velocity.z, 5.0f, 1e-3f);
    }
}

TEST(ParticleSystemTest, WideSpreadProducesVariedDirectionsWithPreservedSpeed)
{
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(64, 64, 100.0f);
    s.initialSpeed    = 4.0f;
    s.gravity         = 0.0f;
    s.drag            = 0.0f;
    s.speedVariance   = 0.0f;
    s.directionBase   = Vec3(0.0f, 1.0f, 0.0f);
    s.directionSpread = 360.0f;   // 전방위
    Entity e = MakeEffectEntity(registry, s);

    system.Update(registry, 0.01f);

    const auto& particles = system.FindEffect(e.id)->particles;
    ASSERT_EQ(particles.size(), 64u);

    bool sawNegativeY = false;
    for (const Particle& p : particles)
    {
        // 방향만 흩어지고 속력은 보존돼야 한다(speedVariance = 0).
        EXPECT_NEAR(glm::length(p.velocity), 4.0f, 1e-3f);
        if (p.velocity.y < 0.0f)
        {
            sawNegativeY = true;
        }
    }
    EXPECT_TRUE(sawNegativeY) << "spread=360인데 기준 방향 반대쪽으로 나간 파티클이 하나도 없다";
}

// ── 전역 상한 (§5.7) ───────────────────────────────────────────────────────────

TEST(ParticleSystemTest, GlobalEffectCapStopsNewEffectInstances)
{
    ECSRegistry registry;
    ParticleSystem system;

    for (size_t i = 0; i < ParticleSystem::kMaxActiveEffects + 10; ++i)
    {
        MakeEffectEntity(registry, BurstOnly(1, 4, 100.0f));
    }

    // 상한을 넘겨 만들지 않는다. (한 프레임에 상한까지만 만들고 멈추므로, 여러 프레임을
    // 돌려도 상한을 넘지 않아야 한다.)
    for (int frame = 0; frame < 5; ++frame)
    {
        system.Update(registry, 0.016f);
        EXPECT_LE(system.GetEffectCount(), ParticleSystem::kMaxActiveEffects);
    }
    EXPECT_EQ(system.GetEffectCount(), ParticleSystem::kMaxActiveEffects);
}

TEST(ParticleSystemTest, GlobalParticleCapLimitsTotalAcrossEffects)
{
    ECSRegistry registry;
    ParticleSystem system;

    // 이펙트당 상한은 넉넉하지만, 전부 합치면 전역 상한을 넘도록 구성한다.
    const size_t perEffectMax = 1000;
    const size_t effectCount  = 40;   // 40 x 1000 = 40,000 > kMaxTotalParticles(20,000)
    for (size_t i = 0; i < effectCount; ++i)
    {
        MakeEffectEntity(registry, BurstOnly(static_cast<int>(perEffectMax),
                                             static_cast<int>(perEffectMax), 100.0f));
    }

    for (int frame = 0; frame < 3; ++frame)
    {
        system.Update(registry, 0.016f);
        EXPECT_LE(system.GetTotalAliveParticles(), ParticleSystem::kMaxTotalParticles)
            << "frame " << frame;
    }
}

// ── 엣지 케이스 ────────────────────────────────────────────────────────────────

TEST(ParticleSystemTest, ZeroOrNegativeMaxParticleSpawnsNothingAndDoesNotCrash)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity zero = MakeEffectEntity(registry, RateOnly(100.0f, 0, 1.0f));
    Entity negative = MakeEffectEntity(registry, RateOnly(100.0f, -5, 1.0f));

    for (int frame = 0; frame < 10; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    EXPECT_EQ(system.FindEffect(zero.id)->particles.size(), 0u);
    EXPECT_EQ(system.FindEffect(negative.id)->particles.size(), 0u);
}

TEST(ParticleSystemTest, ZeroDeltaTimeDoesNotSpawnOrAdvance)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(100.0f, 64, 1.0f));

    system.Update(registry, 0.0f);

    // 이펙트는 만들어지되(다음 프레임을 위해) 파티클은 생기지 않아야 한다.
    ASSERT_NE(system.FindEffect(e.id), nullptr);
    EXPECT_EQ(system.FindEffect(e.id)->particles.size(), 0u);
}

TEST(ParticleSystemTest, ZeroLengthDirectionFallsBackToUpInsteadOfStallingParticles)
{
    // 기준 방향이 영벡터면 방향을 정의할 수 없다. 조용히 0속도 파티클을 만들어 한 점에
    // 뭉치게 두는 대신 위쪽을 기본으로 삼는다.
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(4, 8, 100.0f);
    s.initialSpeed    = 3.0f;
    s.gravity         = 0.0f;
    s.drag            = 0.0f;
    s.speedVariance   = 0.0f;
    s.directionBase   = Vec3(0.0f, 0.0f, 0.0f);
    s.directionSpread = 0.0f;
    Entity e = MakeEffectEntity(registry, s);

    system.Update(registry, 0.01f);

    for (const Particle& p : system.FindEffect(e.id)->particles)
    {
        EXPECT_NEAR(p.velocity.y, 3.0f, 1e-3f);
    }
}

// ── 에디터 Preview 하드 상한 (§5.10, Phase 4) ─────────────────────────────────

TEST(ParticleSystemTest, PreviewCapLimitsASingleEffectRegardlessOfMaxParticle)
{
    // Rate=10000/Lifetime=10 같은 오설정 하나로 에디터가 멈추는 것을 막는 방어선.
    // 전역 상한(kMaxTotalParticles)만으로는 이펙트 하나가 그 전부를 독차지하는 것을
    // 막지 못하므로 이펙트 단위 상한이 따로 필요하다.
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(10000.0f, 100000, 10.0f));

    for (int frame = 0; frame < 30; ++frame)
    {
        system.Update(registry, 0.05f);
        EXPECT_LE(system.FindEffect(e.id)->particles.size(), ParticleSystem::kPreviewMaxParticle)
            << "frame " << frame;
    }
}

TEST(ParticleSystemTest, PreviewCapAlsoBoundsReservedCapacity)
{
    // 상한을 스폰 시점에만 걸고 reserve는 입력값대로 하면, 실제로는 2048개만 쓸 이펙트가
    // 100000개 분량(약 5.6MB)을 미리 잡아버린다.
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, RateOnly(10.0f, 100000, 1.0f));
    system.Update(registry, 0.016f);

    EXPECT_LE(system.FindEffect(e.id)->particles.capacity(), ParticleSystem::kPreviewMaxParticle);
}

TEST(ParticleSystemTest, PreviewCapDoesNotAffectSettingsBelowIt)
{
    // 상한 이하의 정상 설정은 그대로 동작해야 한다.
    ECSRegistry registry;
    ParticleSystem system;

    Entity e = MakeEffectEntity(registry, BurstOnly(500, 500, 100.0f));
    system.Update(registry, 0.016f);

    EXPECT_EQ(system.FindEffect(e.id)->particles.size(), 500u);
}

// ── 월드 공간 시뮬레이션 (Phase 3 계획서 §3.10) ───────────────────────────────

TEST(ParticleSystemTest, SpawnsAtEmitterWorldPositionNotOrigin)
{
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(4, 16, 100.0f);
    s.initialSpeed = 0.0f;
    s.gravity      = 0.0f;
    s.drag         = 0.0f;

    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(5.0f, 2.0f, -3.0f);
    registry.AddComponent(e, t);
    registry.AddComponent(e, s);

    system.Update(registry, 0.01f);

    const auto& particles = system.FindEffect(e.id)->particles;
    ASSERT_EQ(particles.size(), 4u);
    for (const Particle& p : particles)
    {
        EXPECT_NEAR(p.position.x,  5.0f, 1e-4f);
        EXPECT_NEAR(p.position.y,  2.0f, 1e-4f);
        EXPECT_NEAR(p.position.z, -3.0f, 1e-4f);
    }
}

TEST(ParticleSystemTest, AlreadySpawnedParticlesDoNotFollowAMovingEmitter)
{
    // 월드 공간의 핵심 성질. 로컬 공간이었다면 이미 뿜은 파티클이 이미터를 따라가서
    // "이동 잔상"이 잔상이 아니게 된다(§3.10의 표 참고).
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = BurstOnly(1, 16, 100.0f);
    s.initialSpeed = 0.0f;
    s.gravity      = 0.0f;
    s.drag         = 0.0f;

    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(0.0f, 0.0f, 0.0f);
    registry.AddComponent(e, t);
    registry.AddComponent(e, s);

    system.Update(registry, 0.01f);
    ASSERT_EQ(system.FindEffect(e.id)->particles.size(), 1u);

    // 이미터를 멀리 옮긴다.
    registry.GetComponent<TransformComponent>(e)->position = Vec3(100.0f, 0.0f, 0.0f);
    system.Update(registry, 0.01f);

    const Particle& p = system.FindEffect(e.id)->particles[0];
    EXPECT_NEAR(p.position.x, 0.0f, 1e-4f)
        << "이미 뿜은 파티클이 이미터를 따라갔다 - 로컬 공간처럼 동작하고 있다";
}

TEST(ParticleSystemTest, ContinuousEmitterLeavesATrailAlongItsPath)
{
    // 위 성질의 실사용 형태: 이미터가 움직이면서 계속 뿜으면 경로를 따라 파티클이 남는다.
    ECSRegistry registry;
    ParticleSystem system;

    ParticleEffectComponent s = RateOnly(100.0f, 256, 100.0f);
    s.initialSpeed = 0.0f;
    s.gravity      = 0.0f;
    s.drag         = 0.0f;

    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(0.0f, 0.0f, 0.0f);
    registry.AddComponent(e, t);
    registry.AddComponent(e, s);

    for (int frame = 0; frame < 10; ++frame)
    {
        registry.GetComponent<TransformComponent>(e)->position = Vec3(static_cast<float>(frame), 0.0f, 0.0f);
        system.Update(registry, 0.05f);
    }

    const auto& particles = system.FindEffect(e.id)->particles;
    ASSERT_GT(particles.size(), 2u);

    float minX = particles[0].position.x;
    float maxX = particles[0].position.x;
    for (const Particle& p : particles)
    {
        minX = std::min(minX, p.position.x);
        maxX = std::max(maxX, p.position.x);
    }
    EXPECT_GT(maxX - minX, 1.0f) << "이미터가 이동했는데 파티클이 한 지점에만 모여 있다";
}

TEST(ParticleSystemTest, MultipleEffectsSimulateIndependently)
{
    ECSRegistry registry;
    ParticleSystem system;

    Entity shortLived = MakeEffectEntity(registry, BurstOnly(5, 32, 0.2f));
    Entity longLived  = MakeEffectEntity(registry, BurstOnly(5, 32, 100.0f));

    for (int frame = 0; frame < 10; ++frame)
    {
        system.Update(registry, 0.05f);
    }

    EXPECT_EQ(system.FindEffect(shortLived.id)->particles.size(), 0u);
    EXPECT_EQ(system.FindEffect(longLived.id)->particles.size(), 5u);
}
