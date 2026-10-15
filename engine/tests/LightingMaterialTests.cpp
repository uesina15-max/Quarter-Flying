// 씬 조명 선택(R-1)과 인스턴스 색(R-2) 유닛 테스트. docs/USABILITY_IMPROVEMENT_PLAN.md R-1, R-2.
// GL 없이 돈다: 선택 규칙과 색 변환은 순수 함수(SelectSceneLighting, MakeInstanceMaterial)이고, 레거시 색 보정은
// 파일 로더(SceneIO, PrefabAsset)의 업그레이드 훅이다.

#include <gtest/gtest.h>
#include "../ecs/RenderSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../scene/SceneIO.h"
#include "../prefab/PrefabAsset.h"
#include "../core/Utf8Path.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace
{
    Entity AddLight(ECSRegistry& registry, Vec3 direction, int priority = 0, bool enabled = true, float intensity = 1.0f)
    {
        Entity e = registry.CreateEntity();
        DirectionalLightComponent light;
        light.direction = direction;
        light.priority = priority;
        light.enabled = enabled;
        light.intensity = intensity;
        registry.AddComponent(e, light);
        return e;
    }

    bool Near(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f)
    {
        return glm::length(a - b) < eps;
    }

    std::filesystem::path TempPath(const std::string& utf8)
    {
        return std::filesystem::temp_directory_path() / "qf_lighting_material_tests" / PathFromUtf8(utf8);
    }

    void WriteText(const std::filesystem::path& p, const std::string& text)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << text;
    }

    nlohmann::json SceneWithRenderables(const nlohmann::json& renderables)
    {
        nlohmann::json entities = nlohmann::json::array();
        uint64_t uuid = 100;
        for (const auto& r : renderables)
        {
            entities.push_back({ {"uuid", uuid++}, {"name", "R"}, {"components", {
                {"TransformComponent", { {"position", {0, 0, 0}}, {"rotation", {0, 0, 0}}, {"scale", {1, 1, 1}} }},
                {"RenderableComponent", r} }} });
        }
        return { {"format", kSceneFormatName}, {"version", 2}, {"entities", entities} };
    }

    Vec3 LoadedBaseColor(ECSRegistry& registry, uint64_t uuid)
    {
        return registry.GetComponent<RenderableComponent>(registry.GetEntityByUUID(UUID(uuid)))->baseColor;
    }
}

// ── R-1 조명 선택 ─────────────────────────────────────────────────────────────

TEST(SceneLightingTest, NoLight_UsesDefault_SameAsBeforeLightsExisted)
{
    ECSRegistry registry;
    const auto selection = SelectSceneLighting(registry);
    EXPECT_FALSE(selection.entity.IsValid());
    const SceneLighting def = SceneLighting::Default();
    EXPECT_TRUE(Near(selection.lighting.toLight, glm::normalize(glm::vec3(0.4f, 1.0f, 0.35f))));
    EXPECT_TRUE(Near(selection.lighting.color, glm::vec3(1.0f)));
    EXPECT_FLOAT_EQ(selection.lighting.ambient, def.ambient);
}

TEST(SceneLightingTest, DefaultComponent_LooksExactlyLikeNoLight)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, DirectionalLightComponent{});
    const auto selection = SelectSceneLighting(registry);
    const SceneLighting def = SceneLighting::Default();
    EXPECT_TRUE(Near(selection.lighting.toLight, def.toLight));
    EXPECT_TRUE(Near(selection.lighting.color, def.color));
    EXPECT_FLOAT_EQ(selection.lighting.ambient, def.ambient);
}

TEST(SceneLightingTest, DirectionIsNegatedAndNormalized_ColorTimesIntensity)
{
    ECSRegistry registry;
    AddLight(registry, Vec3(0.0f, -2.0f, 0.0f), 0, true, 2.0f);
    const auto selection = SelectSceneLighting(registry);
    EXPECT_TRUE(Near(selection.lighting.toLight, glm::vec3(0.0f, 1.0f, 0.0f)));
    EXPECT_TRUE(Near(selection.lighting.color, glm::vec3(2.0f)));
}

TEST(SceneLightingTest, HighestPriorityWins_DisabledIgnored)
{
    ECSRegistry registry;
    AddLight(registry, Vec3(1, 0, 0), 5, false);          // 꺼짐 - priority가 높아도 무시
    Entity mid = AddLight(registry, Vec3(0, -1, 0), 2);
    AddLight(registry, Vec3(0, 0, -1), 1);
    const auto selection = SelectSceneLighting(registry);
    EXPECT_EQ(selection.entity, mid);
    EXPECT_EQ(selection.candidateCount, 2u);
}

