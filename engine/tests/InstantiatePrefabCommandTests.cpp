// Tests for InstantiatePrefabCommand (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 2)
// Follows the style of CreateDestroyEntityCommandTests.cpp / EditorCommandIntegrationTests.cpp.

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/InstantiatePrefabCommand.h"
#include "../editor/commands/MoveEntityCommand.h"
#include "../prefab/PrefabAsset.h"
#include "../prefab/PrefabInstanceComponent.h"
#include "../core/CommandManager.h"
#include "../core/Transaction.h"
#include "../core/EngineError.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace
{
    std::filesystem::path WriteValidPrefabFile(const char* filename)
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / filename;
        std::ofstream file(path);
        file << R"({
            "version": 1,
            "name": "TestPrefab",
            "components": {
                "TransformComponent": {"_version": 1, "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1]},
                "RenderableComponent": {"_version": 1, "meshHandle": 3, "materialHandle": 0, "castShadows": true}
            }
        })";
        return path;
    }

    // "RenderableComponent.meshHandle" is a string instead of a number -> the
    // GE_BEGIN_COMPONENT-generated deserialize throws json::type_error, which
    // PrefabAsset::ApplyToEntity's DeserializePrefabComponents step turns into a
    // controlled EngineError (same mechanism exercised in PrefabAssetTests.cpp).
    std::filesystem::path WriteMalformedPrefabFile(const char* filename)
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / filename;
        std::ofstream file(path);
        file << R"({
            "version": 1,
            "name": "BadPrefab",
            "components": {
                "RenderableComponent": {"_version": 1, "meshHandle": "not-a-number", "materialHandle": 0, "castShadows": true}
            }
        })";
        return path;
    }
}

// ─── Test Fixture ─────────────────────────────────────────────────────────────

class InstantiatePrefabCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 반복 등록이 안전한지 확인하기 위해(CreateDestroyEntityCommandTests.cpp와 동일 관례).
        RegisterPODComponentsReflection();
    }

    ECSRegistry registry;
};

// ─── Apply / component values ──────────────────────────────────────────────────

TEST_F(InstantiatePrefabCommandTest, Apply_ValidPrefab_CreatesEntityWithComponentsAndInstanceTag)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_valid1.prefab.json");

    InstantiatePrefabCommand cmd(&registry, path);
    ASSERT_TRUE(cmd.Apply().has_value());

    Entity e = cmd.GetSpawnedEntity();
    ASSERT_TRUE(registry.IsValid(e));

    RenderableComponent* r = registry.GetComponent<RenderableComponent>(e);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->meshHandle, 3u);

    PrefabInstanceComponent* meta = registry.GetComponent<PrefabInstanceComponent>(e);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->prefabPath, path.string());
    EXPECT_EQ(meta->sourcePrefabVersion, 1u);

    std::filesystem::remove(path);
}

TEST_F(InstantiatePrefabCommandTest, Apply_WithPositionOverride_SetsOnlyPosition)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_position.prefab.json");
    InstantiatePrefabCommand cmd(&registry, path, Vec3(10.0f, 20.0f, 30.0f));

    ASSERT_TRUE(cmd.Apply().has_value());
    TransformComponent* t = registry.GetComponent<TransformComponent>(cmd.GetSpawnedEntity());
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 10.0f);
    EXPECT_FLOAT_EQ(t->position.y, 20.0f);
    EXPECT_FLOAT_EQ(t->position.z, 30.0f);

    std::filesystem::remove(path);
}

// ─── Undo / Redo ────────────────────────────────────────────────────────────────

TEST_F(InstantiatePrefabCommandTest, ApplyThenUndo_DestroysEntity)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_undo.prefab.json");
    InstantiatePrefabCommand cmd(&registry, path);
    ASSERT_TRUE(cmd.Apply().has_value());
    Entity e = cmd.GetSpawnedEntity();
    ASSERT_TRUE(registry.IsValid(e));

    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry.IsValid(e));

    std::filesystem::remove(path);
}

TEST_F(InstantiatePrefabCommandTest, UndoThenRedo_RestoresSameUUIDAndComponents_EvenIfFileDeletedMeanwhile)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_redo.prefab.json");
    InstantiatePrefabCommand cmd(&registry, path);

    ASSERT_TRUE(cmd.Apply().has_value());
    UUID firstUUID = registry.GetUUID(cmd.GetSpawnedEntity());

    ASSERT_TRUE(cmd.Undo().has_value());

    // Simulate the prefab file changing/disappearing between Undo and Redo —
    // Redo must still reproduce exactly what Undo removed, using the PrefabAsset
    // cached from the first Apply() rather than re-reading the file.
    std::filesystem::remove(path);

    ASSERT_TRUE(cmd.Apply().has_value()); // Redo
    Entity redone = cmd.GetSpawnedEntity();
    EXPECT_EQ(registry.GetUUID(redone), firstUUID);

    RenderableComponent* r = registry.GetComponent<RenderableComponent>(redone);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->meshHandle, 3u);

    PrefabInstanceComponent* meta = registry.GetComponent<PrefabInstanceComponent>(redone);
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->sourcePrefabVersion, 1u);
}

