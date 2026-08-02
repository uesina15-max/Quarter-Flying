// Tests for AddComponentCommand and RemoveComponentCommand
// Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 9.1

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../editor/commands/AddComponentCommand.h"
#include "../editor/commands/RemoveComponentCommand.h"
#include "../core/EngineError.h"

using namespace Engine;

// ─── Fixture ─────────────────────────────────────────────────────────────────

class ComponentCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterPODComponentsReflection();
        entity_ = registry_.CreateEntity();
    }

    ECSRegistry registry_;
    Entity      entity_;
};

// ─── AddComponentCommand ─────────────────────────────────────────────────────

// Req 4.1 – Apply() adds a component with default values
TEST_F(ComponentCommandTest, AddComponent_Apply_AddsComponentWithDefaults)
{
    AddComponentCommand cmd(&registry_, entity_, "TransformComponent");
    ASSERT_TRUE(cmd.Apply().has_value());

    ASSERT_TRUE(registry_.HasComponent<TransformComponent>(entity_));
    const TransformComponent* tc = registry_.GetComponent<TransformComponent>(entity_);
    ASSERT_NE(tc, nullptr);
    // Default constructor sets position to (0,0,0)
    EXPECT_FLOAT_EQ(tc->position.x, 0.0f);
    EXPECT_FLOAT_EQ(tc->position.y, 0.0f);
    EXPECT_FLOAT_EQ(tc->position.z, 0.0f);
    // Default scale is (1,1,1)
    EXPECT_FLOAT_EQ(tc->scale.x, 1.0f);
    EXPECT_FLOAT_EQ(tc->scale.y, 1.0f);
    EXPECT_FLOAT_EQ(tc->scale.z, 1.0f);
}

// Req 4.1 – RenderableComponent can also be added
TEST_F(ComponentCommandTest, AddComponent_Apply_AddsRenderableComponent)
{
    AddComponentCommand cmd(&registry_, entity_, "RenderableComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<RenderableComponent>(entity_));
}

// Req 4.1 – CameraComponent can also be added
TEST_F(ComponentCommandTest, AddComponent_Apply_AddsCameraComponent)
{
    AddComponentCommand cmd(&registry_, entity_, "CameraComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<CameraComponent>(entity_));
}

// Req 4.1 – AIComponent can also be added
TEST_F(ComponentCommandTest, AddComponent_Apply_AddsAIComponent)
{
    AddComponentCommand cmd(&registry_, entity_, "AIComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<AIComponent>(entity_));
}

// Req 4.2 – Undo() removes the component that was just added
TEST_F(ComponentCommandTest, AddComponent_ApplyThenUndo_RemovesComponent)
{
    AddComponentCommand cmd(&registry_, entity_, "TransformComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(registry_.HasComponent<TransformComponent>(entity_));

    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));
}

// Req 4.5 – Apply() on entity that already has the component → DuplicateComponent
TEST_F(ComponentCommandTest, AddComponent_Apply_DuplicateComponent_ReturnsDuplicateComponent)
{
    // Add first time
    AddComponentCommand cmd1(&registry_, entity_, "TransformComponent");
    ASSERT_TRUE(cmd1.Apply().has_value());

    // Attempt to add again
    AddComponentCommand cmd2(&registry_, entity_, "TransformComponent");
    auto result = cmd2.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::DuplicateComponent);
}

// Req 4.6 – Unknown component type → InvalidComponentType
TEST_F(ComponentCommandTest, AddComponent_Apply_UnknownType_ReturnsInvalidComponentType)
{
    AddComponentCommand cmd(&registry_, entity_, "NonExistentComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidComponentType);
}

// Req 9.1 – Null registry → NotInitialized
TEST_F(ComponentCommandTest, AddComponent_NullRegistry_Apply_ReturnsNotInitialized)
{
    AddComponentCommand cmd(nullptr, entity_, "TransformComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Req 9.1 – Null registry Undo → NotInitialized
TEST_F(ComponentCommandTest, AddComponent_NullRegistry_Undo_ReturnsNotInitialized)
{
    AddComponentCommand cmd(nullptr, entity_, "TransformComponent");
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Req 4.1 – Apply on invalid entity → EntityNotFound
TEST_F(ComponentCommandTest, AddComponent_InvalidEntity_ReturnsEntityNotFound)
{
    registry_.DestroyEntity(entity_);
    AddComponentCommand cmd(&registry_, entity_, "TransformComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// Undo without prior Apply is a no-op (component was never added → not present → success)
TEST_F(ComponentCommandTest, AddComponent_UndoWithoutApply_IsNoOp)
{
    AddComponentCommand cmd(&registry_, entity_, "TransformComponent");
    // Component not present, Undo should succeed (no-op)
    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));
}

// Apply → Undo → Apply (Redo) round-trip
TEST_F(ComponentCommandTest, AddComponent_ApplyUndoApply_RoundTrip)
{
    AddComponentCommand cmd(&registry_, entity_, "TransformComponent");

    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<TransformComponent>(entity_));

    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));

    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<TransformComponent>(entity_));
}

// ─── RemoveComponentCommand ──────────────────────────────────────────────────

// Req 4.3 – Apply() removes the component
TEST_F(ComponentCommandTest, RemoveComponent_Apply_RemovesComponent)
{
    TransformComponent tc;
    tc.position = Vec3(1.0f, 2.0f, 3.0f);
    registry_.AddComponent(entity_, tc);

    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));
}

