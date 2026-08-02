// Tests for RotateEntityCommand and ScaleEntityCommand
// Validates: Requirements 2.2, 2.3, 2.4, 2.5, 8.3, 8.4, 9.1

#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../editor/commands/RotateEntityCommand.h"
#include "../editor/commands/ScaleEntityCommand.h"
#include "../core/EngineError.h"

using namespace Engine;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static Entity CreateEntityWithTransform(ECSRegistry& registry,
                                        Quaternion rotation = Quaternion(0, 0, 0, 1),
                                        Vec3 scale = Vec3(1, 1, 1))
{
    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.rotation = rotation;
    t.scale    = scale;
    registry.AddComponent(e, t);
    return e;
}

// ─── RotateEntityCommand ──────────────────────────────────────────────────────

// Req 2.2 – Apply stores new rotation; Undo restores the original
TEST(RotateEntityCommand, ApplyThenUndo_RestoresOriginalRotation)
{
    ECSRegistry registry;
    // SetTransformRotation stores Quaternion(x, y, z, 1.0f) internally
    // so we use values that survive that round-trip
    Quaternion initial(0.0f, 0.0f, 0.0f, 1.0f);
    Quaternion target (0.0f, 0.7071f, 0.0f, 1.0f);

    Entity e = CreateEntityWithTransform(registry, initial);

    RotateEntityCommand cmd(&registry, e, target);

    // Apply → rotation becomes target (w stored as 1.0f by SetTransformRotation)
    ASSERT_TRUE(cmd.Apply().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->rotation.x, target.x);
        EXPECT_FLOAT_EQ(t->rotation.y, target.y);
        EXPECT_FLOAT_EQ(t->rotation.z, target.z);
    }

    // Undo → rotation back to initial
    ASSERT_TRUE(cmd.Undo().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->rotation.x, initial.x);
        EXPECT_FLOAT_EQ(t->rotation.y, initial.y);
        EXPECT_FLOAT_EQ(t->rotation.z, initial.z);
    }
}

// Req 2.4 / 8.3 – Merge session: oldRotation_ preserves the very first value
TEST(RotateEntityCommand, MergeSession_OldRotationRemainsFirstValue)
{
    ECSRegistry registry;
    // Use w=1.0f for all since SetTransformRotation always writes w=1.0f
    Quaternion initial(0.0f, 0.0f, 0.0f, 1.0f);
    Quaternion mid    (0.0f, 0.5f, 0.0f, 1.0f);
    Quaternion final_ (0.0f, 1.0f, 0.0f, 1.0f);

    Entity e = CreateEntityWithTransform(registry, initial);

    RotateEntityCommand cmd1(&registry, e, mid);
    ASSERT_TRUE(cmd1.Apply().has_value());

    // Second command created after first applied — captures mid as old
    RotateEntityCommand cmd2(&registry, e, final_);

    // Merge cmd2 into cmd1
    ASSERT_TRUE(cmd1.CanMergeWith(cmd2));
    ASSERT_TRUE(cmd1.MergeWith(cmd2).has_value());

    // After merge: Apply should set final_, Undo should restore initial
    ASSERT_TRUE(cmd1.Apply().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->rotation.y, final_.y);
    }

    ASSERT_TRUE(cmd1.Undo().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        // Must restore the FIRST value, not mid
        EXPECT_FLOAT_EQ(t->rotation.x, initial.x);
        EXPECT_FLOAT_EQ(t->rotation.y, initial.y);
        EXPECT_FLOAT_EQ(t->rotation.z, initial.z);
    }
}
TEST(RotateEntityCommand, Apply_StaleEntity_ReturnsEntityNotFound)
{
    ECSRegistry registry;
    Quaternion target(0.0f, 1.0f, 0.0f, 0.0f);

    Entity e = CreateEntityWithTransform(registry);
    RotateEntityCommand cmd(&registry, e, target);

    // Destroy the entity → handle becomes stale
    registry.DestroyEntity(e);

    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// CanMergeWith returns false for a different entity
TEST(RotateEntityCommand, CanMergeWith_DifferentEntity_ReturnsFalse)
{
    ECSRegistry registry;
    Entity e1 = CreateEntityWithTransform(registry);
    Entity e2 = CreateEntityWithTransform(registry);

    RotateEntityCommand cmd1(&registry, e1, Quaternion(0,0,0,1));
    RotateEntityCommand cmd2(&registry, e2, Quaternion(0,1,0,0));

    EXPECT_FALSE(cmd1.CanMergeWith(cmd2));
}

// CanMergeWith returns true for the same entity
TEST(RotateEntityCommand, CanMergeWith_SameEntity_ReturnsTrue)
{
    ECSRegistry registry;
    Entity e = CreateEntityWithTransform(registry);

    RotateEntityCommand cmd1(&registry, e, Quaternion(0,0,0,1));
    RotateEntityCommand cmd2(&registry, e, Quaternion(0,1,0,0));

    EXPECT_TRUE(cmd1.CanMergeWith(cmd2));
}

// ─── ScaleEntityCommand ───────────────────────────────────────────────────────

// Req 2.3 – Apply stores new scale; Undo restores the original
TEST(ScaleEntityCommand, ApplyThenUndo_RestoresOriginalScale)
{
    ECSRegistry registry;
    Vec3 initial(1.0f, 1.0f, 1.0f);
    Vec3 target (2.5f, 3.0f, 0.5f);

    Entity e = CreateEntityWithTransform(registry, Quaternion(0,0,0,1), initial);

    ScaleEntityCommand cmd(&registry, e, target);

    // Apply → scale becomes target
    ASSERT_TRUE(cmd.Apply().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->scale.x, target.x);
        EXPECT_FLOAT_EQ(t->scale.y, target.y);
        EXPECT_FLOAT_EQ(t->scale.z, target.z);
    }

    // Undo → scale back to initial
    ASSERT_TRUE(cmd.Undo().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->scale.x, initial.x);
        EXPECT_FLOAT_EQ(t->scale.y, initial.y);
        EXPECT_FLOAT_EQ(t->scale.z, initial.z);
    }
}

