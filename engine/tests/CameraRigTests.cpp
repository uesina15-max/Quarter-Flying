// 카메라 리그(C5 추적/주시, C6 플레이어 궤도) + InputState 유닛 테스트.
// docs/INGAME_CAMERA_PLAN.md C5, C6.

#include <gtest/gtest.h>
#include "../ecs/CameraRigSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../input/InputState.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Engine;

namespace
{
    Entity AddAt(ECSRegistry& registry, float x, float y, float z)
    {
        Entity e = registry.CreateEntity();
        TransformComponent t;
        t.position = Vec3(x, y, z);
        registry.AddComponent(e, t);
        return e;
    }

    glm::vec3 Forward(const TransformComponent& t)
    {
        glm::quat q(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
        return q * glm::vec3(0.0f, 0.0f, -1.0f);
    }

    bool Near(const Vec3& a, const glm::vec3& b, float eps = 1e-4f)
    {
        return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
    }

    InputEvent Ev(InputEventType type)
    {
        InputEvent e;
        e.type = type;
        return e;
    }
}

// ── InputState ───────────────────────────────────────────────────────────────

TEST(InputStateTest, MouseDelta_AccumulatesFromAbsolutePositions_AndResetsEachFrame)
{
    InputState input;
    InputEvent move = Ev(InputEventType::MouseMove);
    move.mouseX = 100; move.mouseY = 50;
    input.BeginFrame();
    input.Apply(move);                       // 첫 좌표는 기준점일 뿐 이동량 0
    move.mouseX = 110; move.mouseY = 45;
    input.Apply(move);
    move.mouseX = 115; move.mouseY = 45;
    input.Apply(move);
    EXPECT_FLOAT_EQ(input.GetMouseDeltaX(), 15.0f);
    EXPECT_FLOAT_EQ(input.GetMouseDeltaY(), -5.0f);

    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetMouseDeltaX(), 0.0f);
}

TEST(InputStateTest, ButtonsAndKeys_StayDownUntilUp)
{
    InputState input;
    InputEvent down = Ev(InputEventType::MouseButtonDown);
    down.mouseButton = MouseButton::Right;
    input.Apply(down);
    input.BeginFrame();
    EXPECT_TRUE(input.IsMouseButtonDown(MouseButton::Right));
    InputEvent up = Ev(InputEventType::MouseButtonUp);
    up.mouseButton = MouseButton::Right;
    input.Apply(up);
    EXPECT_FALSE(input.IsMouseButtonDown(MouseButton::Right));

    InputEvent key = Ev(InputEventType::KeyDown);
    key.keyCode = KeyCode::W;
    input.Apply(key);
    EXPECT_TRUE(input.IsKeyDown(KeyCode::W));
    EXPECT_FALSE(input.IsKeyDown(KeyCode::Unknown));
}

TEST(InputStateTest, Wheel_AccumulatesPerFrame)
{
    InputState input;
    InputEvent wheel = Ev(InputEventType::MouseWheel);
    wheel.mouseWheelDelta = 1.0f;
    input.Apply(wheel);
    input.Apply(wheel);
    EXPECT_FLOAT_EQ(input.GetWheelDelta(), 2.0f);
    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetWheelDelta(), 0.0f);
}

// ── C5: Follow / LookAt ──────────────────────────────────────────────────────

TEST(CameraRigTest, Follow_WithZeroDamping_SnapsToTargetPlusOffset)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 5.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraFollowComponent fc;
    fc.target = target;
    fc.offset = Vec3(0.0f, 2.0f, 6.0f);
    fc.damping = 0.0f;
    registry.AddComponent(cam, fc);

    CameraRigSystem rig(nullptr);
    rig.Update(registry, 0.016f);
    EXPECT_TRUE(Near(registry.GetComponent<TransformComponent>(cam)->position, glm::vec3(5.0f, 2.0f, 6.0f)));
}

TEST(CameraRigTest, Follow_WithDamping_MovesPartWayFrameRateIndependently)
{
    // damping = 1초면 1초 뒤 남은 거리가 1/e. 0.5초 두 번이나 1초 한 번이나 같은 결과여야 한다.
    auto run = [](std::initializer_list<float> steps) {
        ECSRegistry registry;
        Entity target = AddAt(registry, 10.0f, 0.0f, 0.0f);
        Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
        CameraFollowComponent fc;
        fc.target = target;
        fc.offset = Vec3(0.0f, 0.0f, 0.0f);
        fc.damping = 1.0f;
        registry.AddComponent(cam, fc);
        CameraRigSystem rig(nullptr);
        for (float dt : steps) rig.Update(registry, dt);
        return registry.GetComponent<TransformComponent>(cam)->position.x;
    };
    const float expected = 10.0f * (1.0f - std::exp(-1.0f));
    EXPECT_NEAR(run({1.0f}), expected, 1e-4f);
    EXPECT_NEAR(run({0.5f, 0.5f}), expected, 1e-4f);
}

