// Integration tests: CommandManager + Commands end-to-end
// Validates: Req 7.4, 8.1, 8.2, 8.3, 9.3, 9.4, 9.5, 9.6, 9.7, 10.2, 10.3, 10.4, 10.5

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../editor/commands/MoveEntityCommand.h"
#include "../editor/commands/RotateEntityCommand.h"
#include "../editor/commands/ScaleEntityCommand.h"
#include "../core/CommandManager.h"
#include "../core/EngineError.h"

using namespace Engine;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static Entity CreateEntityWithTransform(ECSRegistry& registry, Vec3 position = Vec3(0, 0, 0))
{
    Entity e = registry.CreateEntity();
    TransformComponent tc;
    tc.position = position;
    tc.rotation = Quaternion(0, 0, 0, 1);
    tc.scale    = Vec3(1, 1, 1);
    registry.AddComponent(e, tc);
    return e;
}

// ─── Test Fixture ─────────────────────────────────────────────────────────────

class IntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        CommandManager::GetInstance().Clear();
        CommandManager::GetInstance().SetMaxUndoDepth(100);
    }

    void TearDown() override
    {
        CommandManager::GetInstance().Clear();
        CommandManager::GetInstance().SetMaxUndoDepth(100);
    }

    ECSRegistry registry_;
};

// ─── Test 1: MoveEntityCommand regression via CommandManager (Req 8.2) ────────

// Validates: Requirements 8.2
// Execute MoveEntityCommand via CommandManager, verify undo/redo stack behavior.
TEST_F(IntegrationTest, MoveEntityCommand_ExecuteUndoRedo_StackStateCorrect)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));

    auto& cm = CommandManager::GetInstance();

    // Execute → CanUndo
    auto execResult = cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(10, 0, 0)));
    ASSERT_TRUE(execResult.has_value());
    EXPECT_TRUE(cm.CanUndo());
    EXPECT_FALSE(cm.CanRedo());

    // Verify position applied
    const TransformComponent* tc = registry_.GetTransformComponent(e);
    ASSERT_NE(tc, nullptr);
    EXPECT_FLOAT_EQ(tc->position.x, 10.0f);

    // Undo → position restored, redo stack populated
    auto undoResult = cm.Undo();
    ASSERT_TRUE(undoResult.has_value());
    EXPECT_FALSE(cm.CanUndo());
    EXPECT_TRUE(cm.CanRedo());

    tc = registry_.GetTransformComponent(e);
    ASSERT_NE(tc, nullptr);
    EXPECT_FLOAT_EQ(tc->position.x, 0.0f);

    // Redo → position re-applied, redo stack cleared
    auto redoResult = cm.Redo();
    ASSERT_TRUE(redoResult.has_value());
    EXPECT_TRUE(cm.CanUndo());
    EXPECT_FALSE(cm.CanRedo());

    tc = registry_.GetTransformComponent(e);
    ASSERT_NE(tc, nullptr);
    EXPECT_FLOAT_EQ(tc->position.x, 10.0f);
}

// ─── Test 2: Full Transform Apply/Undo/Redo round-trip (Move + Rotate + Scale) ─

// Validates: Requirements 8.2
// Three different transform commands on the undo stack; each undo restores the correct value.
TEST_F(IntegrationTest, FullTransformRoundTrip_ThreeCommands_UndoRedoInOrder)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));

    auto& cm = CommandManager::GetInstance();

    // Move
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(5, 0, 0))).has_value());
    // Rotate
    ASSERT_TRUE(cm.Execute(std::make_unique<RotateEntityCommand>(&registry_, e, Quaternion(0, 0.5f, 0, 1))).has_value());
    // Scale
    ASSERT_TRUE(cm.Execute(std::make_unique<ScaleEntityCommand>(&registry_, e, Vec3(2, 2, 2))).has_value());

    // Undo stack should have 3 entries
    EXPECT_TRUE(cm.CanUndo());

    // Undo scale → scale back to (1,1,1)
    ASSERT_TRUE(cm.Undo().has_value());
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->scale.x, 1.0f);
        EXPECT_FLOAT_EQ(tc->scale.y, 1.0f);
        EXPECT_FLOAT_EQ(tc->scale.z, 1.0f);
    }

    // Undo rotate → rotation back to (0,0,0,1)
    ASSERT_TRUE(cm.Undo().has_value());
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->rotation.x, 0.0f);
        EXPECT_FLOAT_EQ(tc->rotation.y, 0.0f);
        EXPECT_FLOAT_EQ(tc->rotation.z, 0.0f);
    }

    // Undo move → position back to (0,0,0)
    ASSERT_TRUE(cm.Undo().has_value());
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->position.x, 0.0f);
        EXPECT_FLOAT_EQ(tc->position.y, 0.0f);
        EXPECT_FLOAT_EQ(tc->position.z, 0.0f);
    }

    EXPECT_FALSE(cm.CanUndo());

    // Redo all three in order
    ASSERT_TRUE(cm.Redo().has_value());  // re-apply move
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->position.x, 5.0f);
    }

    ASSERT_TRUE(cm.Redo().has_value());  // re-apply rotate
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->rotation.y, 0.5f);
    }

    ASSERT_TRUE(cm.Redo().has_value());  // re-apply scale
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->scale.x, 2.0f);
    }

    EXPECT_FALSE(cm.CanRedo());
}

