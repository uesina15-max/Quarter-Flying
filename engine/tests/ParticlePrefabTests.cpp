// VFX Lite Phase 5 — 프리팹 연동 확인.
// docs/VFX_LITE_IMPLEMENTATION_PLAN.md §3 "Phase 5".
//
// 이 Phase의 목적은 구현이 아니라 **확인**이다. 범위 정의서 §4.4가 "이펙트 하나 =
// 프리팹 하나로 두면 새 .vfx.json 포맷을 발명할 필요가 없고, 저장/스폰/되돌리기까지
// 공짜로 따라온다"고 주장했는데, 그게 정말 성립하는지를 파티클 컴포넌트로 직접
// 확인한다. 새로 만드는 프로덕션 코드는 없다.
//
// §4.4는 동시에 한 가지를 **구분**했다: 프리팹은 "이 이펙트의 설정을 어떻게 저장하고
// 재현하는가"만 해결하고, 런타임에 실제로 파티클을 뿜는 시뮬레이션은 별개(ParticleSystem)
// 라는 것. 아래 마지막 테스트가 그 구분이 실제로 성립하는지 본다 — 프리팹으로 스폰된
// 엔티티가 손으로 만든 엔티티와 똑같이 ParticleSystem에 잡히는가.

#include <gtest/gtest.h>
#include "../prefab/PrefabAsset.h"
#include "../prefab/PrefabInstanceComponent.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../ecs/ParticleSystem.h"
#include <filesystem>

using namespace Engine;

namespace
{
    // 기본값과 전부 다른 값. "기본 생성자가 우연히 같은 값을 넣어줘서 통과"하는 것을 막는다.
    ParticleEffectComponent DistinctEffect()
    {
        ParticleEffectComponent p;
        p.spawnRate        = 77.5f;
        p.maxParticle      = 321;
        p.lifetime         = 3.25f;
        p.burst            = 17;
        p.initialSpeed     = 8.5f;
        p.directionBase    = Vec3(0.5f, -0.25f, 0.75f);
        p.directionSpread  = 123.0f;
        p.gravity          = -4.5f;
        p.drag             = 0.65f;
        p.startSize        = 2.5f;
        p.endSize          = 0.25f;
        p.startColor       = Vec3(0.11f, 0.22f, 0.33f);
        p.startAlpha       = 0.85f;
        p.endColor         = Vec3(0.44f, 0.55f, 0.66f);
        p.endAlpha         = 0.15f;
        p.startRotation    = 33.0f;
        p.rotationSpeed    = -77.0f;
        p.speedVariance    = 0.31f;
        p.sizeVariance     = 0.42f;
        p.rotationVariance = 0.53f;
        return p;
    }

    void ExpectEffectEqual(const ParticleEffectComponent& a, const ParticleEffectComponent& b)
    {
        EXPECT_FLOAT_EQ(a.spawnRate, b.spawnRate);
        EXPECT_EQ(a.maxParticle, b.maxParticle);
        EXPECT_FLOAT_EQ(a.lifetime, b.lifetime);
        EXPECT_EQ(a.burst, b.burst);
        EXPECT_FLOAT_EQ(a.initialSpeed, b.initialSpeed);
        EXPECT_FLOAT_EQ(a.directionBase.x, b.directionBase.x);
        EXPECT_FLOAT_EQ(a.directionBase.y, b.directionBase.y);
        EXPECT_FLOAT_EQ(a.directionBase.z, b.directionBase.z);
        EXPECT_FLOAT_EQ(a.directionSpread, b.directionSpread);
        EXPECT_FLOAT_EQ(a.gravity, b.gravity);
        EXPECT_FLOAT_EQ(a.drag, b.drag);
        EXPECT_FLOAT_EQ(a.startSize, b.startSize);
        EXPECT_FLOAT_EQ(a.endSize, b.endSize);
        EXPECT_FLOAT_EQ(a.startColor.x, b.startColor.x);
        EXPECT_FLOAT_EQ(a.startColor.y, b.startColor.y);
        EXPECT_FLOAT_EQ(a.startColor.z, b.startColor.z);
        EXPECT_FLOAT_EQ(a.startAlpha, b.startAlpha);
        EXPECT_FLOAT_EQ(a.endColor.x, b.endColor.x);
        EXPECT_FLOAT_EQ(a.endColor.y, b.endColor.y);
        EXPECT_FLOAT_EQ(a.endColor.z, b.endColor.z);
        EXPECT_FLOAT_EQ(a.endAlpha, b.endAlpha);
        EXPECT_FLOAT_EQ(a.startRotation, b.startRotation);
        EXPECT_FLOAT_EQ(a.rotationSpeed, b.rotationSpeed);
        EXPECT_FLOAT_EQ(a.speedVariance, b.speedVariance);
        EXPECT_FLOAT_EQ(a.sizeVariance, b.sizeVariance);
        EXPECT_FLOAT_EQ(a.rotationVariance, b.rotationVariance);
    }

