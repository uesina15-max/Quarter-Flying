// 프리팹 Phase 5: 계층 컴포넌트(ecs/Hierarchy.h)와 다중 엔티티 프리팹(prefab v2), 그리고 그 둘을
// 쓰는 편집 명령(삭제/인스턴스화/되돌리기/부모 변경)의 Undo/Redo.

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Hierarchy.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/DestroyEntityCommand.h"
#include "../editor/commands/InstantiatePrefabCommand.h"
#include "../editor/commands/RevertPrefabInstanceCommand.h"
#include "../editor/commands/SetParentCommand.h"
#include "../prefab/PrefabAsset.h"
#include "../prefab/PrefabInstanceComponent.h"
#include "../core/CommandManager.h"
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace
{
    Entity MakeNode(ECSRegistry& registry, const std::string& name, Vec3 position,
                    Quaternion rotation = Quaternion(0, 0, 0, 1))
    {
        Entity e = registry.CreateEntity();
        registry.SetEntityName(e, name);
        TransformComponent t;
        t.position = position;
        t.rotation = rotation;
        registry.AddComponent(e, t);
        return e;
    }

    Quaternion YawDegrees(float degrees)
    {
        glm::quat q = glm::angleAxis(glm::radians(degrees), glm::vec3(0, 1, 0));
        return Quaternion(q.x, q.y, q.z, q.w);
    }

    glm::vec3 WorldPosition(ECSRegistry& registry, Entity e)
    {
        return glm::vec3(ComputeWorldMatrix(registry, e)[3]);
    }

    void ExpectNear(const glm::vec3& a, const glm::vec3& b)
    {
        EXPECT_NEAR(a.x, b.x, 1e-4f);
        EXPECT_NEAR(a.y, b.y, 1e-4f);
        EXPECT_NEAR(a.z, b.z, 1e-4f);
    }

    Entity FindByName(ECSRegistry& registry, const std::string& name)
    {
        for (Entity e : registry.GetAllEntities())
        {
            if (registry.HasEntityName(e) && registry.GetEntityName(e) == name)
            {
                return e;
            }
        }
        return Entity();
    }

    // Lamp(루트) -> Pole -> Head, Lamp -> Base  (Pole과 Base는 형제)
    struct LampTree
    {
        Entity lamp, pole, head, base;
    };

    LampTree MakeLamp(ECSRegistry& registry)
    {
        LampTree t;
        t.lamp = MakeNode(registry, "Lamp", Vec3(10, 0, 0), YawDegrees(90));
        t.pole = MakeNode(registry, "Pole", Vec3(0, 1, 0));
        t.head = MakeNode(registry, "Head", Vec3(1, 0, 0));
        t.base = MakeNode(registry, "Base", Vec3(0, -0.5f, 0));
        EXPECT_TRUE(SetParent(registry, t.pole, t.lamp).has_value());
        EXPECT_TRUE(SetParent(registry, t.head, t.pole).has_value());
        EXPECT_TRUE(SetParent(registry, t.base, t.lamp).has_value());
        return t;
    }

    std::filesystem::path TempPath(const char* name)
    {
        return std::filesystem::temp_directory_path() / name;
    }
}

// ── 계층 API ───────────────────────────────────────────────────────────────

TEST(HierarchyTest, SetParent_LinksAndUnlinks)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    EXPECT_EQ(GetParent(registry, t.head), t.pole);
    EXPECT_EQ(GetParent(registry, t.lamp), Entity());
    auto children = GetChildren(registry, t.lamp);
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], t.pole);   // id 오름차순
    EXPECT_EQ(children[1], t.base);

    auto descendants = GetDescendants(registry, t.lamp);
    ASSERT_EQ(descendants.size(), 3u);
    EXPECT_EQ(descendants[0], t.pole);  // 전위 순회: 부모가 자식보다 먼저
    EXPECT_EQ(descendants[1], t.head);
    EXPECT_EQ(descendants[2], t.base);

    ASSERT_TRUE(SetParent(registry, t.head, Entity()).has_value());
    EXPECT_EQ(GetParent(registry, t.head), Entity());
    EXPECT_FALSE(registry.HasComponent<HierarchyComponent>(t.head));
}