// ─── Test 3: MergeSession — consecutive moves merge to one undo unit (Req 8.1, 8.3) ─

// Validates: Requirements 8.1, 8.3
// Two moves inside a merge session produce one undo entry that reverts to the
// position before the session began.
TEST_F(IntegrationTest, MergeSession_ConsecutiveMoves_MergeToOneUndoEntry)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));

    auto& cm = CommandManager::GetInstance();

    cm.BeginMergeSession();

    // First move: (0,0,0) → (1,0,0)
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 0, 0))).has_value());
    EXPECT_TRUE(cm.CanUndo());

    // Second move: (1,0,0) → (5,0,0) — should merge with the first
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(5, 0, 0))).has_value());

    cm.EndMergeSession();

    // Verify current position is (5,0,0)
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->position.x, 5.0f);
    }

    // Undo once → should revert all the way to (0,0,0), not (1,0,0)
    ASSERT_TRUE(cm.Undo().has_value());
    {
        const TransformComponent* tc = registry_.GetTransformComponent(e);
        ASSERT_NE(tc, nullptr);
        EXPECT_FLOAT_EQ(tc->position.x, 0.0f);
        EXPECT_FLOAT_EQ(tc->position.y, 0.0f);
        EXPECT_FLOAT_EQ(tc->position.z, 0.0f);
    }

    // Stack had only one merged entry — nothing more to undo
    EXPECT_FALSE(cm.CanUndo());
}

// A new merge session (Inspector focus moved to another input) must not merge into a
// command pushed by an earlier session, outside any session, or restored by Redo.
// It used to: one Undo reverted both the X edit and the later Y edit.
TEST_F(IntegrationTest, MergeSession_DoesNotMergeIntoEarlierSessionOrRedo)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));
    auto& cm = CommandManager::GetInstance();
    auto X = [&] { return registry_.GetTransformComponent(e)->position.x; };
    auto Y = [&] { return registry_.GetTransformComponent(e)->position.y; };

    cm.BeginMergeSession();
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 0, 0))).has_value());
    cm.EndMergeSession();
    cm.BeginMergeSession();
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 5, 0))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 6, 0))).has_value());
    cm.EndMergeSession();

    ASSERT_TRUE(cm.Undo().has_value());   // second session only (both of its moves)
    EXPECT_FLOAT_EQ(X(), 1.0f);
    EXPECT_FLOAT_EQ(Y(), 0.0f);
    ASSERT_TRUE(cm.Undo().has_value());   // first session
    EXPECT_FLOAT_EQ(X(), 0.0f);
    EXPECT_FALSE(cm.CanUndo());

    // A command outside a session, then a session: still separate steps.
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(2, 0, 0))).has_value());
    cm.BeginMergeSession();
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(3, 0, 0))).has_value());
    // Redo inside the same session restores an older command; the next edit must not fold into it.
    ASSERT_TRUE(cm.Undo().has_value());
    ASSERT_TRUE(cm.Redo().has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(4, 0, 0))).has_value());
    cm.EndMergeSession();
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FLOAT_EQ(X(), 3.0f);
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FLOAT_EQ(X(), 2.0f);
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FLOAT_EQ(X(), 0.0f);
    EXPECT_FALSE(cm.CanUndo());
}

// ─── Test 4: SetMaxUndoDepth(3) — oldest entry trimmed on 4th command (Req 10.5) ─

