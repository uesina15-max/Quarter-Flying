// 머티리얼 에셋(R-4, 결정 2026-10-15) 유닛 테스트: 파일 파서, 합성 규칙, RenderSystem의 경로별 로드, 옛 스키마 파일.
// docs/USABILITY_IMPROVEMENT_PLAN.md R-4.

#include <gtest/gtest.h>
#include "../asset/MaterialAsset.h"
#include "../ecs/RenderSystem.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include "../scene/SceneIO.h"
#include "../core/Utf8Path.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace
{
    std::filesystem::path TempPath(const std::string& utf8)
    {
        return std::filesystem::temp_directory_path() / "qf_material_tests" / PathFromUtf8(utf8);
    }

    void WriteText(const std::filesystem::path& p, const std::string& text)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << text;
    }

    nlohmann::json Gold()
    {
        return { {"format", kMaterialFormatName}, {"version", 1}, {"baseColor", {1.0, 0.8, 0.3}},
                 {"texture", "assets/textures/gold.png"}, {"roughness", 0.25}, {"metallic", 1.0} };
    }
}

// ── 파서 ─────────────────────────────────────────────────────────────────────

TEST(MaterialAssetTest, ParsesAllFields)
{
    auto m = ParseMaterial(Gold());
    ASSERT_TRUE(m.has_value()) << m.error().message;
    EXPECT_FLOAT_EQ(m->baseColor.y, 0.8f);
    EXPECT_EQ(m->texture, "assets/textures/gold.png");
    EXPECT_FLOAT_EQ(m->roughness, 0.25f);
    EXPECT_FLOAT_EQ(m->metallic, 1.0f);
}

TEST(MaterialAssetTest, MissingKeysUseDefaults)
{
    auto m = ParseMaterial({ {"format", kMaterialFormatName}, {"version", 1} });
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->baseColor.x, 1.0f);
    EXPECT_TRUE(m->texture.empty());
    EXPECT_FLOAT_EQ(m->roughness, 0.6f);
    EXPECT_FLOAT_EQ(m->metallic, 0.0f);
}

TEST(MaterialAssetTest, WrongTypesAndRanges_AreErrorsNamingTheKey)
{
    auto bad = Gold();
    bad["roughness"] = "rough";
    auto r1 = ParseMaterial(bad);
    ASSERT_FALSE(r1.has_value());
    EXPECT_NE(r1.error().message.find("roughness"), std::string::npos);

    bad = Gold();
    bad["metallic"] = 1.5;
    EXPECT_FALSE(ParseMaterial(bad).has_value());

    bad = Gold();
    bad["baseColor"] = { 1, 2 };
    EXPECT_FALSE(ParseMaterial(bad).has_value());

    bad = Gold();
    bad["format"] = "quarterflying.scene";
    EXPECT_FALSE(ParseMaterial(bad).has_value());

    bad = Gold();
    bad["version"] = kMaterialFormatVersion + 1;
    auto r2 = ParseMaterial(bad);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error().code, EngineErrorCode::NotSupported);
}

TEST(MaterialAssetTest, LoadsFromKoreanPath_AndReportsMissingFile)
{
    const auto path = TempPath("한글 폴더/금속.material.json");
    WriteText(path, Gold().dump());
    auto m = LoadMaterialFile(Utf8FromPath(path));
    ASSERT_TRUE(m.has_value()) << m.error().message;
    EXPECT_FLOAT_EQ(m->metallic, 1.0f);

    auto missing = LoadMaterialFile(Utf8FromPath(TempPath("없는.material.json")));
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, EngineErrorCode::FileNotFound);
}

// ── 합성 규칙 ─────────────────────────────────────────────────────────────────

TEST(MaterialCompositionTest, ColorIsMaterialTimesTint_SurfaceFromMaterial)
{
    MaterialData gold = *ParseMaterial(Gold());
    RenderableComponent r;
    r.baseColor = Vec3(0.5f, 1.0f, 1.0f);
    const auto m = MakeInstanceMaterial(r, &gold);
    EXPECT_FLOAT_EQ(m.color.x, 0.5f);
    EXPECT_FLOAT_EQ(m.color.y, 0.8f);
    EXPECT_FLOAT_EQ(m.roughness, 0.25f);
    EXPECT_FLOAT_EQ(m.metallic, 1.0f);
}

