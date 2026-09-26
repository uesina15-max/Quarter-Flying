#include <gtest/gtest.h>
#include "../prefab/PrefabAsset.h"
#include "../prefab/PrefabInstanceComponent.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../ecs/Reflection.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace
{
    // Test-only component carrying an EntityRef field, used to exercise §2.6's
    // "capturing a component with an EntityRef field fails" rule. Registered once
    // (idempotent — ComponentRegistry::Register just overwrites the same map
    // entry) since multiple TESTs in this file need it.
    struct TestRefComponent
    {
        Entity target;
    };

    void EnsureTestRefComponentRegistered()
    {
        static bool registered = false;
        if (registered)
        {
            return;
        }
        registered = true;

        GE_BEGIN_COMPONENT(TestRefComponent)
            GE_FIELD(TestRefComponent, target, EntityRef, "Target")
        GE_END_COMPONENT(TestRefComponent)
    }

    std::filesystem::path TempPrefabPath(const char* name)
    {
        return std::filesystem::temp_directory_path() / name;
    }
}

// ── CaptureFromEntity / round-trip (test 1, 2, 3) ──────────────────────────────

TEST(PrefabAssetTest, CaptureFromEntity_TransformAndRenderable_RoundTripsThroughJsonAndSpawn)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);
    t.scale = Vec3(2.0f, 2.0f, 2.0f);
    registry.AddComponent(e, t);

    RenderableComponent r;
    r.meshHandle = 7;
    r.materialHandle = 3;
    r.castShadows = false;
    registry.AddComponent(e, r);

    auto captured = PrefabAsset::CaptureFromEntity(registry, e);
    ASSERT_TRUE(captured.has_value());

    auto reloaded = PrefabAsset::FromJson(captured->ToJson());
    ASSERT_TRUE(reloaded.has_value());

    ECSRegistry freshRegistry;
    auto spawned = reloaded->SpawnInto(freshRegistry);
    ASSERT_TRUE(spawned.has_value());

    TransformComponent* st = freshRegistry.GetComponent<TransformComponent>(*spawned);
    ASSERT_NE(st, nullptr);
    EXPECT_FLOAT_EQ(st->position.x, 1.0f);
    EXPECT_FLOAT_EQ(st->position.y, 2.0f);
    EXPECT_FLOAT_EQ(st->position.z, 3.0f);
    EXPECT_FLOAT_EQ(st->scale.x, 2.0f);

    RenderableComponent* sr = freshRegistry.GetComponent<RenderableComponent>(*spawned);
    ASSERT_NE(sr, nullptr);
    EXPECT_EQ(sr->meshHandle, 7u);
    EXPECT_EQ(sr->materialHandle, 3u);
    EXPECT_FALSE(sr->castShadows);
}

TEST(PrefabAssetTest, SaveToFile_LoadFromFile_RoundTrips)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(9.0f, 8.0f, 7.0f);
    registry.AddComponent(e, t);

    auto captured = PrefabAsset::CaptureFromEntity(registry, e);
    ASSERT_TRUE(captured.has_value());

    std::filesystem::path path = TempPrefabPath("prefab_asset_test_roundtrip.prefab.json");
    auto saveResult = captured->SaveToFile(path);
    ASSERT_TRUE(saveResult.has_value());

    auto loaded = PrefabAsset::LoadFromFile(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->version, captured->version);
    EXPECT_EQ(loaded->componentsData, captured->componentsData);

    std::filesystem::remove(path);
}

TEST(PrefabAssetTest, SpawnInto_ProducesEntityEquivalentToSource)
{
    ECSRegistry registry;
    Entity source = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(4.0f, 5.0f, 6.0f);
    registry.AddComponent(source, t);

    auto captured = PrefabAsset::CaptureFromEntity(registry, source);
    ASSERT_TRUE(captured.has_value());

    auto spawned = captured->SpawnInto(registry);
    ASSERT_TRUE(spawned.has_value());
    EXPECT_NE(spawned->id, source.id);

    TransformComponent* spawnedTransform = registry.GetComponent<TransformComponent>(*spawned);
    ASSERT_NE(spawnedTransform, nullptr);
    EXPECT_FLOAT_EQ(spawnedTransform->position.x, 4.0f);
    EXPECT_FLOAT_EQ(spawnedTransform->position.y, 5.0f);
    EXPECT_FLOAT_EQ(spawnedTransform->position.z, 6.0f);
}

// ── ApplyToEntity: Definition A (test 4, 5) ────────────────────────────────────

TEST(PrefabAssetTest, ApplyToEntity_UpdatesExistingEntityValues)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});

    PrefabAsset asset;
    asset.componentsData["TransformComponent"] = {
        {"_version", 1},
        {"position", {1.0, 2.0, 3.0}},
        {"rotation", {0.0, 0.0, 0.0}},
        {"scale", {1.0, 1.0, 1.0}}
    };

    auto result = asset.ApplyToEntity(registry, e);
    ASSERT_TRUE(result.has_value());

    TransformComponent* after = registry.GetComponent<TransformComponent>(e);
    ASSERT_NE(after, nullptr);
    EXPECT_FLOAT_EQ(after->position.x, 1.0f);
    EXPECT_FLOAT_EQ(after->position.y, 2.0f);
    EXPECT_FLOAT_EQ(after->position.z, 3.0f);
}

