#include <gtest/gtest.h>
#include "../ecs/RenderSystem.h"
#include "../ecs/CameraSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../renderer/InstancedBatchManager.h"
#include "../renderer/Camera.h"
#include "../renderer/Mesh.h"
#include <filesystem>
#include <string>
#include <vector>
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
    RenderSystem system(nullptr);

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
    RenderSystem system(nullptr);
    ECSRegistry registry;
    EXPECT_NO_THROW(system.Update(registry, 0.016f));
}


// ── CameraSystem (ECS Main Camera -> 렌더러 Camera). RenderSystem에서 분리됨(C1) ─────────
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

TEST(CameraSystemTest, MainCamera_CopiesTransformPositionToCamera)
{
    Camera camera;
    camera.setPosition(glm::vec3(0.0f));
    CameraSystem system(&camera);

    ECSRegistry registry;
    AddCameraEntity(registry, Vec3(1.0f, 5.0f, 15.0f), /*isMain=*/true);

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(1.0f, 5.0f, 15.0f)));
}

TEST(CameraSystemTest, NonMainCamera_IsIgnored)
{
    Camera camera;
    camera.setPosition(glm::vec3(7.0f, 7.0f, 7.0f));
    CameraSystem system(&camera);

    ECSRegistry registry;
    AddCameraEntity(registry, Vec3(1.0f, 5.0f, 15.0f), /*isMain=*/false);

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(7.0f, 7.0f, 7.0f)));
}

TEST(CameraSystemTest, TransformMovedLater_CameraFollowsNextFrame)
{
    // 에디터 시나리오: Inspector에서 Main Camera의 Position을 바꾸면 다음 프레임에 뷰가 따라와야 한다.
    Camera camera;
    CameraSystem system(&camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(0.0f, 5.0f, 15.0f), /*isMain=*/true);
    system.Update(registry, 0.016f);

    registry.GetComponent<TransformComponent>(cam)->position = Vec3(-4.0f, 2.0f, 9.0f);
    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(-4.0f, 2.0f, 9.0f)));
}