TEST(HierarchyTest, SetParent_RejectsSelfAndCycles)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    EXPECT_FALSE(SetParent(registry, t.lamp, t.lamp).has_value());
    EXPECT_FALSE(SetParent(registry, t.lamp, t.head).has_value());   // Head는 Lamp의 자손
    EXPECT_EQ(GetParent(registry, t.lamp), Entity());                  // 거절 후 상태 불변
    EXPECT_TRUE(SetParent(registry, t.base, t.head).has_value());     // 형제 아래로는 가능
}

TEST(HierarchyTest, WorldMatrix_ComposesParentChain)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    // Lamp: (10,0,0), Y축 90도 -> 로컬 +X가 월드 -Z가 된다.
    ExpectNear(WorldPosition(registry, t.pole), glm::vec3(10, 1, 0));
    ExpectNear(WorldPosition(registry, t.head), glm::vec3(10, 1, -1));

    const TransformComponent world = ComputeWorldTransform(registry, t.head);
    EXPECT_NEAR(world.position.x, 10.0f, 1e-4f);
    EXPECT_NEAR(world.position.z, -1.0f, 1e-4f);
    // 회전도 누적된다: Head의 전방(0,0,-1)이 Lamp의 90도만큼 돈다 -> (-1,0,0)
    glm::quat q(world.rotation.w, world.rotation.x, world.rotation.y, world.rotation.z);
    ExpectNear(q * glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0));

    // 부모를 움직이면 자식이 따라간다
    registry.GetComponent<TransformComponent>(t.lamp)->position = Vec3(0, 0, 0);
    ExpectNear(WorldPosition(registry, t.head), glm::vec3(0, 1, -1));
}

TEST(HierarchyTest, SetParentKeepWorld_ObjectStaysWhereItWas)
{
    // 에디터 드래그 경로. 로컬 유지(false)였을 때 실제 에디터에서 드래그한 큐브가 새 부모(45도 회전,
    // 0.8배)의 좌표계로 다시 해석돼 화면 밖으로 사라졌다.
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);
    Entity cube = MakeNode(registry, "Cube", Vec3(0, 1.5f, 5), YawDegrees(30));
    registry.GetComponent<TransformComponent>(t.pole)->scale = Vec3(2, 2, 2);
    const glm::mat4 before = ComputeWorldMatrix(registry, cube);

    ASSERT_TRUE(SetParent(registry, cube, t.pole, /*keepWorldTransform=*/true).has_value());
    EXPECT_EQ(GetParent(registry, cube), t.pole);
    const glm::mat4 after = ComputeWorldMatrix(registry, cube);
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            EXPECT_NEAR(after[c][r], before[c][r], 1e-4f) << "col " << c << " row " << r;
        }
    }

    ASSERT_TRUE(SetParent(registry, cube, Entity(), /*keepWorldTransform=*/true).has_value());
    ExpectNear(WorldPosition(registry, cube), glm::vec3(0, 1.5f, 5));
}

TEST(HierarchyTest, CycleWrittenDirectly_DoesNotHang)
{
    // Inspector/SetComponentJson은 SetParent의 순환 검사를 거치지 않는다. 그래도 멈추지 않아야 한다.
    ECSRegistry registry;
    Entity a = MakeNode(registry, "A", Vec3(1, 0, 0));
    Entity b = MakeNode(registry, "B", Vec3(0, 1, 0));
    ASSERT_TRUE(SetParent(registry, b, a).has_value());
    HierarchyComponent h;
    h.parent = b;
    registry.AddComponent(a, h);   // a -> b -> a

    (void)ComputeWorldMatrix(registry, a);
    (void)ComputeWorldTransform(registry, b);
    EXPECT_LE(GetDescendants(registry, a).size(), 2u);
    EXPECT_TRUE(IsSelfOrAncestor(registry, a, b));
}

TEST(HierarchyTest, DestroyedParent_ChildBecomesRoot)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);
    registry.DestroyEntity(t.pole);
    EXPECT_EQ(GetParent(registry, t.head), Entity());
    ExpectNear(WorldPosition(registry, t.head), glm::vec3(1, 0, 0));
}

