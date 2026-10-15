#include <gtest/gtest.h>
#include "../scene/SceneIO.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Engine;

// docs/GAME_BUILD_PLAN.md Phase 1 — 씬 파일 저장/불러오기.

namespace
{
    // ComponentRegistry에 일부러 등록하지 않는 컴포넌트. 저장 시 "붙어 있지만 파일에 안 들어감"
    // 경고 경로를 검사한다.
    struct UnregisteredSceneTestComponent
    {
        int value = 0;
    };

    std::filesystem::path TempScenePath(const std::string& name)
    {
        return std::filesystem::temp_directory_path() / "qf_sceneio_tests" / std::filesystem::path(std::u8string(name.begin(), name.end()));
    }

    std::string Utf8(const std::filesystem::path& p)
    {
        const std::u8string s = p.u8string();
        return std::string(s.begin(), s.end());
    }

    void WriteText(const std::filesystem::path& p, const std::string& text)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << text;
    }
}

TEST(SceneIOTest, SaveThenLoad_PreservesEntitiesNamesComponentsAndHierarchy)
{
    ECSRegistry source;
    Entity parent = source.CreateEntity();
    source.SetEntityName(parent, "Root");
    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);
    source.AddComponent(parent, t);

    Entity child = source.CreateEntity();
    source.SetEntityName(child, "Child");
    RenderableComponent r;
    r.meshPath = "assets/models/cube.obj";
    r.texturePath = "assets/textures/a.png";
    source.AddComponent(child, r);
    HierarchyComponent h;
    h.parent = parent;
    source.AddComponent(child, h);

    Entity camera = source.CreateEntity();
    CameraComponent cam;
    cam.isMainCamera = true;
    cam.fov = 70.0f;
    source.AddComponent(camera, cam);

    const auto path = TempScenePath("roundtrip.scene.json");
    auto saved = SaveSceneToFile(source, Utf8(path));
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_EQ(saved->entityCount, 3u);
    EXPECT_TRUE(saved->unregisteredComponents.empty());

    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8(path));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(loaded.GetEntityCount(), 3u);

    Entity lParent = loaded.GetEntityByUUID(source.GetUUID(parent));
    Entity lChild = loaded.GetEntityByUUID(source.GetUUID(child));
    Entity lCamera = loaded.GetEntityByUUID(source.GetUUID(camera));
    ASSERT_TRUE(lParent.IsValid());
    ASSERT_TRUE(lChild.IsValid());
    ASSERT_TRUE(lCamera.IsValid());

    EXPECT_EQ(loaded.GetEntityName(lParent), "Root");
    EXPECT_EQ(loaded.GetEntityName(lChild), "Child");
    EXPECT_FLOAT_EQ(loaded.GetComponent<TransformComponent>(lParent)->position.y, 2.0f);
    EXPECT_EQ(loaded.GetComponent<RenderableComponent>(lChild)->meshPath, "assets/models/cube.obj");
    EXPECT_EQ(loaded.GetComponent<RenderableComponent>(lChild)->texturePath, "assets/textures/a.png");
    // 계층의 parent는 UUID로 저장되므로, 새 레지스트리의 runtime id로 다시 연결돼야 한다.
    EXPECT_EQ(loaded.GetComponent<HierarchyComponent>(lChild)->parent.id, lParent.id);
    EXPECT_TRUE(loaded.GetComponent<CameraComponent>(lCamera)->isMainCamera);
    EXPECT_FLOAT_EQ(loaded.GetComponent<CameraComponent>(lCamera)->fov, 70.0f);
}

TEST(SceneIOTest, EveryRegisteredComponentType_RoundTrips)
{
    // 새 컴포넌트(예: 앞으로 만들 인게임 UI 컴포넌트)를 ComponentRegistry에 등록하면 이 테스트가
    // 따로 손대지 않아도 그 컴포넌트를 검사 대상에 넣는다.
    ECSRegistry source;
    std::vector<std::pair<std::string, UUID>> expected;
    for (const auto& [name, info] : ComponentRegistry::GetAllComponents())
    {
        if (!info.deserialize || !info.hasComponent) continue;
        Entity e = source.CreateEntity();
        info.deserialize(source, e, nlohmann::json::object());
        ASSERT_TRUE(info.hasComponent(source, e)) << name << " was not attached by its deserialize()";
        expected.emplace_back(name, source.GetUUID(e));
    }
    ASSERT_FALSE(expected.empty());

    ECSRegistry loaded;
    auto result = ApplySceneDocument(loaded, BuildSceneDocument(source));
    ASSERT_TRUE(result.has_value()) << result.error().message;

    for (const auto& [name, uuid] : expected)
    {
        Entity e = loaded.GetEntityByUUID(uuid);
        ASSERT_TRUE(e.IsValid()) << name;
        const ComponentInfo* info = ComponentRegistry::GetComponentInfo(name);
        ASSERT_NE(info, nullptr);
        EXPECT_TRUE(info->hasComponent(loaded, e)) << name << " did not survive save -> load";
    }
}