TEST(CameraSystemTest, IsMainCameraSetThroughReflectionJson_SyncsCamera)
{
    // 에디터가 실제로 쓰는 경로: add_component("CameraComponent") 후 SetComponentJson(...)으로
    // isMainCamera=true를 넣는다(demo_scene_seed.py). SetComponentJson 바인딩은 내부에서
    // ComponentInfo::deserialize를 부르므로 여기서 같은 호출을 재현한다.
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("CameraComponent");
    ASSERT_NE(info, nullptr) << "CameraComponent가 ComponentRegistry에 등록되지 않았다";
    ASSERT_NE(info->deserialize, nullptr);

    Camera camera;
    camera.setPosition(glm::vec3(0.0f));
    CameraSystem system(&camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(2.0f, 3.0f, 4.0f), /*isMain=*/false);

    info->deserialize(registry, cam, nlohmann::json::parse(
        R"({"fov": 60.0, "nearPlane": 0.1, "farPlane": 1000.0, "isMainCamera": true})"));
    ASSERT_TRUE(registry.GetComponent<CameraComponent>(cam)->isMainCamera)
        << "deserialize가 isMainCamera를 반영하지 않았다 - 동기화가 안 되는 근본 원인이 이것이다";

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST(CameraSystemTest, WrappedJson_DoesNotSetIsMainCamera)
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

// ── RenderSystem::ResolveMeshHandle (meshPath -> 메시, 개선안 P0-2) ─────────────
// 가짜 로더를 주입해 GL 없이 검증한다. 기본 생성된 Mesh는 GL 핸들이 0이라 소멸해도 안전하다.

namespace
{
    struct FakeLoader
    {
        std::vector<std::string> calls;
        bool succeed = true;
        RenderSystem::MeshLoader Fn()
        {
            return [this](const std::string& fullPath) -> std::shared_ptr<Mesh> {
                calls.push_back(fullPath);
                return succeed ? std::make_shared<Mesh>() : nullptr;
            };
        }
    };

    RenderableComponent RenderableWithPath(const std::string& path)
    {
        RenderableComponent rc;
        rc.meshPath = path;
        return rc;
    }
}

TEST(RenderSystemMeshPathTest, EmptyPath_UsesMeshHandle)
{
    FakeLoader loader;
    RenderSystem system(nullptr, "C:/root", loader.Fn());
    RenderableComponent rc;
    rc.meshHandle = 7;
    EXPECT_EQ(system.ResolveMeshHandle(rc), 7u);
    EXPECT_TRUE(loader.calls.empty());
}

TEST(RenderSystemMeshPathTest, Path_LoadsOnceRelativeToAssetRoot_AndCaches)
{
    FakeLoader loader;
    RenderSystem system(nullptr, "C:/root", loader.Fn());

    uint32_t h1 = system.ResolveMeshHandle(RenderableWithPath("assets/models/cube.obj"));
    uint32_t h2 = system.ResolveMeshHandle(RenderableWithPath("assets/models/cube.obj"));
    uint32_t h3 = system.ResolveMeshHandle(RenderableWithPath("assets/models/pyramid.obj"));

    EXPECT_GE(h1, RenderSystem::kPathHandleBase);
    EXPECT_EQ(h1, h2);                      // 같은 경로는 같은 핸들(같은 배치)
    EXPECT_NE(h1, h3);
    ASSERT_EQ(loader.calls.size(), 2u);     // 경로당 한 번만 로드
    EXPECT_EQ(loader.calls[0], (std::filesystem::path("C:/root") / "assets/models/cube.obj").generic_string());
}

TEST(RenderSystemMeshPathTest, LoadFailure_FallsBackToHandleZero_AndDoesNotRetryEveryFrame)
{
    FakeLoader loader;
    loader.succeed = false;
    RenderSystem system(nullptr, "", loader.Fn());

    EXPECT_EQ(system.ResolveMeshHandle(RenderableWithPath("missing.obj")), 0u);
    EXPECT_EQ(system.ResolveMeshHandle(RenderableWithPath("missing.obj")), 0u);
    ASSERT_EQ(loader.calls.size(), 1u);
    EXPECT_EQ(loader.calls[0], "missing.obj");   // assetRoot가 비면 경로 그대로
}

TEST(RenderSystemMeshPathTest, RegisterMesh_InReservedPathRange_IsIgnored)
{
    FakeLoader loader;
    RenderSystem system(nullptr, "", loader.Fn());
    // 예약 범위 핸들은 무시(에러 로그)돼야 한다 - 크래시 없이 리턴하는지만 확인
    EXPECT_NO_THROW(system.RegisterMesh(RenderSystem::kPathHandleBase + 1, std::make_shared<Mesh>()));
}

TEST(RenderSystemMeshPathTest, MeshPath_RoundTripsThroughReflectionJson)
{
    // meshPath가 직렬화에 안전하다는 설계 전제(프리팹/PIE 스냅샷) 확인
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("RenderableComponent");
    ASSERT_NE(info, nullptr);
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, RenderableComponent{});
    info->deserialize(registry, e, nlohmann::json::parse(R"({"meshPath": "assets/models/cube.obj"})"));
    EXPECT_EQ(registry.GetComponent<RenderableComponent>(e)->meshPath, "assets/models/cube.obj");
}

// ── RenderSystem::ResolveTexture (texturePath -> GL 텍스처, 개선안 P1-2) ─────────
// 가짜 로더가 GL 없이 id만 돌려준다.

namespace
{
    struct FakeTextureLoader
    {
        std::vector<std::string> calls;
        uint32_t nextId = 100;
        bool succeed = true;
        RenderSystem::TextureLoader Fn()
        {
            return [this](const std::string& fullPath) -> RenderSystem::LoadedTexture {
                calls.push_back(fullPath);
                if (!succeed)
                {
                    return {};
                }
                return { nextId++, nullptr };
            };
        }
    };

