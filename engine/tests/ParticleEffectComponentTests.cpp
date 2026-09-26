// VFX Lite Phase 1 — ParticleEffectComponent 리플렉션/직렬화 테스트.
// docs/VFX_LITE_IMPLEMENTATION_PLAN.md §3 "Phase 1"의 검증 항목.
//
// 이 Phase는 렌더링·시뮬레이션과 무관하므로 엔진 실행 없이 전부 검증된다 —
// 계획서가 Phase 1/2를 렌더링보다 앞에 둔 이유가 이것이다(Phase 3에서 화면이
// 이상하면 원인이 렌더링 쪽임을 이미 알고 시작할 수 있다).

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"

using namespace Engine;

namespace
{
    const ComponentInfo* ParticleInfo()
    {
        return ComponentRegistry::GetComponentInfo("ParticleEffectComponent");
    }

    // 기본값과 전부 다른 값으로 채운다 — 라운드트립 테스트가 "기본 생성자가
    // 우연히 같은 값을 넣어줘서 통과"하는 것을 막기 위해서다.
    ParticleEffectComponent MakeDistinctSettings()
    {
        ParticleEffectComponent p;
        p.spawnRate        = 123.5f;
        p.maxParticle      = 777;
        p.lifetime         = 2.25f;
        p.burst            = 42;
        p.initialSpeed     = 9.5f;
        p.directionBase    = Vec3(0.25f, -0.5f, 0.75f);
        p.directionSpread  = 137.0f;
        p.gravity          = -9.81f;
        p.drag             = 0.35f;
        p.startSize        = 1.5f;
        p.endSize          = 0.125f;
        p.startColor       = Vec3(0.1f, 0.2f, 0.3f);
        p.startAlpha       = 0.9f;
        p.endColor         = Vec3(0.4f, 0.5f, 0.6f);
        p.endAlpha         = 0.05f;
        p.startRotation    = 45.0f;
        p.rotationSpeed    = -180.0f;
        p.speedVariance    = 0.33f;
        p.sizeVariance     = 0.44f;
        p.rotationVariance = 0.55f;
        return p;
    }

    void ExpectSettingsEqual(const ParticleEffectComponent& a, const ParticleEffectComponent& b)
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
}

// ── 등록 및 필드 스키마 ────────────────────────────────────────────────────────

TEST(ParticleEffectComponentTest, IsRegisteredInComponentRegistry)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr)
        << "ParticleEffectComponent가 등록되지 않았다 — Reflection.cpp의 "
           "InitializeReflection()이 RegisterParticleComponentsReflection()을 부르는지 확인할 것";
    EXPECT_EQ(info->name, "ParticleEffectComponent");
    EXPECT_NE(info->serialize, nullptr);
    EXPECT_NE(info->deserialize, nullptr);
    EXPECT_NE(info->patchField, nullptr);
}

TEST(ParticleEffectComponentTest, HasTwentyFieldsForTheEighteenScopedParameters)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    // 18개 파라미터 != 20개 필드. Direction("기준 방향 + 퍼짐 각도")과
    // Alpha Fade("수명에 따른 투명도 변화")가 각각 값 하나를 필드 두 개로 표현한다.
    // 이 숫자가 바뀐다면 범위 정의서 §2의 18개 원칙이 조용히 깨진 것이므로
    // 문서(구현 계획서 §2.4의 대조표)를 먼저 고쳐야 한다.
    EXPECT_EQ(info->fields.size(), 20u)
        << "필드 수가 바뀌었다 — docs/VFX_LITE_IMPLEMENTATION_PLAN.md §2.4의 "
           "파라미터/필드 대조표를 함께 갱신했는지 확인할 것";
}

TEST(ParticleEffectComponentTest, NoFieldUsesEnumType)
{
    // 회귀 가드. GE_BEGIN_COMPONENT가 만드는 serialize/deserialize switch에는
    // FieldType::Enum case가 없다(docs/VFX_LITE_PLAN.md §7.3) — 즉 enum 필드를
    // 등록하면 예외도 컴파일 에러도 없이 값이 그냥 저장되지 않는다. 나중에 누가
    // 무심코 enum 필드를 추가하면 여기서 걸린다.
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    for (const auto& f : info->fields)
    {
        EXPECT_NE(f.type, FieldType::Enum)
            << "필드 '" << f.name << "'이 Enum이다 — 매크로 직렬화 경로가 Enum을 "
               "처리하지 않아 조용히 누락된다. Reflection.h의 switch에 case를 "
               "추가하거나 필드를 Int로 표현할 것";
    }
}

// ── 직렬화 라운드트립 ──────────────────────────────────────────────────────────