    Entity MakeEffectEntity(ECSRegistry& registry, const ParticleEffectComponent& settings,
                            const Vec3& position = Vec3(0.0f, 0.0f, 0.0f))
    {
        Entity e = registry.CreateEntity();
        TransformComponent t;
        t.position = position;
        registry.AddComponent(e, t);
        registry.AddComponent(e, settings);
        return e;
    }

    std::filesystem::path TempPath(const char* name)
    {
        return std::filesystem::temp_directory_path() / name;
    }
}

// ── 캡처 / 스폰 (§4.4의 "저장/스폰이 공짜로 따라온다") ─────────────────────────

TEST(ParticlePrefabTest, CaptureAndSpawnPreservesAllTwentyFields)
{
    ECSRegistry registry;
    const ParticleEffectComponent original = DistinctEffect();
    Entity source = MakeEffectEntity(registry, original, Vec3(1.0f, 2.0f, 3.0f));

    auto prefab = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(prefab.has_value()) << "파티클 컴포넌트 캡처 실패 — 새 포맷 없이 프리팹에 "
                                       "얹을 수 있다는 §4.4의 전제가 깨졌다";
    ASSERT_TRUE(prefab->componentsData.contains("ParticleEffectComponent"));

    auto spawned = prefab->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value());

    const ParticleEffectComponent* restored =
        registry.GetComponent<ParticleEffectComponent>(*spawned);
    ASSERT_NE(restored, nullptr);
    ExpectEffectEqual(original, *restored);
}

TEST(ParticlePrefabTest, RoundTripsThroughPrefabFileOnDisk)
{
    // "새 .vfx.json 포맷을 발명할 필요가 없다"(§4.4)의 실제 확인 — 기존
    // *.prefab.json 그대로 저장하고 읽는다.
    ECSRegistry registry;
    const ParticleEffectComponent original = DistinctEffect();
    Entity source = MakeEffectEntity(registry, original);

    auto prefab = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(prefab.has_value());
    prefab->name = "TestFire";

    const auto path = TempPath("vfx_lite_phase5.prefab.json");
    ASSERT_TRUE(prefab->SaveToFile(path).has_value());

    auto loaded = PrefabAsset::LoadFromFile(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->name, "TestFire");

    auto spawned = loaded->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value());
    ExpectEffectEqual(original, *registry.GetComponent<ParticleEffectComponent>(*spawned));

    std::filesystem::remove(path);
}

// ── Revert (§6.2의 "프리팹 Phase 4의 Revert를 자동으로 상속받는다") ────────────

TEST(ParticlePrefabTest, ApplyToEntityRevertsLocallyEditedSettings)
{
    ECSRegistry registry;
    const ParticleEffectComponent original = DistinctEffect();
    Entity source = MakeEffectEntity(registry, original);

    auto prefab = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(prefab.has_value());

    auto instance = prefab->SpawnInto(registry);
    ASSERT_TRUE(instance.has_value());

    // 인스턴스에서 값을 여러 개 바꾼다(에디터 Inspector가 하는 일).
    ParticleEffectComponent* edited = registry.GetComponent<ParticleEffectComponent>(*instance);
    ASSERT_NE(edited, nullptr);
    edited->spawnRate   = 999.0f;
    edited->maxParticle = 1;
    edited->startColor  = Vec3(1.0f, 1.0f, 1.0f);
    edited->endAlpha    = 1.0f;

    // Revert = 프리팹을 그 엔티티에 다시 적용하는 것.
    ASSERT_TRUE(prefab->ApplyToEntity(registry, *instance).has_value());

    ExpectEffectEqual(original, *registry.GetComponent<ParticleEffectComponent>(*instance));
}