// Req 4.4 – Undo() restores the component with all field values intact
TEST_F(ComponentCommandTest, RemoveComponent_ApplyThenUndo_RestoresAllFields)
{
    TransformComponent tc;
    tc.position = Vec3(10.0f, 20.0f, 30.0f);
    tc.scale    = Vec3(2.0f, 3.0f, 4.0f);
    registry_.AddComponent(entity_, tc);

    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));

    ASSERT_TRUE(cmd.Undo().has_value());
    ASSERT_TRUE(registry_.HasComponent<TransformComponent>(entity_));

    const TransformComponent* restored = registry_.GetComponent<TransformComponent>(entity_);
    ASSERT_NE(restored, nullptr);
    EXPECT_FLOAT_EQ(restored->position.x, 10.0f);
    EXPECT_FLOAT_EQ(restored->position.y, 20.0f);
    EXPECT_FLOAT_EQ(restored->position.z, 30.0f);
    EXPECT_FLOAT_EQ(restored->scale.x, 2.0f);
    EXPECT_FLOAT_EQ(restored->scale.y, 3.0f);
    EXPECT_FLOAT_EQ(restored->scale.z, 4.0f);
}

// Req 4.4 – Undo restores RenderableComponent fields
TEST_F(ComponentCommandTest, RemoveComponent_ApplyThenUndo_RestoresRenderableFields)
{
    RenderableComponent rc;
    rc.meshHandle     = 42;
    rc.materialHandle = 99;
    rc.castShadows    = false;
    registry_.AddComponent(entity_, rc);

    RemoveComponentCommand cmd(&registry_, entity_, "RenderableComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(cmd.Undo().has_value());

    const RenderableComponent* restored = registry_.GetComponent<RenderableComponent>(entity_);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->meshHandle, 42u);
    EXPECT_EQ(restored->materialHandle, 99u);
    EXPECT_FALSE(restored->castShadows);
}

// Req 4.4 – Undo restores CameraComponent fields
TEST_F(ComponentCommandTest, RemoveComponent_ApplyThenUndo_RestoresCameraFields)
{
    CameraComponent cam;
    cam.fov          = 90.0f;
    cam.nearPlane    = 0.5f;
    cam.farPlane     = 500.0f;
    cam.isMainCamera = true;
    registry_.AddComponent(entity_, cam);

    RemoveComponentCommand cmd(&registry_, entity_, "CameraComponent");
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(cmd.Undo().has_value());

    const CameraComponent* restored = registry_.GetComponent<CameraComponent>(entity_);
    ASSERT_NE(restored, nullptr);
    EXPECT_FLOAT_EQ(restored->fov, 90.0f);
    EXPECT_FLOAT_EQ(restored->nearPlane, 0.5f);
    EXPECT_FLOAT_EQ(restored->farPlane, 500.0f);
    EXPECT_TRUE(restored->isMainCamera);
}

