// SetComponentFieldCommand — Inspector의 일반 필드 편집을 명령으로 만든다(docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F1).
// 예전에는 이 편집이 레지스트리에 직접 쓰여서 Undo·"저장 안 된 변경"에 잡히지 않았다.
#include <gtest/gtest.h>
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../ecs/RotationConversion.h"
#include "../editor/commands/SetComponentFieldCommand.h"
#include "../core/CommandManager.h"
#include <nlohmann/json.hpp>

using namespace Engine;
using json = nlohmann::json;

namespace
{
    class SetComponentFieldTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            if (!ComponentRegistry::GetComponentInfo("TransformComponent"))
            {
                RegisterPODComponentsReflection();
            }
            CommandManager::GetInstance().Clear();
            e_ = registry_.CreateEntity();
            registry_.AddComponent(e_, TransformComponent{});
        }
        void TearDown() override { CommandManager::GetInstance().Clear(); }

        uint32_t Version(const char* comp) const { return ComponentRegistry::GetComponentInfo(comp)->version; }
        Vec3 Euler() { return EulerDegreesFromQuaternion(registry_.GetComponent<TransformComponent>(e_)->rotation); }

        ECSRegistry registry_;
        Entity e_;
    };
}

TEST_F(SetComponentFieldTest, Rotation_ApplyUndoRedo_UsesEulerDegrees)
{
    SetComponentFieldCommand cmd(&registry_, e_, "TransformComponent", "rotation", json::array({0.0, 30.0, 0.0}),
                                 Version("TransformComponent"));
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_NEAR(Euler().y, 30.0f, 1e-3f);
    EXPECT_NEAR(Euler().z, 0.0f, 1e-3f);
    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_NEAR(Euler().y, 0.0f, 1e-3f);
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_NEAR(Euler().y, 30.0f, 1e-3f);
}

TEST_F(SetComponentFieldTest, Scale_And_String_RoundTrip)
{
    SetComponentFieldCommand scale(&registry_, e_, "TransformComponent", "scale", json::array({3.0, 1.0, 1.0}),
                                   Version("TransformComponent"));
    ASSERT_TRUE(scale.Apply().has_value());
    EXPECT_FLOAT_EQ(registry_.GetComponent<TransformComponent>(e_)->scale.x, 3.0f);
    ASSERT_TRUE(scale.Undo().has_value());
    EXPECT_FLOAT_EQ(registry_.GetComponent<TransformComponent>(e_)->scale.x, 1.0f);

    registry_.AddComponent(e_, RenderableComponent{});
    registry_.GetComponent<RenderableComponent>(e_)->meshPath = "assets/models/cube.obj";
    SetComponentFieldCommand mesh(&registry_, e_, "RenderableComponent", "meshPath", json("assets/models/한글 메시.obj"),
                                  Version("RenderableComponent"));
    ASSERT_TRUE(mesh.Apply().has_value());
    EXPECT_EQ(registry_.GetComponent<RenderableComponent>(e_)->meshPath, "assets/models/한글 메시.obj");
    ASSERT_TRUE(mesh.Undo().has_value());
    EXPECT_EQ(registry_.GetComponent<RenderableComponent>(e_)->meshPath, "assets/models/cube.obj");
}

TEST_F(SetComponentFieldTest, EntityRef_UsesUuidAndUndoClears)
{
    Entity other = registry_.CreateEntity();
    registry_.AddComponent(e_, HierarchyComponent{});
    const uint64_t uuid = registry_.GetUUID(other).GetValue();
    SetComponentFieldCommand cmd(&registry_, e_, "HierarchyComponent", "parent", json(uuid), Version("HierarchyComponent"));
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_EQ(registry_.GetComponent<HierarchyComponent>(e_)->parent.id, other.id);
    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FALSE(registry_.GetComponent<HierarchyComponent>(e_)->parent.IsValid());
}

TEST_F(SetComponentFieldTest, Errors_LeaveValueUnchanged)
{
    const uint32_t v = Version("TransformComponent");
    struct Case { const char* comp; const char* field; json value; uint32_t version; };
    const Case cases[] = {
        {"TransformComponent", "nope", json::array({1, 2, 3}), v},            // 모르는 필드
        {"NoSuchComponent", "rotation", json::array({1, 2, 3}), v},           // 모르는 컴포넌트
        {"TransformComponent", "rotation", json::array({0, 30, 0}), v + 1},   // 스키마 버전 불일치
        {"TransformComponent", "scale", json("abc"), v},                      // 형식 오류
        {"RenderableComponent", "meshPath", json("x.obj"), Version("RenderableComponent")},  // 컴포넌트 없음
    };
    for (const auto& c : cases)
    {
        SetComponentFieldCommand cmd(&registry_, e_, c.comp, c.field, c.value, c.version);
        EXPECT_FALSE(cmd.Apply().has_value()) << c.comp << "." << c.field;
        const TransformComponent* t = registry_.GetComponent<TransformComponent>(e_);
        EXPECT_FLOAT_EQ(t->scale.x, 1.0f) << c.comp << "." << c.field;
        EXPECT_NEAR(Euler().y, 0.0f, 1e-3f) << c.comp << "." << c.field;
    }
}