TEST_F(InstantiatePrefabCommandTest, CommandManager_ExecuteUndoRedo_StackStateCorrect)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_cm_redo.prefab.json");
    auto& cm = CommandManager::GetInstance();
    cm.Clear();

    auto cmd = std::make_unique<InstantiatePrefabCommand>(&registry, path);
    InstantiatePrefabCommand* rawPtr = cmd.get();

    ASSERT_TRUE(cm.Execute(std::move(cmd)).has_value());
    Entity spawned = rawPtr->GetSpawnedEntity();
    ASSERT_TRUE(registry.IsValid(spawned));
    EXPECT_TRUE(cm.CanUndo());
    EXPECT_FALSE(cm.CanRedo());

    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FALSE(registry.IsValid(spawned));
    EXPECT_TRUE(cm.CanRedo());

    ASSERT_TRUE(cm.Redo().has_value());
    EXPECT_TRUE(registry.IsValid(rawPtr->GetSpawnedEntity()));

    cm.Clear();
    std::filesystem::remove(path);
}

// ─── Transaction grouping ───────────────────────────────────────────────────────

// Mirrors EditorAPI::Dispatch's in-transaction path (EditorAPI.cpp): each command
// is Apply()ed immediately for live preview, then tracked via
// Transaction::AddAppliedCommand() so a single Undo reverts the whole group.
TEST_F(InstantiatePrefabCommandTest, Transaction_InstantiateThenMove_SingleUndoRevertsBoth)
{
    auto path = WriteValidPrefabFile("instantiate_cmd_test_txn.prefab.json");
    auto& cm = CommandManager::GetInstance();
    cm.Clear();

    auto txn = std::make_unique<Transaction>("Instantiate + Move");

    auto instantiateCmd = std::make_unique<InstantiatePrefabCommand>(&registry, path);
    ASSERT_TRUE(instantiateCmd->Apply().has_value());
    Entity spawned = instantiateCmd->GetSpawnedEntity();
    txn->AddAppliedCommand(std::move(instantiateCmd));

    auto moveCmd = std::make_unique<MoveEntityCommand>(&registry, spawned, Vec3(99.0f, 0.0f, 0.0f));
    ASSERT_TRUE(moveCmd->Apply().has_value());
    txn->AddAppliedCommand(std::move(moveCmd));

    TransformComponent* t = registry.GetComponent<TransformComponent>(spawned);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 99.0f);

    // Push the already fully-applied transaction onto the undo stack as a single
    // entry — CommandManager::Execute() calls Apply() again, which is a no-op
    // here since Transaction::Apply()'s loop starts at appliedCount_.
    ASSERT_TRUE(cm.Execute(std::move(txn)).has_value());
    EXPECT_TRUE(cm.CanUndo());

    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FALSE(registry.IsValid(spawned)); // both the move and the instantiation undone

    cm.Clear();
    std::filesystem::remove(path);
}

// ─── Failure paths (no partial state left behind) ──────────────────────────────

TEST_F(InstantiatePrefabCommandTest, Apply_NonexistentPath_ReturnsErrorAndCreatesNoEntity)
{
    size_t before = registry.GetEntityCount();
    InstantiatePrefabCommand cmd(&registry,
        std::filesystem::temp_directory_path() / "instantiate_cmd_test_does_not_exist_12345.prefab.json");

    auto result = cmd.Apply();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(registry.GetEntityCount(), before);
}

TEST_F(InstantiatePrefabCommandTest, Apply_MalformedPrefabData_ReturnsErrorAndCreatesNoEntity)
{
    auto path = WriteMalformedPrefabFile("instantiate_cmd_test_malformed.prefab.json");
    size_t before = registry.GetEntityCount();

    InstantiatePrefabCommand cmd(&registry, path);
    auto result = cmd.Apply();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(registry.GetEntityCount(), before);

    std::filesystem::remove(path);
}

TEST_F(InstantiatePrefabCommandTest, Apply_NullRegistry_ReturnsNotInitialized)
{
    InstantiatePrefabCommand cmd(nullptr, "irrelevant.prefab.json");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}
