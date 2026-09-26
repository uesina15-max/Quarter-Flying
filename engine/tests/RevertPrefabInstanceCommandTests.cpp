// Tests for RevertPrefabInstanceCommand (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4)
// Follows the style of InstantiatePrefabCommandTests.cpp.

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/RevertPrefabInstanceCommand.h"
#include "../prefab/PrefabAsset.h"
#include "../prefab/PrefabInstanceComponent.h"
#include "../core/CommandManager.h"
#include "../core/EngineError.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Engine;

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace
{
    std::filesystem::path WritePrefabFile(const char* filename, float posX)
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / filename;
        std::ofstream file(path);
        file << "{\n"
                "  \"version\": 1,\n"
                "  \"name\": \"TestPrefab\",\n"
                "  \"components\": {\n"
                "    \"TransformComponent\": {\"_version\": 1, \"position\": ["
             << posX << ", 0, 0], \"rotation\": [0,0,0], \"scale\": [1,1,1]},\n"
                "    \"RenderableComponent\": {\"_version\": 1, \"meshHandle\": 3, \"materialHandle\": 0, \"castShadows\": true}\n"
                "  }\n"
                "}\n";
        return path;
    }

    // Entity that has already "diverged" from the prefab (moved away + an extra
    // AIComponent the prefab doesn't have) and carries a PrefabInstanceComponent
    // pointing at `path`.
    Entity MakeDivergedInstance(ECSRegistry& registry, const std::filesystem::path& path)
    {
        Entity e = registry.CreateEntity();

        TransformComponent t;
        t.position = Vec3(999.0f, 0.0f, 0.0f); // deliberately different from the prefab's captured position
        registry.AddComponent(e, t);

        RenderableComponent r;
        r.meshHandle = 3;
        registry.AddComponent(e, r);

        registry.AddComponent(e, AIComponent{}); // not present in the prefab -> Revert should remove it

        PrefabInstanceComponent meta;
        meta.prefabPath = path.string();
        meta.sourcePrefabVersion = 1;
        registry.AddComponent(e, meta);

        return e;
    }
}

// ─── Test Fixture ─────────────────────────────────────────────────────────────

class RevertPrefabInstanceCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterPODComponentsReflection();
    }

    ECSRegistry registry;
};

// ─── Definition A on Revert (plan §2.5 regression) ──────────────────────────────

TEST_F(RevertPrefabInstanceCommandTest, Apply_RemovesComponentNotInPrefab_KeepsPrefabInstanceComponent)
{
    auto path = WritePrefabFile("revert_cmd_test_1.prefab.json", 0.0f);
    Entity e = MakeDivergedInstance(registry, path);

    RevertPrefabInstanceCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());

    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));
    EXPECT_TRUE(registry.HasComponent<PrefabInstanceComponent>(e));

    TransformComponent* t = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 0.0f); // snapped back to the prefab's captured position

    std::filesystem::remove(path);
}

// ─── §2.9: deleted source file ──────────────────────────────────────────────────

TEST_F(RevertPrefabInstanceCommandTest, Apply_SourceFileDeleted_LeavesEntityUnchangedAndReturnsError)
{
    auto path = WritePrefabFile("revert_cmd_test_2.prefab.json", 0.0f);
    Entity e = MakeDivergedInstance(registry, path);
    std::filesystem::remove(path); // gone before Revert is even attempted

    RevertPrefabInstanceCommand cmd(&registry, e);
    auto result = cmd.Apply();
    EXPECT_FALSE(result.has_value());

    // Untouched: still has the diverged position and the extra AIComponent.
    EXPECT_TRUE(registry.HasComponent<AIComponent>(e));
    TransformComponent* t = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 999.0f);
}

// ─── §2.3: version mismatch does not gate Revert ────────────────────────────────

TEST_F(RevertPrefabInstanceCommandTest, Apply_VersionMismatch_StillAppliesLatestFileContent)
{
    auto path = WritePrefabFile("revert_cmd_test_3.prefab.json", 5.0f);
    Entity e = MakeDivergedInstance(registry, path);
    PrefabInstanceComponent* meta = registry.GetComponent<PrefabInstanceComponent>(e);
    ASSERT_NE(meta, nullptr);
    meta->sourcePrefabVersion = 999; // deliberately mismatched vs. the file's "version": 1

    RevertPrefabInstanceCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());

    TransformComponent* t = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 5.0f);

    std::filesystem::remove(path);
}

// ─── Undo / Redo ────────────────────────────────────────────────────────────────

TEST_F(RevertPrefabInstanceCommandTest, ApplyThenUndo_RestoresPreRevertState)
{
    auto path = WritePrefabFile("revert_cmd_test_4.prefab.json", 0.0f);
    Entity e = MakeDivergedInstance(registry, path);

    RevertPrefabInstanceCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));

    ASSERT_TRUE(cmd.Undo().has_value());

    EXPECT_TRUE(registry.HasComponent<AIComponent>(e));
    TransformComponent* t = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 999.0f);

    std::filesystem::remove(path);
}

TEST_F(RevertPrefabInstanceCommandTest, UndoThenRedo_AppliesRevertAgain_EvenIfFileDeletedMeanwhile)
{
    auto path = WritePrefabFile("revert_cmd_test_5.prefab.json", 0.0f);
    Entity e = MakeDivergedInstance(registry, path);

    RevertPrefabInstanceCommand cmd(&registry, e);
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(cmd.Undo().has_value());

    std::filesystem::remove(path); // simulate the file disappearing between Undo and Redo

    ASSERT_TRUE(cmd.Apply().has_value()); // Redo — must use the PrefabAsset cached on first Apply
    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));
    TransformComponent* t = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 0.0f);
}

TEST_F(RevertPrefabInstanceCommandTest, CommandManager_ExecuteUndoRedo_StackStateCorrect)
{
    auto path = WritePrefabFile("revert_cmd_test_cm.prefab.json", 0.0f);
    Entity e = MakeDivergedInstance(registry, path);

    auto& cm = CommandManager::GetInstance();
    cm.Clear();

    ASSERT_TRUE(cm.Execute(std::make_unique<RevertPrefabInstanceCommand>(&registry, e)).has_value());
    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));
    EXPECT_TRUE(cm.CanUndo());
    EXPECT_FALSE(cm.CanRedo());

    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_TRUE(registry.HasComponent<AIComponent>(e));
    EXPECT_TRUE(cm.CanRedo());

    ASSERT_TRUE(cm.Redo().has_value());
    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));

    cm.Clear();
    std::filesystem::remove(path);
}

// ─── Error paths ─────────────────────────────────────────────────────────────

TEST_F(RevertPrefabInstanceCommandTest, Apply_NotAPrefabInstance_ReturnsComponentNotFound)
{
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{}); // no PrefabInstanceComponent

    RevertPrefabInstanceCommand cmd(&registry, e);
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::ComponentNotFound);
}

TEST_F(RevertPrefabInstanceCommandTest, Apply_NullRegistry_ReturnsNotInitialized)
{
    RevertPrefabInstanceCommand cmd(nullptr, Entity(1));
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

TEST_F(RevertPrefabInstanceCommandTest, Undo_WithoutApply_ReturnsInvalidState)
{
    Entity e = registry.CreateEntity();
    RevertPrefabInstanceCommand cmd(&registry, e);

    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidState);
}
