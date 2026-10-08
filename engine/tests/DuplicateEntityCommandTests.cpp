// 개선안 P1-5: DuplicateEntityCommand — 엔티티+자손 복제의 Undo/Redo와 EntityRef 재매핑.
//
// 복제의 핵심 계약(MakeDuplicateSnapshots): 복제된 트리 "안"을 가리키는 EntityRef는 복제본으로
// 재연결하고, 트리 "밖"을 가리키는 참조(루트의 부모, 외부 엔티티를 가리키는 카메라 target 등)는
// 그대로 둔다. 그래서 복제본은 원본과 같은 부모 아래로 들어가고, 복제본끼리의 부모-자식/타깃
// 관계는 원본이 아니라 복제본끼리 연결된다. 아래 테스트가 그 양쪽을 모두 고정한다.

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Hierarchy.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/DuplicateEntityCommand.h"
#include "../core/EngineError.h"
#include "../core/UUID.h"
#include <set>

using namespace Engine;

namespace
{
    Entity MakeNode(ECSRegistry& registry, const std::string& name, Vec3 position = Vec3(0, 0, 0))
    {
        Entity e = registry.CreateEntity();
        registry.SetEntityName(e, name);
        TransformComponent t;
        t.position = position;
        registry.AddComponent(e, t);
        return e;
    }

    // Lamp(루트) -> Pole -> Head, Lamp -> Base  (HierarchyTests.cpp의 Lamp 트리와 같은 모양)
    struct LampTree
    {
        Entity lamp, pole, head, base;
    };

    LampTree MakeLamp(ECSRegistry& registry)
    {
        LampTree t;
        t.lamp = MakeNode(registry, "Lamp", Vec3(10, 0, 0));
        t.pole = MakeNode(registry, "Pole", Vec3(0, 1, 0));
        t.head = MakeNode(registry, "Head", Vec3(1, 0, 0));
        t.base = MakeNode(registry, "Base", Vec3(0, -0.5f, 0));
        EXPECT_TRUE(SetParent(registry, t.pole, t.lamp).has_value());
        EXPECT_TRUE(SetParent(registry, t.head, t.pole).has_value());
        EXPECT_TRUE(SetParent(registry, t.base, t.lamp).has_value());
        return t;
    }
}

// ── 단일 엔티티 ──────────────────────────────────────────────────────────────

TEST(DuplicateEntityCommandTest, SingleEntity_CreatesDistinctCopyWithSameComponents)
{
    ECSRegistry registry;
    Entity original = MakeNode(registry, "Box", Vec3(1, 2, 3));
    UUID originalUUID = registry.GetUUID(original);

    DuplicateEntityCommand cmd(&registry, original);
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity copy = cmd.GetDuplicatedRoot();
    EXPECT_TRUE(registry.IsValid(original));            // 원본 그대로
    EXPECT_TRUE(registry.IsValid(copy));
    EXPECT_NE(copy, original);
    EXPECT_NE(registry.GetUUID(copy), originalUUID);     // 새 UUID
    EXPECT_EQ(registry.GetEntityName(copy), "Box");      // 이름 복사
    EXPECT_EQ(registry.GetAllEntities().size(), 2u);

    const TransformComponent* tc = registry.GetComponent<TransformComponent>(copy);
    ASSERT_NE(tc, nullptr);                               // Transform 값까지 복사
    EXPECT_FLOAT_EQ(tc->position.x, 1.0f);
    EXPECT_FLOAT_EQ(tc->position.y, 2.0f);
    EXPECT_FLOAT_EQ(tc->position.z, 3.0f);
}

// ── 서브트리 ─────────────────────────────────────────────────────────────────

TEST(DuplicateEntityCommandTest, Subtree_CopiesAllDescendants_WithNewUUIDs)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    DuplicateEntityCommand cmd(&registry, t.lamp);
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity root = cmd.GetDuplicatedRoot();
    EXPECT_EQ(GetParent(registry, root), Entity());       // Lamp는 루트였으니 복제본도 루트
    EXPECT_EQ(registry.GetAllEntities().size(), 8u);       // 4 + 4

    // 복제 트리 구조가 원본과 같은 모양인지 (전위 순서: pole, head, base)
    auto desc = GetDescendants(registry, root);
    ASSERT_EQ(desc.size(), 3u);
    EXPECT_EQ(GetParent(registry, desc[0]), root);         // pole 복제본 -> lamp 복제본
    EXPECT_EQ(GetParent(registry, desc[1]), desc[0]);      // head 복제본 -> pole 복제본
    EXPECT_EQ(GetParent(registry, desc[2]), root);         // base 복제본 -> lamp 복제본

    // 원본은 전혀 건드리지 않았다
    EXPECT_EQ(GetParent(registry, t.pole), t.lamp);
    EXPECT_EQ(GetDescendants(registry, t.lamp).size(), 3u);

    // 복제본 UUID는 원본 UUID 어느 것과도 겹치지 않는다
    std::set<uint64_t> originalUUIDs = {
        registry.GetUUID(t.lamp).GetValue(), registry.GetUUID(t.pole).GetValue(),
        registry.GetUUID(t.head).GetValue(), registry.GetUUID(t.base).GetValue()};
    for (Entity e : {root, desc[0], desc[1], desc[2]})
    {
        EXPECT_EQ(originalUUIDs.count(registry.GetUUID(e).GetValue()), 0u);
    }
}