TEST(MaterialCompositionTest, NoMaterial_UsesDefaults_SameAsBeforeMaterials)
{
    RenderableComponent r;
    r.baseColor = Vec3(0.75f, 0.75f, 0.78f);
    const auto m = MakeInstanceMaterial(r, nullptr);
    EXPECT_FLOAT_EQ(m.color.z, 0.78f);
    EXPECT_FLOAT_EQ(m.roughness, 0.6f);   // 셰이더가 예전 고정값과 같게 그리는 값
    EXPECT_FLOAT_EQ(m.metallic, 0.0f);
}

TEST(MaterialCompositionTest, ComponentTextureOverridesMaterialTexture)
{
    MaterialData gold = *ParseMaterial(Gold());
    RenderableComponent r;
    EXPECT_EQ(EffectiveTexturePath(r, &gold), "assets/textures/gold.png");
    r.texturePath = "assets/textures/override.png";
    EXPECT_EQ(EffectiveTexturePath(r, &gold), "assets/textures/override.png");
    EXPECT_EQ(EffectiveTexturePath(RenderableComponent{}, nullptr), "");
}

// ── RenderSystem: 경로별 로드 ─────────────────────────────────────────────────

TEST(MaterialResolveTest, LoadsOncePerPath_RelativeToAssetRoot_FailureWarnsOnce)
{
    std::vector<std::string> calls;
    RenderSystem system(nullptr, "C:/프로젝트");
    system.SetMaterialLoader([&](const std::string& full) -> Result<MaterialData> {
        calls.push_back(full);
        if (full.find("broken") != std::string::npos)
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed, "bad", "test");
        return *ParseMaterial(Gold());
    });
    const MaterialData* a = system.ResolveMaterial("assets/materials/gold.material.json");
    const MaterialData* b = system.ResolveMaterial("assets/materials/gold.material.json");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_EQ(system.ResolveMaterial("assets/materials/broken.material.json"), nullptr);
    EXPECT_EQ(system.ResolveMaterial("assets/materials/broken.material.json"), nullptr);
    EXPECT_EQ(system.ResolveMaterial(""), nullptr);
    ASSERT_EQ(calls.size(), 2u);                 // 경로당 한 번(실패도 다시 시도하지 않음)
    EXPECT_EQ(calls[0], "C:/프로젝트/assets/materials/gold.material.json");
}

TEST(MaterialResolveTest, NoLoaderConfigured_IgnoresMaterialWithoutCrash)
{
    RenderSystem system(nullptr, "");
    EXPECT_EQ(system.ResolveMaterial("assets/materials/x.material.json"), nullptr);
}

// ── 옛 스키마(_version 1) 파일 ───────────────────────────────────────────────

TEST(RenderableSchemaTest, SavesVersion2_WithMaterialPath_WithoutHandles)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    RenderableComponent r;
    r.materialPath = "assets/materials/gold.material.json";
    registry.AddComponent(e, r);
    const auto doc = BuildSceneDocument(registry);
    const auto& rc = doc["entities"][0]["components"]["RenderableComponent"];
    EXPECT_EQ(rc["_version"], 2);
    EXPECT_EQ(rc["materialPath"], "assets/materials/gold.material.json");
    EXPECT_FALSE(rc.contains("meshHandle"));
    EXPECT_FALSE(rc.contains("materialHandle"));
    EXPECT_FALSE(rc.contains("roughness"));
}

TEST(RenderableSchemaTest, Version1File_LoadsAndWarnsAboutNonZeroMeshHandle)
{
    const auto path = TempPath("옛 스키마/old.scene.json");
    nlohmann::json doc = { {"format", kSceneFormatName}, {"version", 2}, {"entities", nlohmann::json::array({
        { {"uuid", 10}, {"name", "Old"}, {"components", { {"RenderableComponent", {
            {"_version", 1}, {"meshHandle", 7}, {"materialHandle", 3}, {"castShadows", false},
            {"meshPath", "assets/models/cube.obj"}, {"roughness", 0.2}, {"metallic", 0.9} }} }} } }) } };
    WriteText(path, doc.dump());
    ECSRegistry loaded;
    auto result = LoadSceneFromFile(loaded, Utf8FromPath(path));
    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_EQ(result->warnings.size(), 1u);
    EXPECT_NE(result->warnings[0].find("meshHandle 7"), std::string::npos) << result->warnings[0];
    const auto* r = loaded.GetComponent<RenderableComponent>(loaded.GetEntityByUUID(UUID(10)));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->meshPath, "assets/models/cube.obj");
    EXPECT_FALSE(r->castShadows);
    EXPECT_FLOAT_EQ(r->baseColor.x, 0.75f);   // 옛 파일 + 텍스처 없음 -> 예전 회색(R-2 훅)
}