TEST(HierarchyTest, PieSnapshot_RestoresParentLinks)
{
    // PIE Play->Stop는 SerializeRegistry/DeserializeRegistry로 복원한다. 복원 후 런타임 id가 바뀌어도
    // parent(EntityRef, UUID로 저장)가 올바른 엔티티를 가리켜야 한다.
    ECSRegistry registry;
    MakeLamp(registry);
    const nlohmann::json snapshot = SerializeRegistry(registry);

    ECSRegistry restored;
    DeserializeRegistry(restored, snapshot);
    Entity head = FindByName(restored, "Head");
    Entity pole = FindByName(restored, "Pole");
    ASSERT_TRUE(head.IsValid());
    EXPECT_EQ(GetParent(restored, head), pole);
    ExpectNear(WorldPosition(restored, head), glm::vec3(10, 1, -1));
}

// ── 다중 엔티티 프리팹 ───────────────────────────────────────────────────────

TEST(HierarchyPrefabTest, Capture_RecordsChildrenAsParentIndices)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    auto asset = PrefabAsset::CaptureFromEntity(registry, t.lamp);
    ASSERT_TRUE(asset.has_value()) << asset.error().message;
    EXPECT_EQ(asset->version, 2u);
    ASSERT_EQ(asset->children.size(), 3u);
    EXPECT_EQ(asset->children[0].name, "Pole");
    EXPECT_EQ(asset->children[0].parentIndex, 0);
    EXPECT_EQ(asset->children[1].name, "Head");
    EXPECT_EQ(asset->children[1].parentIndex, 1);   // Pole
    EXPECT_EQ(asset->children[2].parentIndex, 0);
    for (const auto& child : asset->children)
    {
        EXPECT_FALSE(child.componentsData.contains(kHierarchyComponentName));
    }
}

TEST(HierarchyPrefabTest, SaveLoadSpawn_RebuildsTreeAndWorldPositions)
{
    ECSRegistry source;
    LampTree t = MakeLamp(source);
    auto captured = PrefabAsset::CaptureFromEntity(source, t.lamp);
    ASSERT_TRUE(captured.has_value());
    const auto path = TempPath("qf_lamp_v2.prefab.json");
    ASSERT_TRUE(captured->SaveToFile(path).has_value());

    auto loaded = PrefabAsset::LoadFromFile(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ASSERT_EQ(loaded->children.size(), 3u);

    ECSRegistry scene;
    auto spawned = loaded->SpawnHierarchyInto(scene);
    ASSERT_TRUE(spawned.has_value()) << spawned.error().message;
    ASSERT_EQ(spawned->size(), 4u);
    Entity lamp = (*spawned)[0], pole = (*spawned)[1], head = (*spawned)[2];
    EXPECT_EQ(scene.GetEntityName(lamp), "Lamp");
    EXPECT_EQ(scene.GetEntityName(head), "Head");
    EXPECT_EQ(GetParent(scene, head), pole);
    EXPECT_EQ(GetParent(scene, pole), lamp);
    ExpectNear(WorldPosition(scene, head), glm::vec3(10, 1, -1));
    std::filesystem::remove(path);
}

TEST(HierarchyPrefabTest, SingleEntityPrefab_StillWritesV1)
{
    ECSRegistry registry;
    Entity e = MakeNode(registry, "Solo", Vec3(1, 2, 3));
    auto asset = PrefabAsset::CaptureFromEntity(registry, e);
    ASSERT_TRUE(asset.has_value());
    const nlohmann::json j = asset->ToJson();
    EXPECT_EQ(j["version"], 1);
    EXPECT_TRUE(j.contains("components"));
    EXPECT_FALSE(j.contains("entities"));
}

TEST(HierarchyPrefabTest, FromJson_RejectsBadParentIndices)
{
    auto parse = [](const char* text) { return PrefabAsset::FromJson(nlohmann::json::parse(text)); };
    // 부모가 자기보다 뒤에 있음(순환을 만들 수 있는 형태)
    EXPECT_FALSE(parse(R"({"version":2,"entities":[{"components":{}},{"parent":2,"components":{}},{"parent":1,"components":{}}]})").has_value());
    // 자기 자신
    EXPECT_FALSE(parse(R"({"version":2,"entities":[{"components":{}},{"parent":1,"components":{}}]})").has_value());
    // 루트에 parent
    EXPECT_FALSE(parse(R"({"version":2,"entities":[{"parent":0,"components":{}}]})").has_value());
    // 자식에 parent 없음
    EXPECT_FALSE(parse(R"({"version":2,"entities":[{"components":{}},{"components":{}}]})").has_value());
    EXPECT_TRUE(parse(R"({"version":2,"entities":[{"components":{}},{"parent":0,"components":{}}]})").has_value());
}

TEST(HierarchyPrefabTest, Capture_StillRejectsOtherEntityRefs)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);
    CameraFollowComponent follow;
    follow.target = t.lamp;
    registry.AddComponent(t.head, follow);
    EXPECT_FALSE(PrefabAsset::CaptureFromEntity(registry, t.lamp).has_value());
}

