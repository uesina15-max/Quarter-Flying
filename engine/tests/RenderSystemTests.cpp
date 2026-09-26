#include <gtest/gtest.h>
#include "../ecs/RenderSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Camera.h"
#include "../ecs/Reflection.h"
#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace Engine;

namespace
{
    // glm::quat(계산 편의용) -> Engine::Quaternion(POD, glm 비의존) 변환. glm 생성자는
    // (w,x,y,z) 순서이므로 필드 순서를 헷갈리지 않게 여기서만 변환한다.
    Quaternion ToEngineQuat(const glm::quat& q)
    {
        return Quaternion(q.x, q.y, q.z, q.w);
    }
}

// ── ComposeWorldMatrix ──────────────────────────────────────────────────────

TEST(RenderSystemTest, ComposeWorldMatrix_Identity_ReturnsIdentity)
{
    TransformComponent t;  // 기본값: position=0, rotation=identity, scale=1
    glm::mat4 world = ComposeWorldMatrix(t);
    glm::vec4 p = world * glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
    EXPECT_NEAR(p.x, 1.0f, 1e-5f);
    EXPECT_NEAR(p.y, 2.0f, 1e-5f);
    EXPECT_NEAR(p.z, 3.0f, 1e-5f);
}

TEST(RenderSystemTest, ComposeWorldMatrix_Translation_MovesOrigin)
{
    TransformComponent t;
    t.position = Vec3(3.0f, 4.0f, 5.0f);
    glm::mat4 world = ComposeWorldMatrix(t);
    glm::vec4 p = world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.x, 3.0f, 1e-5f);
    EXPECT_NEAR(p.y, 4.0f, 1e-5f);
    EXPECT_NEAR(p.z, 5.0f, 1e-5f);
}

TEST(RenderSystemTest, ComposeWorldMatrix_Scale_ScalesLocalPoint)
{
    TransformComponent t;
    t.scale = Vec3(2.0f, 2.0f, 2.0f);
    glm::mat4 world = ComposeWorldMatrix(t);
    glm::vec4 p = world * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.x, 2.0f, 1e-5f);
    EXPECT_NEAR(p.y, 0.0f, 1e-5f);
    EXPECT_NEAR(p.z, 0.0f, 1e-5f);
}

TEST(RenderSystemTest, ComposeWorldMatrix_Rotation_RotatesLocalPoint)
{
    // Z축 기준 90도 회전: (1,0,0) -> (0,1,0)
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    TransformComponent t;
    t.rotation = ToEngineQuat(rot);

    glm::mat4 world = ComposeWorldMatrix(t);
    glm::vec4 p = world * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.x, 0.0f, 1e-4f);
    EXPECT_NEAR(p.y, 1.0f, 1e-4f);
    EXPECT_NEAR(p.z, 0.0f, 1e-4f);
}

TEST(RenderSystemTest, ComposeWorldMatrix_TRS_AppliesScaleThenRotateThenTranslate)
{
    // 순서 확인: local point (1,0,0)을 2배 스케일 -> Z축 90도 회전 -> (10,0,0) 이동
    // 이 순서라면 최종 결과는 (10, 2, 0)이어야 한다. 순서가 뒤바뀌면(예: 이동 먼저) 다른 값이 나온다.
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    TransformComponent t;
    t.position = Vec3(10.0f, 0.0f, 0.0f);
    t.rotation = ToEngineQuat(rot);
    t.scale = Vec3(2.0f, 2.0f, 2.0f);

    glm::mat4 world = ComposeWorldMatrix(t);
    glm::vec4 p = world * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.x, 10.0f, 1e-4f);
    EXPECT_NEAR(p.y, 2.0f, 1e-4f);
    EXPECT_NEAR(p.z, 0.0f, 1e-4f);
}

// ── MakeMeshBatchKey ─────────────────────────────────────────────────────────

TEST(RenderSystemTest, MakeMeshBatchKey_SameHandle_ProducesEqualKeys)
{
    EXPECT_TRUE(MakeMeshBatchKey(42) == MakeMeshBatchKey(42));
}

TEST(RenderSystemTest, MakeMeshBatchKey_DifferentHandle_ProducesDifferentKeys)
{
    EXPECT_FALSE(MakeMeshBatchKey(1) == MakeMeshBatchKey(2));
}

// ── RenderSystem::Update (GL 컨텍스트 없는 안전 경로) ──────────────────────────

TEST(RenderSystemTest, Update_WithNullBatchManagerAndCamera_DoesNotCrash)
{
    // batchManager/camera가 nullptr이면(예: GL 컨텍스트 없는 유닛테스트 환경) Update()는
    // 아무 GL 호출도 하지 않고 조용히 리턴해야 한다 - RenderSystem.h의 계약.
    RenderSystem system(nullptr, nullptr);

    ECSRegistry registry;
    Entity renderable = registry.CreateEntity();
    registry.AddComponent(renderable, TransformComponent{});
    registry.AddComponent(renderable, RenderableComponent{});

    Entity camera = registry.CreateEntity();
    TransformComponent camTransform;
    camTransform.position = Vec3(0.0f, 5.0f, 15.0f);
    registry.AddComponent(camera, camTransform);
    CameraComponent cameraComponent;
    cameraComponent.isMainCamera = true;
    registry.AddComponent(camera, cameraComponent);

    EXPECT_NO_THROW(system.Update(registry, 0.016f));
}