TEST(CameraRigTest, LookAt_PointsForwardAtTarget)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 10.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraLookAtComponent lc;
    lc.target = target;
    lc.targetOffset = Vec3(0.0f, 0.0f, 0.0f);
    registry.AddComponent(cam, lc);

    CameraRigSystem rig(nullptr);
    rig.Update(registry, 0.016f);
    glm::vec3 f = Forward(*registry.GetComponent<TransformComponent>(cam));
    EXPECT_NEAR(f.x, 1.0f, 1e-4f);
    EXPECT_NEAR(f.y, 0.0f, 1e-4f);
    EXPECT_NEAR(f.z, 0.0f, 1e-4f);
}

TEST(CameraRigTest, MissingTarget_LeavesTransformUnchanged)
{
    ECSRegistry registry;
    Entity cam = AddAt(registry, 1.0f, 2.0f, 3.0f);
    CameraFollowComponent fc;            // target 미지정(무효)
    fc.damping = 0.0f;
    registry.AddComponent(cam, fc);
    CameraRigSystem rig(nullptr);
    rig.Update(registry, 0.016f);
    EXPECT_TRUE(Near(registry.GetComponent<TransformComponent>(cam)->position, glm::vec3(1.0f, 2.0f, 3.0f)));
}

// ── C6: 플레이어 궤도 카메라 ─────────────────────────────────────────────────

TEST(CameraRigTest, Orbit_PlacesCameraAtDistance_LookingAtPivot)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 0.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraOrbitControlComponent oc;
    oc.target = target;
    oc.targetOffset = Vec3(0.0f, 0.0f, 0.0f);
    oc.distance = 10.0f;
    oc.yaw = 0.0f;
    oc.pitch = 0.0f;
    registry.AddComponent(cam, oc);

    CameraRigSystem rig(nullptr);
    rig.Update(registry, 0.016f);
    const TransformComponent& t = *registry.GetComponent<TransformComponent>(cam);
    EXPECT_TRUE(Near(t.position, glm::vec3(0.0f, 0.0f, 10.0f)));      // yaw 0 = +Z 쪽
    glm::vec3 f = Forward(t);
    EXPECT_NEAR(f.z, -1.0f, 1e-4f);
}

TEST(CameraRigTest, Orbit_RightDragRotates_OnlyWhileRightButtonHeld)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 0.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraOrbitControlComponent oc;
    oc.target = target;
    oc.yaw = 0.0f;
    oc.sensitivity = 0.5f;
    registry.AddComponent(cam, oc);

    InputState input;
    CameraRigSystem rig(&input);
    InputEvent move;
    move.type = InputEventType::MouseMove;

    // 버튼 없이 이동 -> 회전 안 함
    input.BeginFrame();
    move.mouseX = 0; input.Apply(move);
    move.mouseX = 20; input.Apply(move);
    rig.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(registry.GetComponent<CameraOrbitControlComponent>(cam)->yaw, 0.0f);

    // 우클릭 누른 채 이동 -> yaw -= 20px * 0.5
    InputEvent down;
    down.type = InputEventType::MouseButtonDown;
    down.mouseButton = MouseButton::Right;
    down.mouseX = 20;                        // 실제 누름 이벤트는 누른 위치를 담는다(드래그 기준점)
    input.BeginFrame();
    input.Apply(down);
    move.mouseX = 40; input.Apply(move);
    rig.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(registry.GetComponent<CameraOrbitControlComponent>(cam)->yaw, -10.0f);
}

TEST(CameraRigTest, Orbit_FastDragWithinOneFrame_StillRotates)
{
    // 누름 -> 이동 -> 뗌이 한 프레임 안에 모두 들어와도 회전해야 한다(실제 에디터 검증에서 무시되던 경우).
    ECSRegistry registry;
    Entity target = AddAt(registry, 0.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraOrbitControlComponent oc;
    oc.target = target;
    oc.yaw = 0.0f;
    oc.sensitivity = 1.0f;
    registry.AddComponent(cam, oc);

    InputState input;
    CameraRigSystem rig(&input);
    InputEvent move;  move.type = InputEventType::MouseMove;
    InputEvent down;  down.type = InputEventType::MouseButtonDown; down.mouseButton = MouseButton::Right;
    InputEvent up;    up.type = InputEventType::MouseButtonUp;     up.mouseButton = MouseButton::Right;

    input.BeginFrame();
    move.mouseX = 0; input.Apply(move);
    input.Apply(down);
    move.mouseX = 30; input.Apply(move);
    input.Apply(up);
    move.mouseX = 50; input.Apply(move);   // 뗀 뒤의 이동은 세지 않는다
    rig.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(registry.GetComponent<CameraOrbitControlComponent>(cam)->yaw, -30.0f);
}

TEST(CameraRigTest, Orbit_WheelZooms_WithinLimits)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 0.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraOrbitControlComponent oc;
    oc.target = target;
    oc.distance = 10.0f;
    oc.minDistance = 2.0f;
    oc.zoomPerStep = 0.5f;
    registry.AddComponent(cam, oc);

    InputState input;
    CameraRigSystem rig(&input);
    InputEvent wheel;
    wheel.type = InputEventType::MouseWheel;
    wheel.mouseWheelDelta = 1.0f;
    input.BeginFrame(); input.Apply(wheel);
    rig.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(registry.GetComponent<CameraOrbitControlComponent>(cam)->distance, 5.0f);

    wheel.mouseWheelDelta = 10.0f;       // 한계 아래로
    input.BeginFrame(); input.Apply(wheel);
    rig.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(registry.GetComponent<CameraOrbitControlComponent>(cam)->distance, 2.0f);
}