    RenderableComponent RenderableWithTexture(const std::string& path)
    {
        RenderableComponent rc;
        rc.texturePath = path;
        return rc;
    }
}

TEST(RenderSystemTextureTest, EmptyTexturePath_IsZero_AndDoesNotCallLoader)
{
    FakeTextureLoader tex;
    RenderSystem system(nullptr, "C:/root", {}, tex.Fn());
    EXPECT_EQ(system.ResolveTexture(RenderableComponent{}), 0u);
    EXPECT_TRUE(tex.calls.empty());
}

TEST(RenderSystemTextureTest, Path_LoadsOnceRelativeToAssetRoot_AndCaches)
{
    FakeTextureLoader tex;
    RenderSystem system(nullptr, "C:/root", {}, tex.Fn());

    uint32_t a1 = system.ResolveTexture(RenderableWithTexture("assets/textures/a.png"));
    uint32_t a2 = system.ResolveTexture(RenderableWithTexture("assets/textures/a.png"));
    uint32_t b = system.ResolveTexture(RenderableWithTexture("assets/textures/b.png"));

    EXPECT_EQ(a1, 100u);
    EXPECT_EQ(a1, a2);
    EXPECT_EQ(b, 101u);
    ASSERT_EQ(tex.calls.size(), 2u);
    EXPECT_EQ(tex.calls[0], (std::filesystem::path("C:/root") / "assets/textures/a.png").generic_string());
}

TEST(RenderSystemTextureTest, LoadFailure_IsZero_AndDoesNotRetryEveryFrame)
{
    FakeTextureLoader tex;
    tex.succeed = false;
    RenderSystem system(nullptr, "", {}, tex.Fn());
    EXPECT_EQ(system.ResolveTexture(RenderableWithTexture("missing.png")), 0u);
    EXPECT_EQ(system.ResolveTexture(RenderableWithTexture("missing.png")), 0u);
    EXPECT_EQ(tex.calls.size(), 1u);
}

TEST(RenderSystemTextureTest, NoLoaderConfigured_TexturePathIsIgnoredWithoutCrash)
{
    RenderSystem system(nullptr, "");
    EXPECT_EQ(system.ResolveTexture(RenderableWithTexture("a.png")), 0u);
}

TEST(RenderSystemTextureTest, SameMeshDifferentTexture_ProducesDifferentBatchKeys)
{
    // 텍스처는 배치 단위로 바인딩되므로 같은 메시라도 텍스처가 다르면 배치가 달라야 한다
    EXPECT_FALSE(MakeMeshBatchKey(5, 100) == MakeMeshBatchKey(5, 101));
    EXPECT_FALSE(MakeMeshBatchKey(5, 100) == MakeMeshBatchKey(5));
    EXPECT_TRUE(MakeMeshBatchKey(5, 100) == MakeMeshBatchKey(5, 100));
}

TEST(RenderSystemTextureTest, TexturePath_RoundTripsThroughReflectionJson)
{
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("RenderableComponent");
    ASSERT_NE(info, nullptr);
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, RenderableComponent{});
    info->deserialize(registry, e, nlohmann::json::parse(R"({"texturePath": "assets/textures/checker.png"})"));
    EXPECT_EQ(registry.GetComponent<RenderableComponent>(e)->texturePath, "assets/textures/checker.png");
    nlohmann::json out;
    info->serialize(registry, e, out);
    EXPECT_EQ(out["RenderableComponent"]["texturePath"], "assets/textures/checker.png");
}

// ── CameraSystem 렌즈 반영 (CameraComponent.fov/near/far, 카메라 계획 C2) ───────
// 예전에는 Engine이 투영을 60도로 고정해서 Inspector에서 fov를 바꿔도 반영되지 않았다.

