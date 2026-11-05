// 에디터 뷰포트 피킹(ecs/Picking.h)과 드래그용 좌표 변환(Hierarchy LocalPositionForWorld),
// RenderSystem::GetMeshBounds. GL 없이 돈다(docs/VIEWPORT_MANIPULATION_PLAN.md).

#include <gtest/gtest.h>
#include "../ecs/Picking.h"
#include "../ecs/Hierarchy.h"
#include "../ecs/RenderSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../renderer/Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Engine;

namespace
{
    Entity AddCube(ECSRegistry& registry, glm::vec3 position, glm::vec3 scale = glm::vec3(1.0f))
    {
        Entity e = registry.CreateEntity();
        TransformComponent t;
        t.position = Vec3(position.x, position.y, position.z);
        t.scale = Vec3(scale.x, scale.y, scale.z);
        registry.AddComponent(e, t);
        registry.AddComponent(e, RenderableComponent{});
        return e;
    }

    // (0,0,10)에서 원점을 보는 카메라 - 화면 가운데가 -Z 방향 광선이다.
    PickRay CenterRay(float ndcX = 0.0f, float ndcY = 0.0f)
    {
        const glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0), glm::vec3(0, 1, 0));
        const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
        auto ray = MakePickRay(view, projection, ndcX, ndcY);
        EXPECT_TRUE(ray.has_value());
        return *ray;
    }
}

TEST(Picking, CenterOfScreenRayPointsAlongCameraForward)
{
    const PickRay ray = CenterRay();
    EXPECT_NEAR(ray.direction.x, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.y, 0.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.z, -1.0f, 1e-5f);
    EXPECT_NEAR(ray.origin.z, 9.9f, 1e-3f);   // near 평면 위
    // 화면 오른쪽 위는 +X, +Y 쪽으로 기운다(NDC y는 위가 +).
    const PickRay corner = CenterRay(0.9f, 0.9f);
    EXPECT_GT(corner.direction.x, 0.0f);
    EXPECT_GT(corner.direction.y, 0.0f);
}

TEST(Picking, RayBoxDistanceRespectsTransformAndMisses)
{
    const PickRay ray{glm::vec3(0, 0, 10), glm::vec3(0, 0, -1)};
    const glm::mat4 identity(1.0f);
    ASSERT_TRUE(IntersectRayBounds(ray, identity, kUnitCubeBounds).has_value());
    EXPECT_NEAR(*IntersectRayBounds(ray, identity, kUnitCubeBounds), 9.5f, 1e-5f);
    // 스케일 2 -> 앞면이 z=1
    EXPECT_NEAR(*IntersectRayBounds(ray, glm::scale(identity, glm::vec3(2.0f)), kUnitCubeBounds), 9.0f, 1e-5f);
    // 옆으로 비켜 있으면 빗나간다
    EXPECT_FALSE(IntersectRayBounds(ray, glm::translate(identity, glm::vec3(2, 0, 0)), kUnitCubeBounds).has_value());
    // 광선 뒤에 있으면 빗나간다
    EXPECT_FALSE(IntersectRayBounds(ray, glm::translate(identity, glm::vec3(0, 0, 20)), kUnitCubeBounds).has_value());
    // 시작점이 상자 안이면 빠져나가는 거리
    const PickRay inside{glm::vec3(0), glm::vec3(0, 0, -1)};
    EXPECT_NEAR(*IntersectRayBounds(inside, identity, kUnitCubeBounds), 0.5f, 1e-5f);
    // 스케일 0(보이지 않음)은 잡히지 않는다
    EXPECT_FALSE(IntersectRayBounds(ray, glm::scale(identity, glm::vec3(0.0f)), kUnitCubeBounds).has_value());
    // 45도 돌린 상자: 모서리가 앞으로 나와 더 가깝다(sqrt(2)/2)
    const glm::mat4 rotated = glm::rotate(identity, glm::radians(45.0f), glm::vec3(0, 1, 0));
    EXPECT_NEAR(*IntersectRayBounds(ray, rotated, kUnitCubeBounds), 10.0f - std::sqrt(2.0f) * 0.5f, 1e-4f);
}