TEST(SceneIOTest, Save_ReportsComponentsThatAreNotRegistered)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, UnregisteredSceneTestComponent{ 5 });
    registry.AddComponent(e, TransformComponent{});

    SceneSaveReport report;
    BuildSceneDocument(registry, &report);
    ASSERT_EQ(report.unregisteredComponents.size(), 1u);
    EXPECT_EQ(report.unregisteredComponents[0], "UnregisteredSceneTestComponent");
}

TEST(SceneIOTest, Load_ReportsUnknownComponentsInFile)
{
    nlohmann::json doc = {
        {"format", kSceneFormatName}, {"version", kSceneFormatVersion},
        {"entities", nlohmann::json::array({
            {{"uuid", 42u}, {"components", {{"NoSuchComponent", nlohmann::json::object()},
                                            {"TransformComponent", nlohmann::json::object()}}}}
        })}
    };
    ECSRegistry registry;
    auto result = ApplySceneDocument(registry, doc);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->unknownComponents.size(), 1u);
    EXPECT_EQ(result->unknownComponents[0], "NoSuchComponent");
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(registry.GetEntityByUUID(UUID(42u))));
}

TEST(SceneIOTest, Load_MissingFile_IsFileNotFound)
{
    ECSRegistry registry;
    auto result = LoadSceneFromFile(registry, Utf8(TempScenePath("does_not_exist.scene.json")));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::FileNotFound);
}

TEST(SceneIOTest, Load_BrokenJson_IsResourceLoadFailed)
{
    const auto path = TempScenePath("broken.scene.json");
    WriteText(path, "{ \"format\": \"quarterflying.scene\", ");
    ECSRegistry registry;
    auto result = LoadSceneFromFile(registry, Utf8(path));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::ResourceLoadFailed);
}

TEST(SceneIOTest, Load_NotASceneFile_IsInvalidParameter)
{
    // 예전 scene.json(메시 배치 목록)처럼 format 헤더가 없는 파일.
    const auto path = TempScenePath("legacy.json");
    WriteText(path, R"({"version": 1, "objects": [], "lights": []})");
    ECSRegistry registry;
    auto result = LoadSceneFromFile(registry, Utf8(path));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidParameter);
}

TEST(SceneIOTest, Load_NewerVersion_IsNotSupported)
{
    nlohmann::json doc = { {"format", kSceneFormatName}, {"version", kSceneFormatVersion + 1},
                           {"entities", nlohmann::json::array()} };
    ECSRegistry registry;
    auto result = ApplySceneDocument(registry, doc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotSupported);
}

TEST(SceneIOTest, FailedLoad_LeavesExistingRegistryUntouched)
{
    ECSRegistry registry;
    Entity keep = registry.CreateEntity();
    registry.SetEntityName(keep, "Keep");

    // 컴포넌트 필드 타입이 틀린 손상 파일: 헤더 검사는 통과하지만 deserialize가 예외를 던진다.
    nlohmann::json doc = {
        {"format", kSceneFormatName}, {"version", kSceneFormatVersion},
        {"entities", nlohmann::json::array({
            {{"uuid", 7u}, {"components", {{"CameraComponent", {{"fov", "not a number"}}}}}}
        })}
    };
    auto result = ApplySceneDocument(registry, doc);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::ResourceLoadFailed);
    EXPECT_EQ(registry.GetEntityCount(), 1u);
    EXPECT_EQ(registry.GetEntityName(keep), "Keep");
}

TEST(SceneIOTest, Save_NonAsciiPath_Works)
{
    ECSRegistry source;
    Entity e = source.CreateEntity();
    source.SetEntityName(e, "한글 이름");

    const auto path = TempScenePath("한글 폴더/메인.scene.json");
    auto saved = SaveSceneToFile(source, Utf8(path));
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(path) += ".tmp"));

    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8(path));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(loaded.GetEntityName(loaded.GetEntityByUUID(source.GetUUID(e))), "한글 이름");
}

// ─── 씬 version 2: 프리팹 경로를 프로젝트 상대 경로로 (USABILITY_IMPROVEMENT_PLAN X-1a) ─────────

#include "../prefab/PrefabInstanceComponent.h"
#include "../core/Utf8Path.h"