// Component not found → ComponentNotFound
TEST_F(ComponentCommandTest, RemoveComponent_Apply_ComponentNotPresent_ReturnsComponentNotFound)
{
    // entity_ has no TransformComponent
    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::ComponentNotFound);
}

// Unknown component type → InvalidComponentType
TEST_F(ComponentCommandTest, RemoveComponent_Apply_UnknownType_ReturnsInvalidComponentType)
{
    RemoveComponentCommand cmd(&registry_, entity_, "NonExistentComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidComponentType);
}

// Req 9.1 – Null registry Apply → NotInitialized
TEST_F(ComponentCommandTest, RemoveComponent_NullRegistry_Apply_ReturnsNotInitialized)
{
    RemoveComponentCommand cmd(nullptr, entity_, "TransformComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Req 9.1 – Null registry Undo → NotInitialized
TEST_F(ComponentCommandTest, RemoveComponent_NullRegistry_Undo_ReturnsNotInitialized)
{
    RemoveComponentCommand cmd(nullptr, entity_, "TransformComponent");
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

// Undo before Apply → InvalidState (no snapshot)
TEST_F(ComponentCommandTest, RemoveComponent_UndoWithoutApply_ReturnsInvalidState)
{
    TransformComponent tc;
    registry_.AddComponent(entity_, tc);

    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");
    auto result = cmd.Undo();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidState);
}

// Apply on invalid entity → EntityNotFound
TEST_F(ComponentCommandTest, RemoveComponent_InvalidEntity_ReturnsEntityNotFound)
{
    registry_.DestroyEntity(entity_);
    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// Apply → Undo → Apply (Redo) round-trip: component absent after Redo
TEST_F(ComponentCommandTest, RemoveComponent_ApplyUndoApply_RoundTrip)
{
    TransformComponent tc;
    tc.position = Vec3(7.0f, 8.0f, 9.0f);
    registry_.AddComponent(entity_, tc);

    RemoveComponentCommand cmd(&registry_, entity_, "TransformComponent");

    // Apply: remove
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));

    // Undo: restore
    ASSERT_TRUE(cmd.Undo().has_value());
    ASSERT_TRUE(registry_.HasComponent<TransformComponent>(entity_));
    {
        const TransformComponent* r = registry_.GetComponent<TransformComponent>(entity_);
        EXPECT_FLOAT_EQ(r->position.x, 7.0f);
        EXPECT_FLOAT_EQ(r->position.y, 8.0f);
        EXPECT_FLOAT_EQ(r->position.z, 9.0f);
    }

    // Redo (Apply again): remove
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FALSE(registry_.HasComponent<TransformComponent>(entity_));
}

// ─── Add + Remove interaction ────────────────────────────────────────────────

// AddComponentCommand → RemoveComponentCommand → Undo(Remove) round-trip
TEST_F(ComponentCommandTest, AddThenRemove_UndoChain_RestoresComponent)
{
    // 1. Add via command
    AddComponentCommand addCmd(&registry_, entity_, "CameraComponent");
    ASSERT_TRUE(addCmd.Apply().has_value());
    EXPECT_TRUE(registry_.HasComponent<CameraComponent>(entity_));

    // Tweak the value directly to something non-default
    CameraComponent* cam = registry_.GetComponent<CameraComponent>(entity_);
    cam->fov = 75.0f;

    // 2. Remove via command (captures snapshot of fov=75)
    RemoveComponentCommand removeCmd(&registry_, entity_, "CameraComponent");
    ASSERT_TRUE(removeCmd.Apply().has_value());
    EXPECT_FALSE(registry_.HasComponent<CameraComponent>(entity_));

    // 3. Undo remove → component restored with fov=75
    ASSERT_TRUE(removeCmd.Undo().has_value());
    ASSERT_TRUE(registry_.HasComponent<CameraComponent>(entity_));
    EXPECT_FLOAT_EQ(registry_.GetComponent<CameraComponent>(entity_)->fov, 75.0f);

    // 4. Undo add → component gone
    ASSERT_TRUE(addCmd.Undo().has_value());
    EXPECT_FALSE(registry_.HasComponent<CameraComponent>(entity_));
}