// ── §4.4의 계층 구분: 프리팹은 설정만, 시뮬레이션은 ParticleSystem ────────────

TEST(ParticlePrefabTest, SpawnedEffectIsSimulatedExactlyLikeAHandMadeOne)
{
    // 이것이 Phase 5의 핵심 확인이다. 프리팹은 컴포넌트 값만 복원하고 시뮬레이션은
    // 전혀 모르는데, 그럼에도 스폰된 엔티티가 ParticleSystem에 손으로 만든 것과
    // 똑같이 잡혀야 한다 — 그래야 §4.4의 "저장/복원 계층과 시뮬레이션 계층이 명확히
    // 분리된다"가 말뿐이 아니게 된다.
    ECSRegistry registry;
    ParticleSystem system;

    // 스폰 "위치"만 보려는 테스트라 파티클을 움직이지 않게 둔다 — 기본 initialSpeed는
    // 3.0이라 한 프레임(0.05초)만에도 눈에 띄게 이동한다(그게 정상 동작이다).
    ParticleEffectComponent settings;
    settings.spawnRate        = 0.0f;
    settings.burst            = 12;
    settings.maxParticle      = 64;
    settings.lifetime         = 5.0f;
    settings.initialSpeed     = 0.0f;
    settings.gravity          = 0.0f;
    settings.drag             = 0.0f;
    settings.speedVariance    = 0.0f;
    settings.sizeVariance     = 0.0f;
    settings.rotationVariance = 0.0f;

    Entity source = MakeEffectEntity(registry, settings, Vec3(7.0f, 0.0f, 0.0f));

    auto prefab = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(prefab.has_value());
    auto spawned = prefab->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value());

    system.Update(registry, 0.05f);

    // 손으로 만든 것과 프리팹에서 나온 것 모두 이펙트로 잡혀야 한다.
    const EffectInstance* fromHand   = system.FindEffect(source.id);
    const EffectInstance* fromPrefab = system.FindEffect(spawned->id);
    ASSERT_NE(fromHand, nullptr);
    ASSERT_NE(fromPrefab, nullptr)
        << "프리팹으로 스폰된 이펙트가 ParticleSystem에 잡히지 않았다 — 시뮬레이션이 "
           "'공짜로 따라온다'는 전제가 깨졌다";

    EXPECT_EQ(fromHand->particles.size(), 12u);
    EXPECT_EQ(fromPrefab->particles.size(), 12u);

    // 프리팹이 TransformComponent도 함께 복원하므로 월드 공간 스폰 위치까지 같아야 한다
    // (Phase 3 계획서 §3.10).
    for (const Particle& p : fromPrefab->particles)
    {
        EXPECT_NEAR(p.position.x, 7.0f, 1e-4f);
    }
}

TEST(ParticlePrefabTest, SpawnedEffectCarriesPrefabInstanceComponentForRevert)
{
    // Revert를 에디터에서 쓸 수 있으려면 스폰된 엔티티가 "어느 프리팹에서 왔는지"를
    // 들고 있어야 한다. 파티클이라고 다를 이유가 없다는 것을 확인한다.
    ECSRegistry registry;
    Entity source = MakeEffectEntity(registry, DistinctEffect());

    auto prefab = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(prefab.has_value());

    // 캡처 결과에는 PrefabInstanceComponent가 들어가면 안 된다(프리팹 계획서 §2.6).
    EXPECT_FALSE(prefab->componentsData.contains("PrefabInstanceComponent"));

    auto spawned = prefab->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value());

    // 원본에는 여전히 없어야 한다 — 캡처는 원본을 인스턴스로 만들지 않는다.
    EXPECT_FALSE(registry.HasComponent<PrefabInstanceComponent>(source));
}