namespace
{
    std::filesystem::path MakeProjectRoot(const std::string& utf8Name)
    {
        const auto root = TempScenePath(utf8Name);
        std::filesystem::create_directories(root / "assets" / "prefabs");
        return root;
    }

    nlohmann::json SceneWithPrefabInstance(int version, const std::string& prefabPath)
    {
        return {
            {"format", kSceneFormatName}, {"version", version},
            {"entities", nlohmann::json::array({
                { {"uuid", 777}, {"name", "Barrel"}, {"components", {
                    {"PrefabInstanceComponent", { {"prefabPath", prefabPath}, {"sourcePrefabVersion", 1} }}
                }} }
            })}
        };
    }

    std::string LoadedPrefabPath(ECSRegistry& registry)
    {
        Entity e = registry.GetEntityByUUID(777);
        auto* meta = registry.GetComponent<PrefabInstanceComponent>(e);
        return meta ? meta->prefabPath : std::string("<no PrefabInstanceComponent>");
    }
}

TEST(SceneIOTest, SavesVersion2)
{
    ECSRegistry registry;
    registry.CreateEntity();
    EXPECT_EQ(BuildSceneDocument(registry)["version"], 2);
}

TEST(SceneIOTest, Version1AbsolutePrefabPath_InsideProject_LoadsAsRelative_AndSavesAsV2)
{
    const auto root = MakeProjectRoot("v1 한글 프로젝트");
    const std::string abs = Utf8(root / "assets" / "prefabs" / PathFromUtf8("통.prefab.json"));
    const auto path = root / "scenes" / "old.scene.json";
    WriteText(path, SceneWithPrefabInstance(1, abs).dump());

    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8(path), Utf8(root));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_TRUE(result->warnings.empty());
    EXPECT_EQ(LoadedPrefabPath(loaded), "assets/prefabs/통.prefab.json");

    // 다시 저장하면 v2 + 상대 경로
    ASSERT_TRUE(SaveSceneToFile(loaded, Utf8(path)).has_value());
    std::ifstream in(path, std::ios::binary);
    nlohmann::json saved;
    in >> saved;
    EXPECT_EQ(saved["version"], 2);
    EXPECT_EQ(saved["entities"][0]["components"]["PrefabInstanceComponent"]["prefabPath"], "assets/prefabs/통.prefab.json");
}

TEST(SceneIOTest, Version2AbsolutePrefabPath_IsAccepted_AndNormalized)
{
    // 손으로 고친 파일이나 옛 도구 출력: v2인데 절대 경로가 들어 있어도 거절하지 않는다.
    const auto root = MakeProjectRoot("v2abs");
    const auto path = root / "scenes" / "hand.scene.json";
    WriteText(path, SceneWithPrefabInstance(2, Utf8(root / "assets" / "prefabs" / "a.prefab.json")).dump());
    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8(path), Utf8(root));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(LoadedPrefabPath(loaded), "assets/prefabs/a.prefab.json");
}

TEST(SceneIOTest, PrefabPathOutsideProject_IsKept_WithWarning)
{
    const auto root = MakeProjectRoot("inproj");
    const std::string outside = Utf8(TempScenePath("다른 폴더/x.prefab.json"));
    const auto path = root / "scenes" / "out.scene.json";
    WriteText(path, SceneWithPrefabInstance(1, outside).dump());
    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8(path), Utf8(root));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(LoadedPrefabPath(loaded), outside);
    ASSERT_EQ(result->warnings.size(), 1u);
    EXPECT_NE(result->warnings[0].find("outside the project folder"), std::string::npos) << result->warnings[0];
}

TEST(SceneIOTest, NoProjectRoot_LeavesPrefabPathUntouched)
{
    const auto root = MakeProjectRoot("noroot");
    const std::string abs = Utf8(root / "assets" / "prefabs" / "a.prefab.json");
    const auto path = root / "scenes" / "noroot.scene.json";
    WriteText(path, SceneWithPrefabInstance(1, abs).dump());
    ECSRegistry loaded;
    ASSERT_TRUE(LoadSceneFromFile(loaded, Utf8(path)).has_value());
    EXPECT_EQ(LoadedPrefabPath(loaded), abs);
}

TEST(SceneIOTest, PieSnapshot_DoesNotRunFileUpgrade)
{
    // Play/Stop 스냅샷(DeserializeRegistry)은 파일 로더가 아니므로 업그레이드 훅을 부르지 않는다.
    const auto root = MakeProjectRoot("pie");
    const std::string abs = Utf8(root / "assets" / "prefabs" / "a.prefab.json");
    ECSRegistry registry;
    DeserializeRegistry(registry, SceneWithPrefabInstance(2, abs));
    EXPECT_EQ(LoadedPrefabPath(registry), abs);
}