TEST(ParticleEffectComponentTest, RoundTripsAllTwentyFieldsThroughJson)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity source = registry.CreateEntity();
    const ParticleEffectComponent original = MakeDistinctSettings();
    registry.AddComponent(source, original);

    nlohmann::json out;
    info->serialize(registry, source, out);
    ASSERT_TRUE(out.contains("ParticleEffectComponent"));

    // 다른 엔티티로 복원한다 — 같은 엔티티에 덮어쓰면 "역직렬화가 아무것도 안 해도"
    // 값이 남아 있어서 통과해버린다.
    Entity target = registry.CreateEntity();
    info->deserialize(registry, target, out["ParticleEffectComponent"]);

    ASSERT_TRUE(registry.HasComponent<ParticleEffectComponent>(target));
    const ParticleEffectComponent* restored = registry.GetComponent<ParticleEffectComponent>(target);
    ASSERT_NE(restored, nullptr);

    ExpectSettingsEqual(original, *restored);
}

TEST(ParticleEffectComponentTest, Vec3FieldsSerializeAsThreeElementArrays)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, MakeDistinctSettings());

    nlohmann::json out;
    info->serialize(registry, e, out);
    const auto& c = out["ParticleEffectComponent"];

    for (const char* name : {"directionBase", "startColor", "endColor"})
    {
        ASSERT_TRUE(c.contains(name)) << name;
        EXPECT_TRUE(c[name].is_array()) << name;
        EXPECT_EQ(c[name].size(), 3u) << name;
    }

    // 색상 알파는 Vec3에 들어가지 않고 별도 Float 필드로 나간다
    // (FieldType에 Vec4/Color가 없기 때문 — docs/VFX_LITE_PLAN.md §7.3).
    EXPECT_TRUE(c["startAlpha"].is_number());
    EXPECT_TRUE(c["endAlpha"].is_number());
    EXPECT_FLOAT_EQ(c["startColor"][0].get<float>(), 0.1f);
    EXPECT_FLOAT_EQ(c["startAlpha"].get<float>(), 0.9f);
}

TEST(ParticleEffectComponentTest, DeserializeCreatesComponentWhenMissing)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    ASSERT_FALSE(registry.HasComponent<ParticleEffectComponent>(e));

    nlohmann::json data;
    data["spawnRate"] = 55.0f;

    info->deserialize(registry, e, data);

    ASSERT_TRUE(registry.HasComponent<ParticleEffectComponent>(e));
    const ParticleEffectComponent* comp = registry.GetComponent<ParticleEffectComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FLOAT_EQ(comp->spawnRate, 55.0f);
    // JSON에 없던 필드는 기본 생성자 값을 유지해야 한다(부분 데이터 허용).
    EXPECT_EQ(comp->maxParticle, ParticleEffectComponent().maxParticle);
}

// ── patchField (에디터 인스펙터가 필드 하나만 고칠 때 쓰는 경로) ───────────────

TEST(ParticleEffectComponentTest, PatchFieldUpdatesSingleFloatAndVec3)
{
    const ComponentInfo* info = ParticleInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, ParticleEffectComponent{});

    info->patchField(registry, e, StringHash("lifetime"), nlohmann::json(3.5f));
    info->patchField(registry, e, StringHash("startColor"), nlohmann::json{0.7f, 0.8f, 0.9f});

    const ParticleEffectComponent* comp = registry.GetComponent<ParticleEffectComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FLOAT_EQ(comp->lifetime, 3.5f);
    EXPECT_FLOAT_EQ(comp->startColor.x, 0.7f);
    EXPECT_FLOAT_EQ(comp->startColor.y, 0.8f);
    EXPECT_FLOAT_EQ(comp->startColor.z, 0.9f);

    // 건드리지 않은 필드는 그대로여야 한다.
    EXPECT_FLOAT_EQ(comp->spawnRate, ParticleEffectComponent().spawnRate);
}

// ── 회귀 게이트: 기존 컴포넌트 직렬화에 영향이 없어야 한다 ─────────────────────

TEST(ParticleEffectComponentTest, AbsentComponentDoesNotAppearInSerializedOutput)
{
    // 계획서 §3 Phase 1의 회귀 게이트. 신규 컴포넌트 타입을 등록한 것 때문에
    // 그 컴포넌트를 갖지 않은 엔티티의 직렬화 결과가 달라지면 안 된다
    // (SerializeRegistry는 등록된 모든 타입의 serialize를 부르므로, 각 serialize가
    // 컴포넌트 부재 시 조용히 return하는지가 실제로 확인할 지점이다).
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);
    registry.AddComponent(e, t);

    nlohmann::json snapshot = SerializeRegistry(registry);

    ASSERT_TRUE(snapshot.contains("entities"));
    ASSERT_EQ(snapshot["entities"].size(), 1u);
    const auto& comps = snapshot["entities"][0]["components"];

    EXPECT_TRUE(comps.contains("TransformComponent"));
    EXPECT_FALSE(comps.contains("ParticleEffectComponent"))
        << "파티클 컴포넌트가 없는 엔티티인데 직렬화 결과에 나타났다";
}
