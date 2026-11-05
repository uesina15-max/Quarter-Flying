// Tests for CreateEntityCommand and DestroyEntityCommand
// Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 9.1, 10.1

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/CreateEntityCommand.h"
#include "../editor/commands/DestroyEntityCommand.h"
#include "../core/EngineError.h"

using namespace Engine;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Reflection을 사전에 등록해두어야 serialize/deserialize 가 작동한다.
// 테스트 픽스처에서 한 번만 등록한다.
class EntityCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 반복 등록이 안전한지 확인하기 위해 매 테스트마다 호출하지 않고
        // 이미 등록된 경우는 GetAllComponents()가 동일 맵을 반환한다.
        RegisterPODComponentsReflection();
    }

    ECSRegistry registry;
};

// ─── CreateEntityCommand ─────────────────────────────────────────────────────

// Req 3.1 – Apply() 시 새 Entity 생성
TEST_F(EntityCommandTest, CreateEntity_Apply_CreatesValidEntity)
{
    CreateEntityCommand cmd(&registry, "TestEntity");

    ASSERT_TRUE(cmd.Apply().has_value());

    Entity e = cmd.GetCreatedEntity();
    EXPECT_TRUE(registry.IsValid(e));
    EXPECT_EQ(registry.GetEntityName(e), "TestEntity");
}

// Req 3.3 – Undo() 시 Entity 삭제
TEST_F(EntityCommandTest, CreateEntity_ApplyThenUndo_DestroysEntity)
{
    CreateEntityCommand cmd(&registry, "ToUndo");

    ASSERT_TRUE(cmd.Apply().has_value());
    Entity e = cmd.GetCreatedEntity();
    ASSERT_TRUE(registry.IsValid(e));

    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry.IsValid(e));
}

// Req 3.2, 10.1 – Undo → Redo 후 동일 UUID 복원
TEST_F(EntityCommandTest, CreateEntity_UndoThenRedo_RestoresSameUUID)
{
    CreateEntityCommand cmd(&registry, "UUIDTest");

    // 최초 Apply
    ASSERT_TRUE(cmd.Apply().has_value());
    UUID firstUUID = registry.GetUUID(cmd.GetCreatedEntity());

    // Undo
    ASSERT_TRUE(cmd.Undo().has_value());

    // Redo (두 번째 Apply)
    ASSERT_TRUE(cmd.Apply().has_value());
    UUID redoUUID = registry.GetUUID(cmd.GetCreatedEntity());

    EXPECT_EQ(firstUUID, redoUUID) << "Redo must restore the same UUID as the initial create";
    EXPECT_TRUE(registry.IsValid(cmd.GetCreatedEntity()));
}

// Req 10.1 – Apply → Undo → Apply 반복 라운드트립
TEST_F(EntityCommandTest, CreateEntity_MultipleRoundTrips_ConsistentUUID)
{
    CreateEntityCommand cmd(&registry, "RoundTrip");

    ASSERT_TRUE(cmd.Apply().has_value());
    UUID uuid1 = registry.GetUUID(cmd.GetCreatedEntity());

    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(cmd.Undo().has_value());
        ASSERT_TRUE(cmd.Apply().has_value());
        UUID uuidN = registry.GetUUID(cmd.GetCreatedEntity());
        EXPECT_EQ(uuid1, uuidN) << "UUID must be stable across round-trip " << i;
    }
}