// 트리 밖(원본의 부모)을 가리키는 참조는 보존 + 트리 안을 가리키는 참조는 재매핑 — 한 번에.
TEST(DuplicateEntityCommandTest, Subtree_KeepsExternalParent_RemapsInternalParent)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    // Pole(자식 Head를 가진 중간 노드)만 복제한다. Pole의 부모 Lamp는 복제 트리 "밖"이다.
    DuplicateEntityCommand cmd(&registry, t.pole);
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity root = cmd.GetDuplicatedRoot();           // Pole 복제본
    EXPECT_EQ(GetParent(registry, root), t.lamp);    // 밖을 가리키는 부모 → 그대로(원본 Lamp 아래)

    auto desc = GetDescendants(registry, root);
    ASSERT_EQ(desc.size(), 1u);                       // Head 복제본
    EXPECT_EQ(GetParent(registry, desc[0]), root);    // 안을 가리키는 부모 → 복제본으로 재매핑
    EXPECT_NE(GetParent(registry, desc[0]), t.pole);  // 원본 Pole이 아니다
}

// ── EntityRef(카메라 target) 재매핑 ─────────────────────────────────────────

TEST(DuplicateEntityCommandTest, ExternalEntityRef_Preserved)
{
    ECSRegistry registry;
    Entity target = MakeNode(registry, "Target", Vec3(5, 0, 0));
    Entity cam    = MakeNode(registry, "Cam");
    CameraFollowComponent fc;
    fc.target = target;              // 복제 트리 밖의 엔티티를 가리킨다
    registry.AddComponent(cam, fc);

    DuplicateEntityCommand cmd(&registry, cam);       // 카메라 단일 복제
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity copy = cmd.GetDuplicatedRoot();
    const CameraFollowComponent* fcCopy = registry.GetComponent<CameraFollowComponent>(copy);
    ASSERT_NE(fcCopy, nullptr);
    EXPECT_EQ(fcCopy->target, target);                // 밖을 가리키는 참조 → 그대로
}

TEST(DuplicateEntityCommandTest, InternalEntityRef_RemappedToCopy)
{
    ECSRegistry registry;
    Entity pivot = MakeNode(registry, "Pivot");
    Entity cam   = MakeNode(registry, "ChildCam");
    ASSERT_TRUE(SetParent(registry, cam, pivot).has_value());  // cam은 pivot의 자식(같은 트리)
    CameraFollowComponent fc;
    fc.target = pivot;               // 복제 트리 "안"의 엔티티(자기 부모)를 가리킨다
    registry.AddComponent(cam, fc);

    DuplicateEntityCommand cmd(&registry, pivot);     // pivot + cam 함께 복제
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity pivotCopy = cmd.GetDuplicatedRoot();
    auto desc = GetDescendants(registry, pivotCopy);
    ASSERT_EQ(desc.size(), 1u);
    Entity camCopy = desc[0];

    const CameraFollowComponent* fcCopy = registry.GetComponent<CameraFollowComponent>(camCopy);
    ASSERT_NE(fcCopy, nullptr);
    EXPECT_EQ(fcCopy->target, pivotCopy);             // 안을 가리키는 참조 → 복제본으로
    EXPECT_NE(fcCopy->target, pivot);                 // 원본 pivot이 아니다
}

// ── Undo / Redo ──────────────────────────────────────────────────────────────

TEST(DuplicateEntityCommandTest, Undo_RemovesAllCopies_LeavesOriginals)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    DuplicateEntityCommand cmd(&registry, t.lamp);
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 8u);

    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 4u);   // 복제본만 사라진다
    EXPECT_TRUE(registry.IsValid(t.lamp));
    EXPECT_TRUE(registry.IsValid(t.pole));
    EXPECT_EQ(GetParent(registry, t.pole), t.lamp);    // 원본 계층 그대로
}

TEST(DuplicateEntityCommandTest, UndoThenRedo_RestoresSameUUIDsAndStructure)
{
    ECSRegistry registry;
    LampTree t = MakeLamp(registry);

    DuplicateEntityCommand cmd(&registry, t.lamp);
    ASSERT_TRUE(cmd.Apply().has_value());
    UUID rootUUID = registry.GetUUID(cmd.GetDuplicatedRoot());

    ASSERT_TRUE(cmd.Undo().has_value());
    ASSERT_TRUE(cmd.Apply().has_value());               // Redo

    Entity root = cmd.GetDuplicatedRoot();
    EXPECT_EQ(registry.GetUUID(root), rootUUID);         // 같은 UUID로 복원
    EXPECT_EQ(registry.GetAllEntities().size(), 8u);
    auto desc = GetDescendants(registry, root);
    ASSERT_EQ(desc.size(), 3u);
    EXPECT_EQ(GetParent(registry, desc[1]), desc[0]);    // head 복제본 -> pole 복제본 (구조 유지)
}

// ── 에러 경로 ────────────────────────────────────────────────────────────────

TEST(DuplicateEntityCommandTest, Apply_InvalidSource_ReturnsEntityNotFound)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.DestroyEntity(e);

    DuplicateEntityCommand cmd(&registry, e);
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

TEST(DuplicateEntityCommandTest, NullRegistry_Apply_ReturnsNotInitialized)
{
    DuplicateEntityCommand cmd(nullptr, Entity(1));
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

TEST(DuplicateEntityCommandTest, NullRegistry_Undo_ReturnsNotInitialized)
{
    DuplicateEntityCommand cmd(nullptr, Entity(1));
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

TEST(DuplicateEntityCommandTest, UndoWithoutApply_ReturnsInvalidState)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    DuplicateEntityCommand cmd(&registry, e);

    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidState);
}