TEST(Picking, NearestEntityWinsAndOnlyVisibleThingsArePickable)
{
    ECSRegistry registry;
    const Entity far = AddCube(registry, glm::vec3(0, 0, -5));
    const Entity near = AddCube(registry, glm::vec3(0, 0, 2));
    // Transform만 있는 빈 엔티티(예: 그룹 부모)는 화면에 아무것도 없으므로 잡히지 않는다.
    Entity empty = registry.CreateEntity();
    registry.AddComponent(empty, TransformComponent{});
    registry.GetTransformComponent(empty)->position = Vec3(0, 0, 5);

    auto hit = PickEntity(registry, CenterRay(), {});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, near);
    EXPECT_NEAR(hit->distance, 10.0f - 2.5f - 0.1f, 1e-3f);   // 시작점이 near 평면(z=9.9)
    EXPECT_NEAR(hit->point.z, 2.5f, 1e-3f);

    registry.DestroyEntity(near);
    hit = PickEntity(registry, CenterRay(), {});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, far);

    EXPECT_FALSE(PickEntity(registry, CenterRay(0.9f, 0.9f), {}).has_value());   // 빈 곳
}

TEST(Picking, ChildUsesWorldMatrixAndMeshBoundsComeFromLookup)
{
    ECSRegistry registry;
    Entity parent = registry.CreateEntity();
    TransformComponent pt;
    pt.position = Vec3(3, 0, 0);
    registry.AddComponent(parent, pt);
    const Entity child = AddCube(registry, glm::vec3(-3, 0, 0));   // 부모 기준 -3 -> 월드 원점
    ASSERT_TRUE(SetParent(registry, child, parent).has_value());

    auto hit = PickEntity(registry, CenterRay(), {});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, child);

    // 키 큰 메시(높이 0..4)면 원점보다 위(y=3)를 지나는 광선에도 잡힌다 - 유닛 큐브라면 빗나간다.
    const PickRay high{glm::vec3(0, 3, 10), glm::vec3(0, 0, -1)};
    EXPECT_FALSE(PickEntity(registry, high, {}).has_value());
    MeshBoundsLookup tall = [](const RenderableComponent&) {
        return LocalBounds{glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 4.0f, 0.5f)};
    };
    hit = PickEntity(registry, high, tall);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, child);
}

TEST(Picking, CamerasAndParticleEmittersUseProxyBox)
{
    ECSRegistry registry;
    Entity camera = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(0, 0, 1);
    t.scale = Vec3(100, 100, 100);   // 스케일은 무시한다(대리 상자는 월드 크기 고정)
    registry.AddComponent(camera, t);
    registry.AddComponent(camera, CameraComponent{});
    Entity particles = registry.CreateEntity();
    t.position = Vec3(0, 0, -1);
    registry.AddComponent(particles, t);
    registry.AddComponent(particles, ParticleEffectComponent{});

    auto hit = PickEntity(registry, CenterRay(), {});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, camera);
    EXPECT_NEAR(hit->point.z, 1.0f + kPickProxyHalfExtent, 1e-3f);
    // 대리 상자 밖(0.5만큼 옆)은 빗나간다
    EXPECT_FALSE(PickEntity(registry, PickRay{glm::vec3(0.5f, 0, 10), glm::vec3(0, 0, -1)}, {}).has_value());

    registry.DestroyEntity(camera);
    hit = PickEntity(registry, CenterRay(), {});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, particles);
}

TEST(Picking, LocalPositionForWorldInvertsRotatedScaledParent)
{
    ECSRegistry registry;
    Entity parent = registry.CreateEntity();
    TransformComponent pt;
    pt.position = Vec3(5, 1, 0);
    const glm::quat q = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    pt.rotation = Quaternion(q.x, q.y, q.z, q.w);
    pt.scale = Vec3(2, 2, 2);
    registry.AddComponent(parent, pt);
    Entity child = registry.CreateEntity();
    registry.AddComponent(child, TransformComponent{});
    ASSERT_TRUE(SetParent(registry, child, parent).has_value());

    const glm::vec3 target(4.0f, 3.0f, -7.0f);
    const glm::vec3 local = LocalPositionForWorld(registry, child, target);
    registry.GetTransformComponent(child)->position = Vec3(local.x, local.y, local.z);
    const glm::vec3 world = glm::vec3(ComputeWorldMatrix(registry, child)[3]);
    EXPECT_NEAR(world.x, target.x, 1e-4f);
    EXPECT_NEAR(world.y, target.y, 1e-4f);
    EXPECT_NEAR(world.z, target.z, 1e-4f);

    // 루트는 그대로
    const glm::vec3 same = LocalPositionForWorld(registry, parent, target);
    EXPECT_EQ(same, target);
}