// Req 3.3 – Undo에서 이미 삭제된 Entity → EntityNotFound
TEST_F(EntityCommandTest, CreateEntity_Undo_OnAlreadyDestroyedEntity_ReturnsError)
{
    CreateEntityCommand cmd(&registry);

    ASSERT_TRUE(cmd.Apply().has_value());
    Entity e = cmd.GetCreatedEntity();

    // 외부에서 삭제
    registry.DestroyEntity(e);

    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// Req 9.1 – null registry → NotInitialized
TEST_F(EntityCommandTest, CreateEntity_NullRegistry_ReturnsNotInitialized)
{
    CreateEntityCommand cmd(nullptr, "NoReg");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Req 9.1 – null registry Undo → NotInitialized
TEST_F(EntityCommandTest, CreateEntity_NullRegistry_Undo_ReturnsNotInitialized)
{
    CreateEntityCommand cmd(nullptr);
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// 이름 없이 생성해도 Apply가 성공하는지 확인
TEST_F(EntityCommandTest, CreateEntity_NoName_Apply_Succeeds)
{
    CreateEntityCommand cmd(&registry);
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry.IsValid(cmd.GetCreatedEntity()));
}

// ─── DestroyEntityCommand ────────────────────────────────────────────────────

// Req 3.5 – Apply()로 Entity 삭제
TEST_F(EntityCommandTest, DestroyEntity_Apply_DestroysEntity)
{
    Entity e = registry.CreateEntity();
    registry.SetEntityName(e, "ToDestroy");

    DestroyEntityCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());

    EXPECT_FALSE(registry.IsValid(e));
}

// Req 3.6 – Undo() 시 Entity 및 Component 복원
TEST_F(EntityCommandTest, DestroyEntity_ApplyThenUndo_RestoresEntityAndComponents)
{
    Entity e = registry.CreateEntity();
    registry.SetEntityName(e, "WithTransform");
    UUID originalUUID = registry.GetUUID(e);

    // TransformComponent 추가
    TransformComponent tc;
    tc.position = Vec3(1.0f, 2.0f, 3.0f);
    tc.scale    = Vec3(2.0f, 2.0f, 2.0f);
    registry.AddComponent(e, tc);

    DestroyEntityCommand cmd(&registry, e);

    // Apply: 삭제
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry.IsValid(e));

    // Undo: 복원
    ASSERT_TRUE(cmd.Undo().has_value());

    // GetAllEntities로 이름 검색
    bool found = false;
    for (Entity ent : registry.GetAllEntities())
    {
        if (registry.GetEntityName(ent) == "WithTransform")
        {
            found = true;
            // TransformComponent 복원 확인
            ASSERT_TRUE(registry.HasTransformComponent(ent));
            const TransformComponent* restoredTc = registry.GetTransformComponent(ent);
            ASSERT_NE(restoredTc, nullptr);
            EXPECT_FLOAT_EQ(restoredTc->position.x, 1.0f);
            EXPECT_FLOAT_EQ(restoredTc->position.y, 2.0f);
            EXPECT_FLOAT_EQ(restoredTc->position.z, 3.0f);
            EXPECT_FLOAT_EQ(restoredTc->scale.x, 2.0f);
            // UUID도 동일한지 확인
            EXPECT_EQ(registry.GetUUID(ent), originalUUID);
            break;
        }
    }
    EXPECT_TRUE(found) << "Restored entity should be findable by name";
}

// Req 3.6 – Undo 후 UUID가 동일한지 확인 (Req 10.1 라운드트립)
TEST_F(EntityCommandTest, DestroyEntity_UndoRestoresSameUUID)
{
    Entity e = registry.CreateEntity();
    UUID originalUUID = registry.GetUUID(e);
    registry.SetEntityName(e, "UUIDCheck");

    DestroyEntityCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(cmd.Undo().has_value());

    // 복원된 entity가 동일 UUID를 가지는지 확인
    bool foundSameUUID = false;
    for (Entity ent : registry.GetAllEntities())
    {
        if (registry.GetEntityName(ent) == "UUIDCheck")
        {
            EXPECT_EQ(registry.GetUUID(ent), originalUUID);
            foundSameUUID = true;
            break;
        }
    }
    EXPECT_TRUE(foundSameUUID) << "Restored entity must have the original UUID";
}