TEST(HierarchyPrefabTest, ApplyToEntity_KeepsPlacementUnderParent)
{
    // 인스턴스를 다른 엔티티 아래에 둔 뒤 Revert해도 부모에서 떨어지면 안 된다(IsRemovalExempt).
    ECSRegistry registry;
    Entity holder = MakeNode(registry, "Holder", Vec3(0, 0, 0));
    Entity instance = MakeNode(registry, "Instance", Vec3(0, 0, 0));
    ASSERT_TRUE(SetParent(registry, instance, holder).has_value());

    PrefabAsset asset;
    asset.componentsData = { { "TransformComponent", { { "position", { 5, 0, 0 } } } } };
    ASSERT_TRUE(asset.ApplyToEntity(registry, instance).has_value());
    EXPECT_EQ(GetParent(registry, instance), holder);
}

// ── 명령 (Undo/Redo) ─────────────────────────────────────────────────────────

class HierarchyCommandTest : public ::testing::Test
{
protected:
    void SetUp() override { CommandManager::GetInstance().Clear(); }
    void TearDown() override { CommandManager::GetInstance().Clear(); }
    CommandManager& cm() { return CommandManager::GetInstance(); }
    ECSRegistry registry;
};

TEST_F(HierarchyCommandTest, Destroy_RemovesSubtree_UndoRestoresLinks)
{
    LampTree t = MakeLamp(registry);
    const UUID headUuid = registry.GetUUID(t.head);

    ASSERT_TRUE(cm().Execute(std::make_unique<DestroyEntityCommand>(&registry, t.pole)).has_value());
    EXPECT_FALSE(registry.IsValid(t.pole));
    EXPECT_FALSE(registry.IsValid(t.head));   // 자손도 함께
    EXPECT_TRUE(registry.IsValid(t.base));    // 형제는 그대로

    ASSERT_TRUE(cm().Undo().has_value());
    Entity head = registry.GetEntityByUUID(headUuid);
    ASSERT_TRUE(registry.IsValid(head));
    Entity pole = GetParent(registry, head);
    EXPECT_EQ(registry.GetEntityName(pole), "Pole");
    EXPECT_EQ(GetParent(registry, pole), t.lamp);
    ExpectNear(WorldPosition(registry, head), glm::vec3(10, 1, -1));

    ASSERT_TRUE(cm().Redo().has_value());
    EXPECT_FALSE(registry.IsValid(registry.GetEntityByUUID(headUuid)));
}

TEST_F(HierarchyCommandTest, SetParent_UndoRedo_AndCycleIsNotRecorded)
{
    LampTree t = MakeLamp(registry);
    ASSERT_TRUE(cm().Execute(std::make_unique<SetParentCommand>(&registry, t.base, t.head)).has_value());
    EXPECT_EQ(GetParent(registry, t.base), t.head);
    ASSERT_TRUE(cm().Undo().has_value());
    EXPECT_EQ(GetParent(registry, t.base), t.lamp);
    ASSERT_TRUE(cm().Redo().has_value());
    EXPECT_EQ(GetParent(registry, t.base), t.head);

    ASSERT_TRUE(cm().Undo().has_value());
    EXPECT_FALSE(cm().Execute(std::make_unique<SetParentCommand>(&registry, t.lamp, t.head)).has_value());
    EXPECT_EQ(GetParent(registry, t.lamp), Entity());

    // 루트로 만들기: 월드 위치 유지, Undo는 원래 로컬 값을 정확히 되돌린다
    ASSERT_TRUE(cm().Execute(std::make_unique<SetParentCommand>(&registry, t.head, Entity())).has_value());
    EXPECT_EQ(GetParent(registry, t.head), Entity());
    ExpectNear(WorldPosition(registry, t.head), glm::vec3(10, 1, -1));
    ASSERT_TRUE(cm().Undo().has_value());
    EXPECT_EQ(GetParent(registry, t.head), t.pole);
    const TransformComponent* local = registry.GetComponent<TransformComponent>(t.head);
    EXPECT_FLOAT_EQ(local->position.x, 1.0f);
    EXPECT_FLOAT_EQ(local->position.z, 0.0f);
    ASSERT_TRUE(cm().Redo().has_value());
    ExpectNear(WorldPosition(registry, t.head), glm::vec3(10, 1, -1));
}