TEST(PrefabAssetTest, ApplyToEntity_RemovesComponentsNotInPrefab_ExceptExemptions)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});
    registry.AddComponent(e, AIComponent{});
    PrefabInstanceComponent meta;
    meta.prefabPath = "assets/prefabs/Whatever.prefab.json";
    registry.AddComponent(e, meta);

    PrefabAsset asset; // componentsData is empty -> only the exemptions should survive
    auto result = asset.ApplyToEntity(registry, e);
    ASSERT_TRUE(result.has_value());

    EXPECT_FALSE(registry.HasComponent<AIComponent>(e));            // removed
    EXPECT_TRUE(registry.HasComponent<TransformComponent>(e));      // exempt -> kept
    EXPECT_TRUE(registry.HasComponent<PrefabInstanceComponent>(e)); // exempt -> kept
}

// ── Capture exclusion rules (test 6, 7) ─────────────────────────────────────────

TEST(PrefabAssetTest, CaptureFromEntity_ExcludesPrefabInstanceComponent)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});
    PrefabInstanceComponent meta;
    meta.prefabPath = "assets/prefabs/Other.prefab.json";
    registry.AddComponent(e, meta);

    auto captured = PrefabAsset::CaptureFromEntity(registry, e);
    ASSERT_TRUE(captured.has_value());
    EXPECT_FALSE(captured->componentsData.contains("PrefabInstanceComponent"));
    EXPECT_TRUE(captured->componentsData.contains("TransformComponent"));
}

TEST(PrefabAssetTest, CaptureFromEntity_RejectsComponentWithEntityRefField)
{
    EnsureTestRefComponentRegistered();

    ECSRegistry registry;
    Entity target = registry.CreateEntity();
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TestRefComponent{target});

    auto captured = PrefabAsset::CaptureFromEntity(registry, e);
    EXPECT_FALSE(captured.has_value());
}

TEST(PrefabAssetTest, CaptureFromEntity_SucceedsWhenNoEntityRefComponentAttached)
{
    EnsureTestRefComponentRegistered();

    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{}); // no EntityRef-bearing component attached

    auto captured = PrefabAsset::CaptureFromEntity(registry, e);
    EXPECT_TRUE(captured.has_value());
}

// ── Component policy table (test 8, 9, 10) ──────────────────────────────────────

TEST(PrefabAssetTest, ApplyToEntity_UnknownComponentName_IsIgnored)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    PrefabAsset asset;
    asset.componentsData["ThisComponentDoesNotExist"] = {{"foo", "bar"}};

    auto result = asset.ApplyToEntity(registry, e);
    EXPECT_TRUE(result.has_value());
}

TEST(PrefabAssetTest, ApplyToEntity_MalformedKnownComponentData_ReturnsError)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();

    PrefabAsset asset;
    asset.componentsData["RenderableComponent"] = {
        {"_version", 1}, {"meshHandle", "not-a-number"}, {"materialHandle", 0}, {"castShadows", true}
    };

    auto result = asset.ApplyToEntity(registry, e);
    EXPECT_FALSE(result.has_value());
}

TEST(PrefabAssetTest, FromJson_MissingVersion_ReturnsError)
{
    nlohmann::json j = {{"name", "Bad"}, {"components", nlohmann::json::object()}};
    auto result = PrefabAsset::FromJson(j);
    EXPECT_FALSE(result.has_value());
}

TEST(PrefabAssetTest, FromJson_VersionWrongType_ReturnsError)
{
    nlohmann::json j = {{"version", "not-a-number"}, {"components", nlohmann::json::object()}};
    auto result = PrefabAsset::FromJson(j);
    EXPECT_FALSE(result.has_value());
}

TEST(PrefabAssetTest, FromJson_NegativeVersion_ReturnsError)
{
    // is_number_integer() is true for negative values too — without an explicit
    // sign check, this would previously wrap around into a huge uint32_t via
    // .get<uint32_t>() instead of being rejected as malformed.
    nlohmann::json j = {{"version", -1}, {"components", nlohmann::json::object()}};
    auto result = PrefabAsset::FromJson(j);
    EXPECT_FALSE(result.has_value());
}

// ── File I/O failure paths (test 11) ────────────────────────────────────────────

TEST(PrefabAssetTest, LoadFromFile_MalformedJson_ReturnsError)
{
    std::filesystem::path path = TempPrefabPath("prefab_asset_test_malformed.prefab.json");
    {
        std::ofstream file(path);
        file << "{ this is not valid json ";
    }

    auto result = PrefabAsset::LoadFromFile(path);
    EXPECT_FALSE(result.has_value());

    std::filesystem::remove(path);
}

