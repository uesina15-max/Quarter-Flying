#pragma once

#include "../core/EngineError.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Entity.h"
#include "SerializeOptions.h"
#include <nlohmann/json.hpp>
#include <expected>
#include <filesystem>
#include <string>

// See docs/PREFAB_IMPLEMENTATION_PLAN.md for the design this file implements
// (§2 for the decisions, §3 Phase 1 for the exact API this mirrors).

namespace Engine
{
    // Captures a single entity's registered components (Reflection.h's
    // ComponentRegistry) into a JSON blob that can be written to / read from a
    // `*.prefab.json` file, spawned as a brand new entity, or re-applied onto an
    // existing entity to sync it back to the prefab's exact component set.
    //
    // v1 scope is deliberately one entity = one prefab (no hierarchy/children) —
    // see the plan's §2.1 for why.
    //
    // SerializeEntityComponents (declared alongside PrefabAsset rather than in
    // engine/ecs/Reflection.h) is the shared low-level capture routine both
    // CaptureFromEntity and ApplyToEntity's internal snapshotting use — see the
    // plan's "모듈 경계 정정" revision note for why it lives here and not in
    // Reflection.h (ecs/ must not depend on prefab/).
    std::expected<nlohmann::json, EngineError>
    SerializeEntityComponents(ECSRegistry& registry, Entity entity, SerializeOptions options = {});

    class PrefabAsset
    {
    public:
        std::string    name;
        uint32_t       version = 1;                          // prefab file format version (plan §2.2)
        nlohmann::json componentsData = nlohmann::json::object(); // shape matches the "components"
                                                                    // object in *.prefab.json (plan §2.2)

        // Captures `entity`'s current components as a new PrefabAsset. Excludes
        // PrefabInstanceComponent (plan §2.6) and fails if any captured component
        // declares an EntityRef field (plan §2.6).
        static std::expected<PrefabAsset, EngineError>
        CaptureFromEntity(ECSRegistry& registry, Entity entity);

        // Creates a brand new entity and applies this prefab to it. Atomic: on
        // failure the newly created entity is destroyed and nothing is left behind
        // (plan §2.7) — this is a *different* contract from ApplyToEntity's below.
        std::expected<Entity, EngineError>
        SpawnInto(ECSRegistry& registry) const;

        // Syncs an *existing* entity's component set to exactly match this prefab
        // (Definition A, plan §2.5): components the entity has that this prefab
        // doesn't get removed (except PrefabInstanceComponent/TransformComponent,
        // plan §2.5), components this prefab has get applied. Strong guarantee
        // (plan §2.4): on success the entity's components equal the prefab
        // exactly; on failure the entity is left exactly as it was before the
        // call (an existing entity can't be destroyed to roll back, unlike
        // SpawnInto's brand new one).
        std::expected<void, EngineError>
        ApplyToEntity(ECSRegistry& registry, Entity entity) const;

        nlohmann::json ToJson() const;
        static std::expected<PrefabAsset, EngineError> FromJson(const nlohmann::json& json);

        std::expected<void, EngineError> SaveToFile(const std::filesystem::path& path) const;
        static std::expected<PrefabAsset, EngineError> LoadFromFile(const std::filesystem::path& path);
    };
}