TEST_F(HierarchyCommandTest, InstantiateV2_UndoRemovesAll_RedoRestoresSameUuids)
{
    ECSRegistry source;
    LampTree t = MakeLamp(source);
    const auto path = TempPath("qf_lamp_cmd.prefab.json");
    ASSERT_TRUE(PrefabAsset::CaptureFromEntity(source, t.lamp)->SaveToFile(path).has_value());

    auto cmd = std::make_unique<InstantiatePrefabCommand>(&registry, path, Vec3(0, 0, 0));
    auto* raw = cmd.get();
    ASSERT_TRUE(cm().Execute(std::move(cmd)).has_value());
    Entity root = raw->GetSpawnedEntity();
    EXPECT_EQ(registry.GetEntityCount(), 4u);
    Entity head = FindByName(registry, "Head");
    const UUID headUuid = registry.GetUUID(head);
    ExpectNear(WorldPosition(registry, head), glm::vec3(0, 1, -1));   // position override는 루트만
    EXPECT_TRUE(registry.HasComponent<PrefabInstanceComponent>(root));

    ASSERT_TRUE(cm().Undo().has_value());
    EXPECT_EQ(registry.GetEntityCount(), 0u);

    ASSERT_TRUE(cm().Redo().has_value());
    EXPECT_EQ(registry.GetEntityCount(), 4u);
    Entity redoneHead = registry.GetEntityByUUID(headUuid);
    ASSERT_TRUE(registry.IsValid(redoneHead));
    EXPECT_EQ(registry.GetEntityName(GetParent(registry, redoneHead)), "Pole");
    std::filesystem::remove(path);
}

TEST_F(HierarchyCommandTest, RevertV2_RestoresChildren_UndoBringsBackEdits)
{
    ECSRegistry source;
    LampTree t = MakeLamp(source);
    const auto path = TempPath("qf_lamp_revert.prefab.json");
    ASSERT_TRUE(PrefabAsset::CaptureFromEntity(source, t.lamp)->SaveToFile(path).has_value());

    auto cmd = std::make_unique<InstantiatePrefabCommand>(&registry, path);
    auto* raw = cmd.get();
    ASSERT_TRUE(cm().Execute(std::move(cmd)).has_value());
    Entity root = raw->GetSpawnedEntity();

    // 사용자 편집: Head 삭제, Base 이동
    Entity head = FindByName(registry, "Head");
    Entity base = FindByName(registry, "Base");
    ASSERT_TRUE(cm().Execute(std::make_unique<DestroyEntityCommand>(&registry, head)).has_value());
    registry.GetComponent<TransformComponent>(base)->position = Vec3(0, 50, 0);
    const UUID baseUuid = registry.GetUUID(base);

    ASSERT_TRUE(cm().Execute(std::make_unique<RevertPrefabInstanceCommand>(&registry, root)).has_value());
    EXPECT_EQ(GetDescendants(registry, root).size(), 3u);
    Entity revertedHead = FindByName(registry, "Head");
    ASSERT_TRUE(revertedHead.IsValid());
    ExpectNear(WorldPosition(registry, revertedHead), glm::vec3(10, 1, -1));
    EXPECT_NEAR(ComputeWorldTransform(registry, FindByName(registry, "Base")).position.y, -0.5f, 1e-4f);

    ASSERT_TRUE(cm().Undo().has_value());
    EXPECT_EQ(GetDescendants(registry, root).size(), 2u);   // Head는 여전히 삭제된 상태
    Entity restoredBase = registry.GetEntityByUUID(baseUuid);
    ASSERT_TRUE(registry.IsValid(restoredBase));
    EXPECT_NEAR(registry.GetComponent<TransformComponent>(restoredBase)->position.y, 50.0f, 1e-4f);
    EXPECT_EQ(GetParent(registry, restoredBase), root);

    ASSERT_TRUE(cm().Redo().has_value());
    EXPECT_EQ(GetDescendants(registry, root).size(), 3u);
    std::filesystem::remove(path);
}
