// TransformComponent::rotation(Quaternion) <-> 직렬화/편집 표현(오일러 각, 도) 회귀 테스트.
// 배경: rotation이 리플렉션에 Vec3로 등록돼 있어서 직렬화 시 w가 빠지고 역직렬화 시 x,y,z만 덮어써서,
// PIE 스냅샷 복원이나 프리팹 인스턴스화 뒤 회전이 조용히 깨졌다(Reflection.h FieldType::EulerRotation 주석).

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../ecs/RotationConversion.h"
#include <cmath>

using namespace Engine;

namespace
{
    // q와 -q는 같은 회전이다.
    bool SameRotation(const Quaternion& a, const Quaternion& b, float eps = 1e-4f)
    {
        float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        return std::fabs(std::fabs(dot) - 1.0f) < eps;
    }

    const ComponentInfo* TransformInfo()
    {
        return ComponentRegistry::GetComponentInfo("TransformComponent");
    }
}

TEST(TransformRotationTest, EulerQuaternionRoundTrip)
{
    for (Vec3 e : { Vec3(0, 90, 0), Vec3(30, -45, 10), Vec3(0, 0, 0), Vec3(-20, 60, 170) })
    {
        Quaternion q = QuaternionFromEulerDegrees(e);
        Quaternion q2 = QuaternionFromEulerDegrees(EulerDegreesFromQuaternion(q));
        EXPECT_TRUE(SameRotation(q, q2)) << "euler (" << e.x << "," << e.y << "," << e.z << ")";
    }
}

TEST(TransformRotationTest, SetTransformRotation_InterpretsDegrees)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});
    registry.SetTransformRotation(e, 0.0f, 90.0f, 0.0f);

    const Quaternion& q = registry.GetComponent<TransformComponent>(e)->rotation;
    const float s = std::sqrt(0.5f);
    EXPECT_TRUE(SameRotation(q, Quaternion(0.0f, s, 0.0f, s)))
        << "got (" << q.x << "," << q.y << "," << q.z << "," << q.w << ")";
}

TEST(TransformRotationTest, RotationField_IsRegisteredAsEulerRotation)
{
    const ComponentInfo* info = TransformInfo();
    ASSERT_NE(info, nullptr);
    auto it = std::find_if(info->fields.begin(), info->fields.end(), [](const FieldInfo& f) { return f.name == "rotation"; });
    ASSERT_NE(it, info->fields.end());
    EXPECT_EQ(it->type, FieldType::EulerRotation);
}

TEST(TransformRotationTest, SerializeDeserialize_PreservesNonTrivialRotation)
{
    const ComponentInfo* info = TransformInfo();
    ASSERT_NE(info, nullptr);

    ECSRegistry source;
    Entity e = source.CreateEntity();
    TransformComponent t;
    t.rotation = QuaternionFromEulerDegrees(Vec3(30.0f, -45.0f, 10.0f));
    source.AddComponent(e, t);

    nlohmann::json out;
    info->serialize(source, e, out);
    ASSERT_TRUE(out.contains("TransformComponent"));
    ASSERT_EQ(out["TransformComponent"]["rotation"].size(), 3u);   // 파일 형식: 오일러 3개 값

    ECSRegistry target;
    Entity e2 = target.CreateEntity();
    info->deserialize(target, e2, out["TransformComponent"]);

    const Quaternion& restored = target.GetComponent<TransformComponent>(e2)->rotation;
    EXPECT_TRUE(SameRotation(t.rotation, restored))
        << "restored (" << restored.x << "," << restored.y << "," << restored.z << "," << restored.w << ")";
}

TEST(TransformRotationTest, LegacyPrefabZeroRotation_IsIdentity)
{
    // assets/prefabs/*.prefab.json은 "rotation": [0.0, 0.0, 0.0]로 저장돼 있다.
    const ComponentInfo* info = TransformInfo();
    ASSERT_NE(info, nullptr);
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    info->deserialize(registry, e, nlohmann::json::parse(R"({"rotation": [0.0, 0.0, 0.0]})"));
    EXPECT_TRUE(SameRotation(registry.GetComponent<TransformComponent>(e)->rotation, Quaternion()));
}

// ── PIE 스냅샷(SerializeRegistry/DeserializeRegistry)의 엔티티 이름 보존 ─────────
// 에디터에서 Play -> Stop 후 모든 이름이 "Entity_<새 id>"로 바뀌던 버그의 회귀 테스트.
TEST(PieSnapshotTest, EntityNames_SurviveSnapshotRestore)
{
    ECSRegistry registry;
    Entity named = registry.CreateEntity();
    registry.SetEntityName(named, "Main Camera");
    registry.AddComponent(named, TransformComponent{});
    Entity unnamed = registry.CreateEntity();
    registry.AddComponent(unnamed, TransformComponent{});
    const uint64_t namedUuid = registry.GetUUID(named).GetValue();
    const uint64_t unnamedUuid = registry.GetUUID(unnamed).GetValue();

    nlohmann::json snapshot = SerializeRegistry(registry);
    registry.Clear();
    DeserializeRegistry(registry, snapshot);

    Entity restoredNamed = registry.GetEntityByUUID(UUID(namedUuid));
    Entity restoredUnnamed = registry.GetEntityByUUID(UUID(unnamedUuid));
    ASSERT_TRUE(restoredNamed.IsValid());
    ASSERT_TRUE(restoredUnnamed.IsValid());
    EXPECT_EQ(registry.GetEntityName(restoredNamed), "Main Camera");
    // 이름이 없던 엔티티는 복원 후에도 명시적 이름이 없어야 한다(옛 id가 박힌 가짜 이름 금지)
    EXPECT_FALSE(registry.HasEntityName(restoredUnnamed));
}