TEST(Picking, RenderSystemMeshBoundsFollowRegisteredMeshWithoutLoading)
{
    RenderSystem renderSystem(nullptr);
    RenderableComponent rc;
    const LocalBounds cube = renderSystem.GetMeshBounds(rc);   // meshPath 없음 = 절차적 큐브
    EXPECT_EQ(cube.min, kUnitCubeBounds.min);
    EXPECT_EQ(cube.max, kUnitCubeBounds.max);

    auto mesh = std::make_shared<Mesh>();
    mesh->setLocalBounds({{-1, 0, -2, 0, 1, 0, 0, 0}, {1, 3, 2, 0, 1, 0, 0, 0}});
    renderSystem.RegisterMesh("Tall", mesh);
    rc.meshPath = "Tall";
    const LocalBounds tall = renderSystem.GetMeshBounds(rc);
    EXPECT_EQ(tall.min, glm::vec3(-1, 0, -2));
    EXPECT_EQ(tall.max, glm::vec3(1, 3, 2));

    // 아직 불러오지 않은 파일 경로는 불러오지 않고(GL 없음) 대체 큐브 경계를 돌려준다.
    rc.meshPath = "assets/models/never_loaded.obj";
    EXPECT_EQ(renderSystem.GetMeshBounds(rc).max, kUnitCubeBounds.max);
}

// ── 변환 기즈모(renderer/TransformGizmo.h) ───────────────────────────────────
#include "../renderer/TransformGizmo.h"

namespace
{
    const glm::mat4 kGizmoView = glm::lookAt(glm::vec3(0, 5, 15), glm::vec3(0), glm::vec3(0, 1, 0));
    const glm::mat4 kGizmoProjection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
    const glm::vec2 kViewport(1600.0f, 900.0f);
    const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);

    glm::vec2 ToNdc(const glm::vec3& p, const glm::mat4& view = kGizmoView)
    {
        const glm::vec4 clip = kGizmoProjection * view * glm::vec4(p, 1.0f);
        return glm::vec2(clip) / clip.w;
    }

    GizmoHandle PickAt(const TransformGizmo& g, glm::vec2 ndc, const glm::mat4& view = kGizmoView)
    {
        return PickGizmoHandle(g, view, kGizmoProjection, ndc.x, ndc.y, kViewport);
    }
}

TEST(TransformGizmo, ScreenSizeStaysConstantAndBehindCameraHasNone)
{
    const auto near = MakeTransformGizmo(GizmoMode::Move, glm::vec3(0), kIdentity, kGizmoView, kGizmoProjection);
    const auto far = MakeTransformGizmo(GizmoMode::Move, glm::vec3(0, -5, -15), kIdentity, kGizmoView, kGizmoProjection);
    ASSERT_TRUE(near && far);
    EXPECT_NEAR(far->length / near->length, 2.0f, 1e-3f);   // 두 배 거리 -> 두 배 길이
    const glm::vec2 a = ToNdc(near->origin), b = ToNdc(near->AxisTip(GizmoHandle::Y));
    const glm::vec2 c = ToNdc(far->origin), d = ToNdc(far->AxisTip(GizmoHandle::Y));
    EXPECT_NEAR(glm::length(b - a), glm::length(d - c), 0.02f);   // 화면에서는 같은 길이
    EXPECT_FALSE(MakeTransformGizmo(GizmoMode::Move, glm::vec3(0, 5, 30), kIdentity, kGizmoView, kGizmoProjection));
}

TEST(TransformGizmo, MovePicksAxisUnderCursorWithinTolerance)
{
    const auto g = MakeTransformGizmo(GizmoMode::Move, glm::vec3(0), kIdentity, kGizmoView, kGizmoProjection);
    ASSERT_TRUE(g);
    for (GizmoHandle h : {GizmoHandle::X, GizmoHandle::Y, GizmoHandle::Z})
    {
        EXPECT_EQ(PickAt(*g, ToNdc(g->origin + g->AxisDirection(h) * (g->length * 0.6f))), h);
    }
    EXPECT_EQ(PickAt(*g, glm::vec2(0.9f, 0.9f)), GizmoHandle::None);
    // 허용 거리(픽셀) 바로 안/밖: X축 중간에서 화면 위로 kPickTolerancePx -/+ 2픽셀
    const glm::vec2 mid = ToNdc(g->AxisTip(GizmoHandle::X) * 0.6f);
    const float perPixel = 2.0f / kViewport.y;
    EXPECT_EQ(PickAt(*g, mid + glm::vec2(0, (TransformGizmo::kPickTolerancePx - 2) * perPixel)), GizmoHandle::X);
    EXPECT_EQ(PickAt(*g, mid + glm::vec2(0, (TransformGizmo::kPickTolerancePx + 2) * perPixel)), GizmoHandle::None);
}