TEST(CameraRigTest, Orbit_OverridesFollowOnSameEntity)
{
    ECSRegistry registry;
    Entity target = AddAt(registry, 0.0f, 0.0f, 0.0f);
    Entity cam = AddAt(registry, 0.0f, 0.0f, 0.0f);
    CameraOrbitControlComponent oc;
    oc.target = target;
    oc.targetOffset = Vec3(0.0f, 0.0f, 0.0f);
    oc.distance = 10.0f; oc.yaw = 0.0f; oc.pitch = 0.0f;
    registry.AddComponent(cam, oc);
    CameraFollowComponent fc;
    fc.target = target;
    fc.offset = Vec3(100.0f, 0.0f, 0.0f);
    fc.damping = 0.0f;
    registry.AddComponent(cam, fc);

    CameraRigSystem rig(nullptr);
    rig.Update(registry, 0.016f);
    EXPECT_TRUE(Near(registry.GetComponent<TransformComponent>(cam)->position, glm::vec3(0.0f, 0.0f, 10.0f)));
}

TEST(CameraRigTest, DoesNotRunInEditMode)
{
    CameraRigSystem rig(nullptr);
    EXPECT_FALSE(rig.RunsInEditMode());
    EXPECT_LT(rig.GetPriority(), 0);     // CameraSystem(0)보다 먼저
}

TEST(InputStateTest, ButtonPressPosition_IsDragBaseline)
{
    // 마지막 MouseMove(예: 트래킹으로 들어온 먼 커서 위치)와 누른 위치 사이의 점프가 드래그에 섞이면 안 된다.
    InputState input;
    InputEvent move;  move.type = InputEventType::MouseMove;
    move.mouseX = 0; move.mouseY = 0; input.Apply(move);            // 먼 곳의 마지막 커서
    input.BeginFrame();
    InputEvent down;  down.type = InputEventType::MouseButtonDown; down.mouseButton = MouseButton::Right;
    down.mouseX = 600; down.mouseY = 240; input.Apply(down);        // 여기서 누름
    move.mouseX = 610; move.mouseY = 240; input.Apply(move);
    EXPECT_FLOAT_EQ(input.GetDragDeltaX(MouseButton::Right), 10.0f);
    EXPECT_FLOAT_EQ(input.GetDragDeltaY(MouseButton::Right), 0.0f);
}

// ── 카메라 기즈모 (C7) ───────────────────────────────────────────────────────
#include "../ecs/CameraGizmoSystem.h"
#include "../renderer/DebugLineBuffer.h"
#include "../renderer/Camera.h"

TEST(CameraGizmoTest, Pyramid_ApexAtCamera_FarRectCenteredOnForward)
{
    DebugLineBuffer buffer;
    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);           // 회전 없음 -> 전방 -Z
    CameraComponent cc;
    cc.fov = 90.0f;                                 // tan(45) = 1 -> halfH = length
    AppendCameraGizmoLines(buffer, t, cc, 2.0f, 1.0f);

    ASSERT_EQ(buffer.Lines().size(), 11u);          // 모서리 4 + 사각형 4 + 위 표시 3
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_TRUE(Near(Vec3(1.0f, 2.0f, 3.0f), buffer.Lines()[i].first));   // 꼭짓점 = 카메라 위치
    }
    // 먼 평면 네 모서리의 평균 = 전방 1m 지점, 폭 = 2*halfW = 2*aspect*halfH = 4
    glm::vec3 sum(0.0f);
    for (int i = 0; i < 4; ++i) sum += buffer.Lines()[i].second;
    const glm::vec3 center = sum / 4.0f;
    EXPECT_TRUE(Near(Vec3(1.0f, 2.0f, 2.0f), center));
    const auto& topEdge = buffer.Lines()[4];        // tl -> tr
    EXPECT_NEAR(glm::length(topEdge.second - topEdge.first), 4.0f, 1e-4f);
}

TEST(CameraGizmoTest, System_AddsLinesPerCameraEntity)
{
    DebugLineBuffer buffer;
    Camera aspect;
    CameraGizmoSystem system(&buffer, &aspect);
    ECSRegistry registry;
    for (int i = 0; i < 2; ++i)
    {
        Entity e = AddAt(registry, float(i), 0.0f, 0.0f);
        registry.AddComponent(e, CameraComponent{});
    }
    AddAt(registry, 9.0f, 0.0f, 0.0f);              // 카메라 아님
    system.Update(registry, 0.016f);
    EXPECT_EQ(buffer.Lines().size(), 22u);
}