// Validates: Requirements 10.5
// When the undo stack depth is capped at 3, executing a 4th command trims the oldest.
TEST_F(IntegrationTest, SetMaxUndoDepth3_FourthCommandTrimmsOldest)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));

    auto& cm = CommandManager::GetInstance();
    cm.SetMaxUndoDepth(3);

    // Execute 4 commands
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 0, 0))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(2, 0, 0))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(3, 0, 0))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(4, 0, 0))).has_value());

    // Can undo exactly 3 times (oldest was trimmed)
    EXPECT_TRUE(cm.CanUndo());
    ASSERT_TRUE(cm.Undo().has_value());  // undo cmd4 → pos (3,0,0)
    EXPECT_TRUE(cm.CanUndo());
    ASSERT_TRUE(cm.Undo().has_value());  // undo cmd3 → pos (2,0,0)
    EXPECT_TRUE(cm.CanUndo());
    ASSERT_TRUE(cm.Undo().has_value());  // undo cmd2 → pos (1,0,0)

    // 4th undo attempt must fail — only 3 entries were kept
    EXPECT_FALSE(cm.CanUndo());
}

// ─── Test 5: Execute() clears redo stack (Req 10.4) ─────────────────────────

// Validates: Requirements 10.4
// After undoing a command, executing a new command clears the redo stack.
TEST_F(IntegrationTest, Execute_AfterUndo_ClearsRedoStack)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));

    auto& cm = CommandManager::GetInstance();

    // Execute two commands
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(1, 0, 0))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(2, 0, 0))).has_value());

    // Undo one → redo stack now has 1 item
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_TRUE(cm.CanRedo());

    // Execute a new command → redo stack must be cleared
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(5, 0, 0))).has_value());
    EXPECT_FALSE(cm.CanRedo());
}

// ─── Test 6: Undo/Redo on empty stacks return InvalidState (Req 9.3, 9.4, 9.5) ─

// Validates: Requirements 9.3, 9.4, 9.5
// Undo/Redo on empty stacks and exhausted stacks return InvalidState.
TEST_F(IntegrationTest, UndoRedo_EmptyStack_ReturnsInvalidState)
{
    auto& cm = CommandManager::GetInstance();

    // Fresh manager: Undo → InvalidState
    auto undoResult = cm.Undo();
    ASSERT_FALSE(undoResult.has_value());
    EXPECT_EQ(undoResult.error().code, EngineErrorCode::InvalidState);

    // Fresh manager: Redo → InvalidState
    auto redoResult = cm.Redo();
    ASSERT_FALSE(redoResult.has_value());
    EXPECT_EQ(redoResult.error().code, EngineErrorCode::InvalidState);

    // Execute a command, undo it, redo it — then redo again → InvalidState
    Entity e = CreateEntityWithTransform(registry_, Vec3(0, 0, 0));
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&registry_, e, Vec3(3, 0, 0))).has_value());
    ASSERT_TRUE(cm.Undo().has_value());
    ASSERT_TRUE(cm.Redo().has_value());

    auto redoAgain = cm.Redo();
    ASSERT_FALSE(redoAgain.has_value());
    EXPECT_EQ(redoAgain.error().code, EngineErrorCode::InvalidState);
}

// ─── Test 7: Stale entity — Apply returns error, ECS state unchanged (Req 7.4) ─

// Validates: Requirements 7.4
// MoveEntityCommand on a destroyed entity returns an error and does not crash.
// MoveEntityCommand checks HasTransformComponent (not IsValid), so after
// DestroyEntity the component is gone → ComponentNotFound.
TEST_F(IntegrationTest, StaleEntity_MoveCommandApply_ReturnsError_NoStateChange)
{
    Entity e = CreateEntityWithTransform(registry_, Vec3(1, 2, 3));

    // Destroy the entity — components are removed but entity id remains non-zero
    registry_.DestroyEntity(e);

    // Attempting to create and apply a MoveEntityCommand on the stale entity
    // The command is applied directly (not via CommandManager) to test raw behavior
    MoveEntityCommand cmd(&registry_, e, Vec3(99, 99, 99));
    auto result = cmd.Apply();

    // Expect an error (ComponentNotFound since MoveEntityCommand checks HasTransformComponent
    // rather than IsValid; after DestroyEntity the component is removed)
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().code == EngineErrorCode::ComponentNotFound ||
                result.error().code == EngineErrorCode::EntityNotFound);

    // No crash, no ECS state modification — registry still intact
    EXPECT_EQ(registry_.GetEntityCount(), 0u);
}