// Inspector 입력칸에 포커스가 있는 동안(병합 세션) 키 입력마다 생기는 편집은 Undo 한 번으로 돌아가야 하고,
// 편집할 때마다 "저장 안 된 변경" 카운터가 올라야 한다(예전 증상의 핵심).
TEST_F(SetComponentFieldTest, MergeSession_OneUndo_AndChangeCountRises)
{
    auto& cm = CommandManager::GetInstance();
    const auto before = cm.GetChangeCount();
    cm.BeginMergeSession();
    for (double y : {3.0, 30.0, 45.0})
    {
        ASSERT_TRUE(cm.Execute(std::make_unique<SetComponentFieldCommand>(
            &registry_, e_, "TransformComponent", "rotation", json::array({0.0, y, 0.0}),
            Version("TransformComponent"))).has_value());
    }
    cm.EndMergeSession();
    EXPECT_EQ(cm.GetChangeCount(), before + 3);
    EXPECT_NEAR(Euler().y, 45.0f, 1e-3f);
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_NEAR(Euler().y, 0.0f, 1e-3f);
    EXPECT_FALSE(cm.CanUndo());
    ASSERT_TRUE(cm.Redo().has_value());
    EXPECT_NEAR(Euler().y, 45.0f, 1e-3f);
}

// 다른 필드 편집은 병합되지 않는다(각각 Undo).
TEST_F(SetComponentFieldTest, DifferentFields_DoNotMerge)
{
    auto& cm = CommandManager::GetInstance();
    cm.BeginMergeSession();
    ASSERT_TRUE(cm.Execute(std::make_unique<SetComponentFieldCommand>(
        &registry_, e_, "TransformComponent", "rotation", json::array({0.0, 30.0, 0.0}), Version("TransformComponent"))).has_value());
    ASSERT_TRUE(cm.Execute(std::make_unique<SetComponentFieldCommand>(
        &registry_, e_, "TransformComponent", "scale", json::array({2.0, 2.0, 2.0}), Version("TransformComponent"))).has_value());
    cm.EndMergeSession();
    ASSERT_TRUE(cm.Undo().has_value());
    EXPECT_FLOAT_EQ(registry_.GetComponent<TransformComponent>(e_)->scale.x, 1.0f);
    EXPECT_NEAR(Euler().y, 30.0f, 1e-3f);
}

TEST_F(SetComponentFieldTest, FieldEdit_FollowsUuidAcrossRecreation)
{
    const UUID uuid = registry_.GetUUID(e_);
    SetComponentFieldCommand cmd(&registry_, e_, "TransformComponent", "scale", json::array({3, 2, 1}),
                                 Version("TransformComponent"));
    ASSERT_TRUE(cmd.Apply().has_value());
    ASSERT_TRUE(cmd.Undo().has_value());
    registry_.DestroyEntity(e_);
    EXPECT_FALSE(cmd.Apply().has_value());
    Entity restored = registry_.CreateEntityWithUUID(uuid);
    ASSERT_NE(restored.id, e_.id);
    registry_.AddComponent(restored, TransformComponent{});
    ASSERT_TRUE(cmd.Apply().has_value());
    EXPECT_FLOAT_EQ(registry_.GetComponent<TransformComponent>(restored)->scale.x, 3.0f);
    ASSERT_TRUE(cmd.Undo().has_value());
    EXPECT_FLOAT_EQ(registry_.GetComponent<TransformComponent>(restored)->scale.x, 1.0f);
    SetComponentFieldCommand later(&registry_, restored, "TransformComponent", "scale", json::array({4, 2, 1}),
                                   Version("TransformComponent"));
    EXPECT_TRUE(cmd.CanMergeWith(later));
    ECSRegistry otherRegistry;
    Entity other = otherRegistry.CreateEntityWithUUID(uuid);
    otherRegistry.AddComponent(other, TransformComponent{});
    SetComponentFieldCommand unrelated(&otherRegistry, other, "TransformComponent", "scale", json::array({4, 2, 1}),
                                       Version("TransformComponent"));
    EXPECT_FALSE(cmd.CanMergeWith(unrelated));
}