TEST(CameraSystemLensTest, MainCameraLens_IsAppliedToCamera_AspectIsKept)
{
    Camera camera;
    camera.setAspect(2.0f);                 // Engine(뷰포트)이 정한 종횡비
    CameraSystem system(&camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(0.0f, 0.0f, 10.0f), /*isMain=*/true);
    CameraComponent* cc = registry.GetComponent<CameraComponent>(cam);
    cc->fov = 30.0f;
    cc->nearPlane = 0.5f;
    cc->farPlane = 250.0f;

    system.Update(registry, 0.016f);
    EXPECT_NEAR(camera.getFovRadians(), glm::radians(30.0f), 1e-5f);
    EXPECT_FLOAT_EQ(camera.getNearPlane(), 0.5f);
    EXPECT_FLOAT_EQ(camera.getFarPlane(), 250.0f);
    EXPECT_FLOAT_EQ(camera.getAspect(), 2.0f);   // 렌즈 갱신이 종횡비를 덮어쓰지 않는다
}

TEST(CameraSystemLensTest, InvalidLens_IsNotApplied)
{
    Camera camera;
    const float fovBefore = camera.getFovRadians();
    CameraSystem system(&camera);

    ECSRegistry registry;
    Entity cam = AddCameraEntity(registry, Vec3(0.0f, 0.0f, 10.0f), /*isMain=*/true);
    CameraComponent* cc = registry.GetComponent<CameraComponent>(cam);
    cc->fov = 0.0f;                          // 잘못된 값
    cc->farPlane = 0.01f;                    // near(0.1)보다 작음

    system.Update(registry, 0.016f);
    EXPECT_FLOAT_EQ(camera.getFovRadians(), fovBefore);
}

TEST(CameraSystemLensTest, SetAspect_KeepsLens)
{
    Camera camera;
    camera.setLens(glm::radians(40.0f), 0.2f, 300.0f);
    camera.setAspect(1.5f);                  // 리사이즈
    EXPECT_NEAR(camera.getFovRadians(), glm::radians(40.0f), 1e-5f);
    EXPECT_FLOAT_EQ(camera.getNearPlane(), 0.2f);
    EXPECT_FLOAT_EQ(camera.getAspect(), 1.5f);
}

// ── 활성 카메라 선택 규칙 (SelectActiveCamera, 카메라 계획 C3) ────────────────────

namespace
{
    Entity AddCameraWithPriority(ECSRegistry& registry, float x, bool isMain, int priority)
    {
        Entity e = AddCameraEntity(registry, Vec3(x, 0.0f, 10.0f), isMain);
        registry.GetComponent<CameraComponent>(e)->priority = priority;
        return e;
    }
}

TEST(SelectActiveCameraTest, NoCandidates_ReturnsInvalid)
{
    ECSRegistry registry;
    AddCameraWithPriority(registry, 0.0f, /*isMain=*/false, 10);
    ActiveCameraSelection s = SelectActiveCamera(registry);
    EXPECT_FALSE(s.entity.IsValid());
    EXPECT_EQ(s.candidateCount, 0u);
}

TEST(SelectActiveCameraTest, HighestPriorityWins_RegardlessOfOrder)
{
    ECSRegistry registry;
    AddCameraWithPriority(registry, 1.0f, true, 0);
    Entity high = AddCameraWithPriority(registry, 2.0f, true, 5);
    AddCameraWithPriority(registry, 3.0f, true, 1);
    AddCameraWithPriority(registry, 4.0f, false, 99);   // 후보 아님
    ActiveCameraSelection s = SelectActiveCamera(registry);
    EXPECT_EQ(s.entity, high);
    EXPECT_EQ(s.candidateCount, 3u);
    EXPECT_FALSE(s.tie);
}