TEST(RenderSystemTest, Update_WithEmptyRegistry_DoesNotCrash)
{
    RenderSystem system(nullptr, nullptr);
    ECSRegistry registry;
    EXPECT_NO_THROW(system.Update(registry, 0.016f));
}


// ── RenderSystem::Update 카메라 동기화 (ECS Main Camera -> 렌더러 Camera) ─────────
//
// ROADMAP.md §6에 "RenderSystem의 카메라 sync 블록이 실제로 한 번도 실행되지 않는다(원인
// 미확정)"로 남아 있던 항목의 회귀 테스트. 그때까지 이 경로의 테스트는 camera=nullptr로
// 크래시 여부만 보는 위의 테스트뿐이어서, 동기화가 실제로 되는지는 아무도 확인하지 않았다.
// Camera는 순수 glm 클래스라 GL 컨텍스트 없이 검증할 수 있다.

namespace
{
    Entity AddCameraEntity(ECSRegistry& registry, const Vec3& position, bool isMain)
    {
        Entity e = registry.CreateEntity();
        TransformComponent t;
        t.position = position;
        registry.AddComponent(e, t);
        CameraComponent c;
        c.isMainCamera = isMain;
        registry.AddComponent(e, c);
        return e;
    }

    bool NearlyEqual(const glm::vec3& a, const glm::vec3& b)
    {
        return glm::all(glm::epsilonEqual(a, b, 1e-5f));
    }
}

TEST(RenderSystemCameraSyncTest, MainCamera_CopiesTransformPositionToCamera)
{
    Camera camera;
    camera.setPosition(glm::vec3(0.0f));
    RenderSystem system(nullptr, &camera);

    ECSRegistry registry;
    AddCameraEntity(registry, Vec3(1.0f, 5.0f, 15.0f), /*isMain=*/true);

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(1.0f, 5.0f, 15.0f)));
}

TEST(RenderSystemCameraSyncTest, NonMainCamera_IsIgnored)
{
    Camera camera;
    camera.setPosition(glm::vec3(7.0f, 7.0f, 7.0f));
    RenderSystem system(nullptr, &camera);

    ECSRegistry registry;
    AddCameraEntity(registry, Vec3(1.0f, 5.0f, 15.0f), /*isMain=*/false);

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(7.0f, 7.0f, 7.0f)));
}

TEST(RenderSystemCameraSyncTest, TransformMovedLater_CameraFollowsNextFrame)
{
    // 에디터 시나리오: Inspector에서 Main Camera의 Position을 바꾸면 다음 프레임에 뷰가 따라와야 한다.
    Camera camera;
    RenderSystem system(nullptr, &camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(0.0f, 5.0f, 15.0f), /*isMain=*/true);
    system.Update(registry, 0.016f);

    registry.GetComponent<TransformComponent>(cam)->position = Vec3(-4.0f, 2.0f, 9.0f);
    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(-4.0f, 2.0f, 9.0f)));
}

TEST(RenderSystemCameraSyncTest, IsMainCameraSetThroughReflectionJson_SyncsCamera)
{
    // 에디터가 실제로 쓰는 경로: add_component("CameraComponent") 후 SetComponentJson(...)으로
    // isMainCamera=true를 넣는다(demo_scene_seed.py). SetComponentJson 바인딩은 내부에서
    // ComponentInfo::deserialize를 부르므로 여기서 같은 호출을 재현한다.
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("CameraComponent");
    ASSERT_NE(info, nullptr) << "CameraComponent가 ComponentRegistry에 등록되지 않았다";
    ASSERT_NE(info->deserialize, nullptr);

    Camera camera;
    camera.setPosition(glm::vec3(0.0f));
    RenderSystem system(nullptr, &camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(2.0f, 3.0f, 4.0f), /*isMain=*/false);

    info->deserialize(registry, cam, nlohmann::json::parse(
        R"({"fov": 60.0, "nearPlane": 0.1, "farPlane": 1000.0, "isMainCamera": true})"));
    ASSERT_TRUE(registry.GetComponent<CameraComponent>(cam)->isMainCamera)
        << "deserialize가 isMainCamera를 반영하지 않았다 - 동기화가 안 되는 근본 원인이 이것이다";

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST(RenderSystemCameraSyncTest, WrappedJson_DoesNotSetIsMainCamera)
{
    // 과거 SetComponentJson 바인딩 버그(ECSBindings.cpp 주석)를 문서화하는 테스트:
    // {"CameraComponent": {...}}처럼 한 겹 감싸서 넘기면 필드가 하나도 매칭되지 않아 조용히
    // 무시된다. 이게 ROADMAP §6 "카메라 동기화가 한 번도 실행 안 됨"의 실제 원인이었다
    // (isMainCamera가 끝내 false로 남아 sync 루프가 항상 continue).
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("CameraComponent");
    ASSERT_NE(info, nullptr);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(0.0f, 0.0f, 0.0f), /*isMain=*/false);
    info->deserialize(registry, cam, nlohmann::json::parse(
        R"({"CameraComponent": {"isMainCamera": true}})"));
    EXPECT_FALSE(registry.GetComponent<CameraComponent>(cam)->isMainCamera);
}