// Req 3.7 – Apply() 시 대상 Entity가 유효하지 않으면 EntityNotFound
TEST_F(EntityCommandTest, DestroyEntity_Apply_InvalidEntity_ReturnsEntityNotFound)
{
    Entity e = registry.CreateEntity();
    registry.DestroyEntity(e);  // 이미 삭제된 Entity

    DestroyEntityCommand cmd(&registry, e);
    auto result = cmd.Apply();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// Req 9.1 – null registry → NotInitialized
TEST_F(EntityCommandTest, DestroyEntity_NullRegistry_ReturnsNotInitialized)
{
    Entity e(1);
    DestroyEntityCommand cmd(nullptr, e);
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Req 9.1 – null registry Undo → NotInitialized
TEST_F(EntityCommandTest, DestroyEntity_NullRegistry_Undo_ReturnsNotInitialized)
{
    Entity e(1);
    DestroyEntityCommand cmd(nullptr, e);
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Undo before Apply (no snapshot) → InvalidState
TEST_F(EntityCommandTest, DestroyEntity_UndoWithoutApply_ReturnsInvalidState)
{
    Entity e = registry.CreateEntity();
    DestroyEntityCommand cmd(&registry, e);

    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidState);
}

// Req 10.1 – Apply → Undo → Apply (Redo) 라운드트립
TEST_F(EntityCommandTest, DestroyEntity_ApplyUndoApply_RoundTrip)
{
    Entity e = registry.CreateEntity();
    registry.SetEntityName(e, "RoundTripEntity");
    UUID originalUUID = registry.GetUUID(e);

    TransformComponent tc;
    tc.position = Vec3(5.0f, 6.0f, 7.0f);
    registry.AddComponent(e, tc);

    DestroyEntityCommand cmd(&registry, e);

    // Apply (삭제)
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 0u);

    // Undo (복원)
    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 1u);

    // Redo (재삭제)
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 0u);
}

// ─── Create + Destroy 상호작용 ─────────────────────────────────────────────

// CreateEntity → DestroyEntity → Undo(Destroy) 후 동일 UUID 유지 확인
TEST_F(EntityCommandTest, CreateThenDestroy_UndoRedoChain)
{
    // 1. Entity 생성
    CreateEntityCommand createCmd(&registry, "ChainEntity");
    ASSERT_TRUE(createCmd.Apply().has_value());
    Entity created = createCmd.GetCreatedEntity();
    UUID createdUUID = registry.GetUUID(created);
    EXPECT_TRUE(registry.IsValid(created));

    // 2. 생성된 Entity를 삭제하는 커맨드
    DestroyEntityCommand destroyCmd(&registry, created);
    ASSERT_TRUE(destroyCmd.Apply().has_value());
    EXPECT_FALSE(registry.IsValid(created));
    EXPECT_EQ(registry.GetAllEntities().size(), 0u);

    // 3. Destroy Undo → Entity 복원
    ASSERT_TRUE(destroyCmd.Undo().has_value());
    EXPECT_EQ(registry.GetAllEntities().size(), 1u);

    // 4. 복원된 entity가 동일 UUID를 가지는지 확인
    Entity restoredEntity = registry.GetAllEntities()[0];
    EXPECT_EQ(registry.GetUUID(restoredEntity), createdUUID);
    EXPECT_EQ(registry.GetEntityName(restoredEntity), "ChainEntity");
}

// ─── CommandManager 변경 카운터 (USABILITY_IMPROVEMENT_PLAN E-5: 저장 안 된 변경 표시) ──────────

#include "../core/CommandManager.h"

// 에디터는 저장 시점의 GetChangeCount()와 지금 값을 비교해 창 제목에 '*'를 붙인다.
// 싱글턴이라 다른 테스트가 남긴 값이 있을 수 있으므로 절대값이 아니라 증가량만 본다.
TEST_F(EntityCommandTest, ChangeCount_IncrementsOnExecuteUndoRedo_AndSurvivesClear)
{
    CommandManager& cm = CommandManager::GetInstance();
    cm.Clear();
    const uint64_t start = cm.GetChangeCount();

    ASSERT_TRUE(cm.Execute(std::make_unique<CreateEntityCommand>(&registry, "Counted")).has_value());
    EXPECT_EQ(cm.GetChangeCount(), start + 1);

    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_EQ(cm.GetChangeCount(), start + 2);   // Undo도 월드를 바꾼다 - 저장 시점과 달라짐

    ASSERT_TRUE(cm.Redo().has_value());
    EXPECT_EQ(cm.GetChangeCount(), start + 3);

    // Clear(Play 종료 등)는 카운터를 되돌리지 않는다. 되돌리면 예전 저장 시점의 값과 우연히 같아져
    // 변경이 있는데도 "저장됨"으로 보일 수 있다.
    cm.Clear();
    EXPECT_EQ(cm.GetChangeCount(), start + 3);
}