// Req 2.4 / 8.3 – Merge session: oldScale_ preserves the very first value
TEST(ScaleEntityCommand, MergeSession_OldScaleRemainsFirstValue)
{
    ECSRegistry registry;
    Vec3 initial(1.0f, 1.0f, 1.0f);
    Vec3 mid    (2.0f, 2.0f, 2.0f);
    Vec3 final_ (5.0f, 5.0f, 5.0f);

    Entity e = CreateEntityWithTransform(registry, Quaternion(0,0,0,1), initial);

    ScaleEntityCommand cmd1(&registry, e, mid);
    ASSERT_TRUE(cmd1.Apply().has_value());

    // Second command created after first applied — captures mid as old
    ScaleEntityCommand cmd2(&registry, e, final_);

    // Merge cmd2 into cmd1
    ASSERT_TRUE(cmd1.CanMergeWith(cmd2));
    ASSERT_TRUE(cmd1.MergeWith(cmd2).has_value());

    // After merge: Apply should set final_, Undo should restore initial
    ASSERT_TRUE(cmd1.Apply().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        EXPECT_FLOAT_EQ(t->scale.x, final_.x);
    }

    ASSERT_TRUE(cmd1.Undo().has_value());
    {
        const TransformComponent* t = registry.GetTransformComponent(e);
        ASSERT_NE(t, nullptr);
        // Must restore the FIRST value, not mid
        EXPECT_FLOAT_EQ(t->scale.x, initial.x);
        EXPECT_FLOAT_EQ(t->scale.y, initial.y);
        EXPECT_FLOAT_EQ(t->scale.z, initial.z);
    }
}

// Req 2.5 – Apply on a destroyed (stale) entity returns EntityNotFound
TEST(ScaleEntityCommand, Apply_StaleEntity_ReturnsEntityNotFound)
{
    ECSRegistry registry;
    Vec3 target(2.0f, 2.0f, 2.0f);

    Entity e = CreateEntityWithTransform(registry);
    ScaleEntityCommand cmd(&registry, e, target);

    // Destroy the entity → handle becomes stale
    registry.DestroyEntity(e);

    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::EntityNotFound);
}

// CanMergeWith returns false for a different entity
TEST(ScaleEntityCommand, CanMergeWith_DifferentEntity_ReturnsFalse)
{
    ECSRegistry registry;
    Entity e1 = CreateEntityWithTransform(registry);
    Entity e2 = CreateEntityWithTransform(registry);

    ScaleEntityCommand cmd1(&registry, e1, Vec3(1,1,1));
    ScaleEntityCommand cmd2(&registry, e2, Vec3(2,2,2));

    EXPECT_FALSE(cmd1.CanMergeWith(cmd2));
}

// CanMergeWith returns true for the same entity
TEST(ScaleEntityCommand, CanMergeWith_SameEntity_ReturnsTrue)
{
    ECSRegistry registry;
    Entity e = CreateEntityWithTransform(registry);

    ScaleEntityCommand cmd1(&registry, e, Vec3(1,1,1));
    ScaleEntityCommand cmd2(&registry, e, Vec3(2,2,2));

    EXPECT_TRUE(cmd1.CanMergeWith(cmd2));
}

// ─── Cross-type merge rejection ───────────────────────────────────────────────

// RotateEntityCommand must not merge with ScaleEntityCommand (different type)
TEST(RotateEntityCommand, CanMergeWith_DifferentCommandType_ReturnsFalse)
{
    ECSRegistry registry;
    Entity e = CreateEntityWithTransform(registry);

    RotateEntityCommand rotCmd(&registry, e, Quaternion(0,0,0,1));
    ScaleEntityCommand  scaleCmd(&registry, e, Vec3(2,2,2));

    EXPECT_FALSE(rotCmd.CanMergeWith(scaleCmd));
}

// ScaleEntityCommand must not merge with RotateEntityCommand (different type)
TEST(ScaleEntityCommand, CanMergeWith_DifferentCommandType_ReturnsFalse)
{
    ECSRegistry registry;
    Entity e = CreateEntityWithTransform(registry);

    ScaleEntityCommand  scaleCmd(&registry, e, Vec3(2,2,2));
    RotateEntityCommand rotCmd(&registry, e, Quaternion(0,0,0,1));

    EXPECT_FALSE(scaleCmd.CanMergeWith(rotCmd));
}

// ─── Null registry guard ──────────────────────────────────────────────────────

TEST(RotateEntityCommand, Apply_NullRegistry_ReturnsNotInitialized)
{
    Entity e(1);  // non-zero id to pass IsValid()

    RotateEntityCommand cmd(nullptr, e, Quaternion(0,0,0,1));
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}

TEST(ScaleEntityCommand, Apply_NullRegistry_ReturnsNotInitialized)
{
    Entity e(1);  // non-zero id to pass IsValid()

    ScaleEntityCommand cmd(nullptr, e, Vec3(1,1,1));
    auto result = cmd.Apply();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotInitialized);
}