TEST(SceneLightingTest, PriorityTie_SmallestUuidWins_EvenAfterSaveAndLoad)
{
    ECSRegistry registry;
    Entity a = AddLight(registry, Vec3(0, -1, 0));
    Entity b = AddLight(registry, Vec3(1, -1, 0));
    const uint64_t ua = registry.GetUUID(a).GetValue();
    const uint64_t ub = registry.GetUUID(b).GetValue();
    const uint64_t expected = std::min(ua, ub);
    EXPECT_EQ(registry.GetUUID(SelectSceneLighting(registry).entity).GetValue(), expected);

    // 저장 -> 불러오기(엔티티 id는 새로 매겨진다)해도 같은 라이트
    ECSRegistry loaded;
    ASSERT_TRUE(ApplySceneDocument(loaded, BuildSceneDocument(registry)).has_value());
    EXPECT_EQ(loaded.GetUUID(SelectSceneLighting(loaded).entity).GetValue(), expected);
}

TEST(SceneLightingTest, ZeroDirection_FallsBackToDefaultDirection_AndFlags)
{
    ECSRegistry registry;
    AddLight(registry, Vec3(0, 0, 0), 0, true, 0.5f);
    const auto selection = SelectSceneLighting(registry);
    EXPECT_TRUE(selection.invalidDirection);
    EXPECT_TRUE(Near(selection.lighting.toLight, SceneLighting::Default().toLight));
    EXPECT_TRUE(Near(selection.lighting.color, glm::vec3(0.5f)));
}

// ── R-2 인스턴스 색 ───────────────────────────────────────────────────────────

TEST(InstanceMaterialTest, NewComponentIsWhiteTint_WithOldRoughnessMetallic)
{
    RenderableComponent r;
    const auto m = MakeInstanceMaterial(r);
    EXPECT_TRUE(Near(m.color, glm::vec3(1.0f)));
    EXPECT_FLOAT_EQ(m.roughness, 0.6f);
    EXPECT_FLOAT_EQ(m.metallic, 0.0f);
    r.baseColor = Vec3(0.2f, 0.4f, 0.8f);
    EXPECT_TRUE(Near(MakeInstanceMaterial(r).color, glm::vec3(0.2f, 0.4f, 0.8f)));
}

TEST(InstanceMaterialTest, LegacySceneFile_KeepsOldLook)
{
    // baseColor가 없는 옛 파일: 텍스처 없으면 회색, 있으면 흰색. 명시된 값은 그대로.
    const auto path = TempPath("레거시/old.scene.json");
    WriteText(path, SceneWithRenderables(nlohmann::json::array({
        { {"meshPath", "a.obj"} },
        { {"meshPath", "a.obj"}, {"texturePath", "assets/textures/c.png"} },
        { {"meshPath", "a.obj"}, {"baseColor", {0.1f, 0.2f, 0.3f}} } })).dump());
    ECSRegistry loaded;
    ASSERT_TRUE(LoadSceneFromFile(loaded, Utf8FromPath(path)).has_value());
    EXPECT_FLOAT_EQ(LoadedBaseColor(loaded, 100).x, 0.75f);
    EXPECT_FLOAT_EQ(LoadedBaseColor(loaded, 100).z, 0.78f);
    EXPECT_FLOAT_EQ(LoadedBaseColor(loaded, 101).x, 1.0f);
    EXPECT_FLOAT_EQ(LoadedBaseColor(loaded, 102).y, 0.2f);
}

TEST(InstanceMaterialTest, LegacyPrefabFile_KeepsOldLook)
{
    const auto path = TempPath("레거시/old.prefab.json");
    WriteText(path, R"({"version": 1, "name": "Old", "components": {
        "TransformComponent": {"position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1]},
        "RenderableComponent": {"meshHandle": 0}}})");
    auto asset = PrefabAsset::LoadFromFile(path);
    ASSERT_TRUE(asset.has_value()) << asset.error().message;
    ECSRegistry registry;
    auto spawned = asset->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value()) << spawned.error().message;
    EXPECT_FLOAT_EQ(registry.GetComponent<RenderableComponent>(*spawned)->baseColor.y, 0.75f);
}

TEST(InstanceMaterialTest, AddComponentAndPartialJson_DoNotRunLegacyMigration)
{
    // AddComponent는 빈 JSON {}로 역직렬화한다 -> 새 기본값(흰색)이어야 한다(레거시 회색 아님).
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo("RenderableComponent");
    ASSERT_NE(info, nullptr);
    info->deserialize(registry, e, nlohmann::json::object());
    EXPECT_FLOAT_EQ(registry.GetComponent<RenderableComponent>(e)->baseColor.x, 1.0f);

    // 일부 필드만 담은 JSON(SetComponentJson)은 나머지를 그대로 둔다 - 색이 회색으로 되돌아가면 안 된다.
    registry.GetComponent<RenderableComponent>(e)->baseColor = Vec3(0.3f, 0.3f, 0.3f);
    info->deserialize(registry, e, nlohmann::json{ {"meshPath", "b.obj"} });
    EXPECT_FLOAT_EQ(registry.GetComponent<RenderableComponent>(e)->baseColor.x, 0.3f);
}