TEST(PrefabAssetTest, LoadFromFile_NonexistentPath_ReturnsError)
{
    auto result = PrefabAsset::LoadFromFile(TempPrefabPath("prefab_asset_test_does_not_exist.prefab.json"));
    EXPECT_FALSE(result.has_value());
}

// ── Atomicity: SpawnInto vs. ApplyToEntity strong guarantee (test 12, 13, 14) ──

TEST(PrefabAssetTest, SpawnInto_FailureMidway_LeavesNoOrphanEntity)
{
    ECSRegistry registry;
    size_t countBefore = registry.GetEntityCount();

    PrefabAsset badAsset;
    badAsset.componentsData["RenderableComponent"] = {
        {"_version", 1}, {"meshHandle", "not-a-number"}, {"materialHandle", 0}, {"castShadows", true}
    };

    auto result = badAsset.SpawnInto(registry);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(registry.GetEntityCount(), countBefore);
}

TEST(PrefabAssetTest, ApplyToEntity_FailureMidway_LeavesEntityUnchanged)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});
    RenderableComponent r;
    r.meshHandle = 11;
    registry.AddComponent(e, r);
    AIComponent ai;
    ai.sight_range = 42.0f;
    registry.AddComponent(e, ai);

    PrefabAsset badAsset;
    // No "AIComponent" key -> Definition A step 1 removes AIComponent from `e`
    // before the malformed RenderableComponent data below is even reached.
    badAsset.componentsData["RenderableComponent"] = {
        {"_version", 1}, {"meshHandle", "not-a-number"}, {"materialHandle", 0}, {"castShadows", true}
    };

    auto result = badAsset.ApplyToEntity(registry, e);
    ASSERT_FALSE(result.has_value());

    // Strong guarantee: AIComponent (removed mid-attempt) must be back, and
    // RenderableComponent must still hold its original good value.
    ASSERT_TRUE(registry.HasComponent<AIComponent>(e));
    AIComponent* aiAfter = registry.GetComponent<AIComponent>(e);
    ASSERT_NE(aiAfter, nullptr);
    EXPECT_FLOAT_EQ(aiAfter->sight_range, 42.0f);

    RenderableComponent* rAfter = registry.GetComponent<RenderableComponent>(e);
    ASSERT_NE(rAfter, nullptr);
    EXPECT_EQ(rAfter->meshHandle, 11u);
}

TEST(PrefabAssetTest, SpawnInto_EmptyComponents_CreatesBareEntity)
{
    ECSRegistry registry;
    PrefabAsset asset;
    asset.componentsData = nlohmann::json::object();

    auto result = asset.SpawnInto(registry);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry.IsValid(*result));
    EXPECT_FALSE(registry.HasComponent<TransformComponent>(*result));
    EXPECT_FALSE(registry.HasComponent<RenderableComponent>(*result));
}

// ── SerializeOptions / regression gate (test 15, 16) ────────────────────────────

TEST(PrefabAssetTest, SerializeEntityComponents_ExcludePrefabMetadata_OmitsPrefabInstanceComponent)
{
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    registry.AddComponent(e, TransformComponent{});
    PrefabInstanceComponent meta;
    meta.prefabPath = "assets/prefabs/Whatever.prefab.json";
    registry.AddComponent(e, meta);

    auto withMetadata = SerializeEntityComponents(registry, e, SerializeOptions{});
    ASSERT_TRUE(withMetadata.has_value());
    EXPECT_TRUE(withMetadata->contains("PrefabInstanceComponent"));

    SerializeOptions excludeOptions;
    excludeOptions.excludePrefabMetadata = true;
    auto withoutMetadata = SerializeEntityComponents(registry, e, excludeOptions);
    ASSERT_TRUE(withoutMetadata.has_value());
    EXPECT_FALSE(withoutMetadata->contains("PrefabInstanceComponent"));
}

TEST(PrefabAssetTest, SerializeRegistry_UnaffectedForEntitiesWithoutPrefabInstanceComponent)
{
    // Regression gate (plan §3 Phase 1, test 16): registering PrefabInstanceComponent
    // in Reflection.cpp must not change SerializeRegistry()'s output for entities
    // that never carried one — only entities that actually have the new component
    // should see it appear.
    ECSRegistry registry;
    Entity e = registry.CreateEntity();
    TransformComponent t;
    t.position = Vec3(1.0f, 2.0f, 3.0f);
    registry.AddComponent(e, t);
    RenderableComponent r;
    r.meshHandle = 5;
    registry.AddComponent(e, r);

    nlohmann::json snapshot = SerializeRegistry(registry);

    ASSERT_EQ(snapshot["entities"].size(), 1u);
    const auto& comps = snapshot["entities"][0]["components"];

    EXPECT_TRUE(comps.contains("TransformComponent"));
    EXPECT_TRUE(comps.contains("RenderableComponent"));
    EXPECT_FALSE(comps.contains("PrefabInstanceComponent"));
    EXPECT_FLOAT_EQ(comps["TransformComponent"]["position"][0].get<float>(), 1.0f);
    EXPECT_EQ(comps["RenderableComponent"]["meshHandle"].get<int>(), 5);
}