// ─── Test 8: null command → InvalidParameter (Req 9.3) ─────────────────────

// Validates: Requirements 9.3
// Passing nullptr to CommandManager::Execute returns InvalidParameter.
TEST_F(IntegrationTest, Execute_NullCommand_ReturnsInvalidParameter)
{
    auto& cm = CommandManager::GetInstance();

    auto result = cm.Execute(nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidParameter);
}

// Regression: preview used to discard history, and restored UUIDs had new IDs.
#include "../ecs/World.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/CreateEntityCommand.h"
#include "../editor/commands/DestroyEntityCommand.h"
#include "../editor/commands/RemoveComponentCommand.h"
#include "../editor/commands/InstantiatePrefabCommand.h"
#include "../editor/commands/RevertPrefabInstanceCommand.h"
#include <fstream>
#include <filesystem>

class PlayUndoTest : public ::testing::Test {
protected:
    World world;
    CommandManager& cm = CommandManager::GetInstance();
    ECSRegistry& reg = *world.GetRegistry();
    void SetUp() override {
        RegisterPODComponentsReflection();
        cm.Clear(); cm.SetEditorRegistry(&reg);
    }
    void TearDown() override {
        cm.EndPlay(&reg); cm.SetTransactionOpen(false);
        cm.SetEditorRegistry(nullptr); cm.Clear();
    }
    void Preview() { world.Play(); world.Pause(); world.Play(); world.Stop(); }
};