TEST(TransformGizmo, AxisPointingAtCameraIsNotPickableAndMoveLinesStayOnAxis)
{
    const glm::mat4 front = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0), glm::vec3(0, 1, 0));
    const auto g = MakeTransformGizmo(GizmoMode::Move, glm::vec3(0), kIdentity, front, kGizmoProjection);
    ASSERT_TRUE(g);
    EXPECT_NE(PickAt(*g, glm::vec2(0.0f), front), GizmoHandle::Z);   // 정면에서 Z축은 점이다

    std::vector<std::pair<glm::vec3, glm::vec3>> lines;
    AppendGizmoHandleLines(*g, GizmoHandle::Y, lines);
    ASSERT_FALSE(lines.empty());
    float maxAlong = 0.0f;
    for (const auto& [a, b] : lines)
    {
        for (const glm::vec3& p : {a, b})
        {
            EXPECT_GE(p.y, -1e-5f);
            EXPECT_LE(glm::length(glm::vec2(p.x, p.z)), g->length * 0.1f);   // 축 가까이(화살촉 반지름 안)
            maxAlong = std::max(maxAlong, p.y);
        }
    }
    EXPECT_NEAR(maxAlong, g->length, 1e-5f);   // 화살촉 끝까지
}

TEST(TransformGizmo, RotateRingsArePerpendicularToTheirAxisAndPickable)
{
    const auto g = MakeTransformGizmo(GizmoMode::Rotate, glm::vec3(0), kIdentity, kGizmoView, kGizmoProjection);
    ASSERT_TRUE(g);
    const float radius = g->length * TransformGizmo::kRingRadius;
    for (GizmoHandle h : {GizmoHandle::X, GizmoHandle::Y, GizmoHandle::Z})
    {
        std::vector<std::pair<glm::vec3, glm::vec3>> lines;
        AppendGizmoHandleLines(*g, h, lines);
        ASSERT_FALSE(lines.empty());
        for (const auto& [a, b] : lines)
        {
            EXPECT_NEAR(glm::dot(a, g->AxisDirection(h)), 0.0f, 1e-4f);   // 축에 수직인 평면 위
            EXPECT_NEAR(glm::length(a), radius, radius * 0.02f);           // 반지름(두 겹)
        }
    }
    // 한 고리에만 있는 점(두 고리가 만나는 축 위의 점은 피한다)을 누르면 그 고리. 멀리 바깥은 아무것도 아니다.
    const float d = radius * 0.70710678f;
    EXPECT_EQ(PickAt(*g, ToNdc(glm::vec3(d, 0, d))), GizmoHandle::Y);    // XZ 평면
    EXPECT_EQ(PickAt(*g, ToNdc(glm::vec3(0, d, d))), GizmoHandle::X);    // YZ 평면
    EXPECT_EQ(PickAt(*g, ToNdc(glm::vec3(d, d, 0))), GizmoHandle::Z);    // XY 평면
    EXPECT_EQ(PickAt(*g, glm::vec2(0.9f, 0.9f)), GizmoHandle::None);
}

TEST(TransformGizmo, ScaleUsesLocalAxesAndCenterIsUniform)
{
    // 엔티티가 Y축으로 90도 돌아 있으면 크기 기즈모의 X 손잡이는 월드 -Z를 가리킨다(로컬 X). 이동은 여전히 월드 X.
    const glm::quat turned = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    const auto scale = MakeTransformGizmo(GizmoMode::Scale, glm::vec3(0), turned, kGizmoView, kGizmoProjection);
    const auto move = MakeTransformGizmo(GizmoMode::Move, glm::vec3(0), turned, kGizmoView, kGizmoProjection);
    ASSERT_TRUE(scale && move);
    EXPECT_NEAR(scale->AxisDirection(GizmoHandle::X).z, -1.0f, 1e-5f);
    EXPECT_NEAR(move->AxisDirection(GizmoHandle::X).x, 1.0f, 1e-5f);
    // 가운데는 전체 크기, 손잡이 중간은 그 축
    EXPECT_EQ(PickAt(*scale, ToNdc(scale->origin)), GizmoHandle::Uniform);
    EXPECT_EQ(PickAt(*scale, ToNdc(scale->origin + scale->AxisDirection(GizmoHandle::Y) * (scale->length * 0.6f))),
              GizmoHandle::Y);
    // 이동 모드에는 가운데 손잡이가 없다
    EXPECT_NE(PickAt(*move, ToNdc(move->origin)), GizmoHandle::Uniform);
    std::vector<std::pair<glm::vec3, glm::vec3>> lines;
    AppendGizmoHandleLines(*scale, GizmoHandle::Uniform, lines);
    EXPECT_EQ(lines.size(), 12u);   // 상자 모서리
}