TEST_F(EntityCommandTest, ChangeCount_DoesNotChangeOnFailedUndoOrRedo)
{
    CommandManager& cm = CommandManager::GetInstance();
    cm.Clear();
    const uint64_t start = cm.GetChangeCount();

    EXPECT_FALSE(cm.Undo().has_value());   // 되돌릴 것이 없음
    EXPECT_FALSE(cm.Redo().has_value());
    EXPECT_EQ(cm.GetChangeCount(), start);
}

TEST_F(EntityCommandTest, DestroyUndo_RestoresExternalCameraTargetsAcrossRepeatedRedo)
{
    ASSERT_NE(ComponentRegistry::GetComponentInfo("CameraFollowComponent"), nullptr);
    Entity target = registry.CreateEntity();
    registry.AddComponent(target, TransformComponent{});
    const UUID targetUUID = registry.GetUUID(target);
    Entity camera = registry.CreateEntity();
    CameraFollowComponent follow;
    follow.target = target;
    follow.offset = Vec3(2, 5, 9);
    follow.damping = 0.7f;
    registry.AddComponent(camera, follow);
    CameraLookAtComponent look;
    look.target = target;
    registry.AddComponent(camera, look);
    Entity other = registry.CreateEntity();
    Entity untouchedCamera = registry.CreateEntity();
    follow.target = other;
    registry.AddComponent(untouchedCamera, follow);
    DestroyEntityCommand command(&registry, target);
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(command.Apply().has_value());
        EXPECT_FALSE(registry.GetComponent<CameraFollowComponent>(camera)->target.IsValid());
        ASSERT_TRUE(command.Undo().has_value());
        const Entity restored = registry.GetEntityByUUID(targetUUID);
        ASSERT_TRUE(registry.IsValid(restored));
        EXPECT_EQ(registry.GetComponent<CameraFollowComponent>(camera)->target.id, restored.id);
        EXPECT_EQ(registry.GetComponent<CameraLookAtComponent>(camera)->target.id, restored.id);
        EXPECT_FLOAT_EQ(registry.GetComponent<CameraFollowComponent>(camera)->offset.x, 2.0f);
        EXPECT_FLOAT_EQ(registry.GetComponent<CameraFollowComponent>(camera)->damping, 0.7f);
        EXPECT_EQ(registry.GetComponent<CameraFollowComponent>(untouchedCamera)->target.id, other.id);
    }
}

TEST_F(EntityCommandTest, DestroySubtree_RestoresExternalReferenceToChild)
{
    ASSERT_NE(ComponentRegistry::GetComponentInfo("CameraFollowComponent"), nullptr);
    Entity parent = registry.CreateEntity();
    Entity child = registry.CreateEntity();
    registry.AddComponent(parent, TransformComponent{});
    registry.AddComponent(child, TransformComponent{});
    HierarchyComponent hierarchy;
    hierarchy.parent = parent;
    registry.AddComponent(child, hierarchy);
    const UUID childUUID = registry.GetUUID(child);
    Entity camera = registry.CreateEntity();
    CameraFollowComponent follow;
    follow.target = child;
    registry.AddComponent(camera, follow);
    DestroyEntityCommand command(&registry, parent);
    ASSERT_TRUE(command.Apply().has_value());
    ASSERT_TRUE(command.Undo().has_value());
    EXPECT_EQ(registry.GetComponent<CameraFollowComponent>(camera)->target.id,
              registry.GetEntityByUUID(childUUID).id);
}