TEST_F(PlayUndoTest, TransformUndoRedoAcrossRepeatedPreview) {
    Entity e = CreateEntityWithTransform(reg);
    UUID uid = reg.GetUUID(e);
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&reg,e,Vec3(4,2,3))));
    ASSERT_TRUE(cm.Execute(std::make_unique<RotateEntityCommand>(&reg,e,Quaternion(0,0,1,0))));
    ASSERT_TRUE(cm.Execute(std::make_unique<ScaleEntityCommand>(&reg,e,Vec3(2,3,4))));
    for(int n=0;n<3;++n) {
        Preview(); EXPECT_NE(reg.GetEntityByUUID(uid).id,e.id);
        ASSERT_TRUE(cm.Undo()); ASSERT_TRUE(cm.Undo()); ASSERT_TRUE(cm.Undo());
        auto* t=reg.GetTransformComponent(reg.GetEntityByUUID(uid));
        EXPECT_FLOAT_EQ(t->position.x,0); EXPECT_FLOAT_EQ(t->scale.x,1); EXPECT_FLOAT_EQ(t->rotation.w,1);
        Preview();
        ASSERT_TRUE(cm.Redo()); ASSERT_TRUE(cm.Redo()); ASSERT_TRUE(cm.Redo());
        t=reg.GetTransformComponent(reg.GetEntityByUUID(uid));
        EXPECT_FLOAT_EQ(t->position.x,4); EXPECT_FLOAT_EQ(t->scale.z,4); EXPECT_FLOAT_EQ(t->rotation.z,1);
    }
}
TEST_F(PlayUndoTest, CreateRemoveDestroySurvivePreview) {
    auto create=std::make_unique<CreateEntityCommand>(&reg,"Object");
    auto* ptr=create.get(); ASSERT_TRUE(cm.Execute(std::move(create)));
    Entity e=ptr->GetCreatedEntity(); UUID uid=reg.GetUUID(e);
    reg.AddComponent(e,TransformComponent{});
    ASSERT_TRUE(cm.Execute(std::make_unique<RemoveComponentCommand>(&reg,e,"TransformComponent")));
    Preview(); ASSERT_TRUE(cm.Undo());
    EXPECT_TRUE(reg.HasTransformComponent(reg.GetEntityByUUID(uid)));
    Preview(); ASSERT_TRUE(cm.Redo()); EXPECT_FALSE(reg.HasTransformComponent(reg.GetEntityByUUID(uid)));
    ASSERT_TRUE(cm.Undo());
    ASSERT_TRUE(cm.Execute(std::make_unique<DestroyEntityCommand>(&reg,reg.GetEntityByUUID(uid))));
    Preview(); ASSERT_TRUE(cm.Undo()); EXPECT_TRUE(reg.GetEntityByUUID(uid).IsValid());
    Preview(); ASSERT_TRUE(cm.Redo()); EXPECT_FALSE(reg.GetEntityByUUID(uid).IsValid());
    ASSERT_TRUE(cm.Undo());
    Preview(); ASSERT_TRUE(cm.Undo()); EXPECT_FALSE(reg.GetEntityByUUID(uid).IsValid());
    ASSERT_TRUE(cm.Redo()); EXPECT_TRUE(reg.GetEntityByUUID(uid).IsValid());
}
TEST_F(PlayUndoTest, PrefabInstantiateAndRevertSurvivePreview) {
    auto path=std::filesystem::temp_directory_path()/"qf-play-undo-test.prefab.json";
    { std::ofstream f(path); f << R"({"version":1,"name":"Prefab","components":{"TransformComponent":{"position":[1,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}})"; }
    auto spawn=std::make_unique<InstantiatePrefabCommand>(&reg,path);
    auto* ptr=spawn.get(); ASSERT_TRUE(cm.Execute(std::move(spawn)));
    Entity e=ptr->GetSpawnedEntity(); UUID uid=reg.GetUUID(e);
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&reg,e,Vec3(9,0,0))));
    ASSERT_TRUE(cm.Execute(std::make_unique<RevertPrefabInstanceCommand>(&reg,e)));
    Preview(); ASSERT_TRUE(cm.Undo()); EXPECT_FLOAT_EQ(reg.GetTransformComponent(reg.GetEntityByUUID(uid))->position.x,9);
    Preview(); ASSERT_TRUE(cm.Redo()); EXPECT_FLOAT_EQ(reg.GetTransformComponent(reg.GetEntityByUUID(uid))->position.x,1);
    ASSERT_TRUE(cm.Undo()); ASSERT_TRUE(cm.Undo());
    Preview(); ASSERT_TRUE(cm.Undo()); EXPECT_FALSE(reg.GetEntityByUUID(uid).IsValid());
    Preview(); ASSERT_TRUE(cm.Redo()); EXPECT_TRUE(reg.GetEntityByUUID(uid).IsValid());
    std::filesystem::remove(path);
}
TEST_F(PlayUndoTest, PlayLocksHistoryAndRestoresSimulationWithoutNewCommands) {
    Entity e=CreateEntityWithTransform(reg); UUID uid=reg.GetUUID(e);
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&reg,e,Vec3(4,0,0))));
    auto count=cm.GetChangeCount(); cm.BeginMergeSession();
    world.Play(); EXPECT_FALSE(cm.IsInMergeSession()); EXPECT_FALSE(cm.CanUndo());
    EXPECT_FALSE(cm.Undo()); EXPECT_FALSE(cm.Redo());
    EXPECT_FALSE(cm.Execute(std::make_unique<MoveEntityCommand>(&reg,e,Vec3(99,0,0))));
    EXPECT_THROW(cm.Clear(),std::runtime_error);
    reg.SetTransformPosition(e,20,0,0); Entity runtime=reg.CreateEntity(); UUID runtimeUid=reg.GetUUID(runtime);
    world.Pause(); EXPECT_FALSE(cm.CanUndo()); world.Stop();
    EXPECT_EQ(cm.GetChangeCount(),count); EXPECT_TRUE(cm.CanUndo());
    EXPECT_FALSE(reg.GetEntityByUUID(runtimeUid).IsValid());
    EXPECT_FLOAT_EQ(reg.GetTransformComponent(reg.GetEntityByUUID(uid))->position.x,4);
    ASSERT_TRUE(cm.Undo()); EXPECT_FLOAT_EQ(reg.GetTransformComponent(reg.GetEntityByUUID(uid))->position.x,0);
}
TEST_F(PlayUndoTest, OpenTransactionRejectsPlayAndOtherWorldDoesNotTouchHistory) {
    Entity e=CreateEntityWithTransform(reg);
    ASSERT_TRUE(cm.Execute(std::make_unique<MoveEntityCommand>(&reg,e,Vec3(4,0,0))));
    cm.SetTransactionOpen(true); EXPECT_THROW(world.Play(),std::runtime_error);
    EXPECT_EQ(world.GetEditorState(),World::EditorState::Edit); EXPECT_TRUE(cm.CanUndo());
    cm.SetTransactionOpen(false);
    { World other; other.Play(); EXPECT_FALSE(cm.IsEditingLocked()); other.Stop(); }
    EXPECT_TRUE(cm.CanUndo()); ASSERT_TRUE(cm.Undo());
}