TEST(SelectActiveCameraTest, Tie_PicksLowestEntityId_AndReportsTie)
{
    ECSRegistry registry;
    Entity first = AddCameraWithPriority(registry, 1.0f, true, 3);
    AddCameraWithPriority(registry, 2.0f, true, 3);
    ActiveCameraSelection s = SelectActiveCamera(registry);
    EXPECT_EQ(s.entity, first);
    EXPECT_TRUE(s.tie);
}

TEST(SelectActiveCameraTest, CameraSystem_FollowsHigherPriorityCamera)
{
    Camera camera;
    CameraSystem system(&camera);
    ECSRegistry registry;
    AddCameraWithPriority(registry, 1.0f, true, 0);
    AddCameraWithPriority(registry, 7.0f, true, 10);

    system.Update(registry, 0.016f);
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(7.0f, 0.0f, 10.0f)));
}

// ── 카메라 전환 블렌드 (CameraComponent.blendInSeconds, 카메라 계획 C4) ─────────

TEST(CameraBlendTest, FirstActivation_IsCut)
{
    Camera camera;
    CameraSystem system(&camera);
    ECSRegistry registry;
    Entity a = AddCameraWithPriority(registry, 10.0f, true, 0);
    registry.GetComponent<CameraComponent>(a)->blendInSeconds = 2.0f;
    system.Update(registry, 0.016f);
    EXPECT_FALSE(system.IsBlending());
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(10.0f, 0.0f, 10.0f)));
}

TEST(CameraBlendTest, SwitchWithBlend_IsHalfwayAtHalfTime_AndExactAtEnd)
{
    Camera camera;
    CameraSystem system(&camera);
    ECSRegistry registry;
    Entity a = AddCameraWithPriority(registry, 0.0f, true, 1);
    system.Update(registry, 0.016f);                                   // A 활성 (x=0)

    Entity b = AddCameraWithPriority(registry, 10.0f, true, 5);        // B가 더 높은 priority (x=10)
    registry.GetComponent<CameraComponent>(b)->blendInSeconds = 1.0f;
    system.Update(registry, 0.0f);                                     // 전환 감지, 경과 0
    EXPECT_TRUE(system.IsBlending());
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(0.0f, 0.0f, 10.0f)));

    system.Update(registry, 0.5f);                                     // smoothstep(0.5) = 0.5
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(5.0f, 0.0f, 10.0f)))
        << "x=" << camera.getPosition().x;

    system.Update(registry, 0.6f);                                     // 끝을 넘김
    EXPECT_FALSE(system.IsBlending());
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(10.0f, 0.0f, 10.0f)));
    (void)a;
}

TEST(CameraBlendTest, SwitchWithZeroBlend_IsCut)
{
    Camera camera;
    CameraSystem system(&camera);
    ECSRegistry registry;
    AddCameraWithPriority(registry, 0.0f, true, 1);
    system.Update(registry, 0.016f);
    AddCameraWithPriority(registry, 10.0f, true, 5);                   // blendInSeconds 기본 0
    system.Update(registry, 0.016f);
    EXPECT_FALSE(system.IsBlending());
    EXPECT_TRUE(NearlyEqual(camera.getPosition(), glm::vec3(10.0f, 0.0f, 10.0f)));
}

TEST(CameraBlendTest, Fov_BlendsToo)
{
    Camera camera;
    CameraSystem system(&camera);
    ECSRegistry registry;
    Entity a = AddCameraWithPriority(registry, 0.0f, true, 1);
    registry.GetComponent<CameraComponent>(a)->fov = 40.0f;
    system.Update(registry, 0.016f);
    Entity b = AddCameraWithPriority(registry, 0.0f, true, 5);
    registry.GetComponent<CameraComponent>(b)->fov = 80.0f;
    registry.GetComponent<CameraComponent>(b)->blendInSeconds = 1.0f;
    system.Update(registry, 0.0f);
    system.Update(registry, 0.5f);
    EXPECT_NEAR(camera.getFovRadians(), glm::radians(60.0f), 1e-4f);
}
