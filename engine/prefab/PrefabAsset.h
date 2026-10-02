#pragma once

#include "../core/EngineError.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Entity.h"
#include "SerializeOptions.h"
#include <nlohmann/json.hpp>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

// See docs/PREFAB_IMPLEMENTATION_PLAN.md for the design this file implements
// (§2 for the decisions, §3 Phase 1 for the exact API this mirrors).

namespace Engine
{
    // Captures a single entity's registered components (Reflection.h's
    // ComponentRegistry) into a JSON blob that can be written to / read from a
    // `*.prefab.json` file, spawned as a brand new entity, or re-applied onto an
    // existing entity to sync it back to the prefab's exact component set.
    //
    // v1 was deliberately one entity = one prefab (plan §2.1). Phase 5 adds
    // children (HierarchyComponent): file format v2 stores an "entities" array.
    // ApplyToEntity (used by Revert) still syncs one entity's components.
    //
    // SerializeEntityComponents (declared alongside PrefabAsset rather than in
    // engine/ecs/Reflection.h) is the shared low-level capture routine both
    // CaptureFromEntity and ApplyToEntity's internal snapshotting use — see the
    // plan's "모듈 경계 정정" revision note for why it lives here and not in
    // Reflection.h (ecs/ must not depend on prefab/).
    std::expected<nlohmann::json, EngineError>
    SerializeEntityComponents(ECSRegistry& registry, Entity entity, SerializeOptions options = {});

    // Name of HierarchyComponent in ComponentRegistry (ecs/Components.h). Prefab
    // capture leaves it out and records parents as indices instead (Phase 5).
    inline constexpr const char* kHierarchyComponentName = "HierarchyComponent";

    // One non-root entity of a multi-entity prefab (Phase 5, file format v2).
    struct PrefabChildEntity
    {
        std::string    name;
        // Index into the prefab's entity list: 0 = root, i >= 1 = children[i - 1].
        // Always smaller than this entity's own index (parents come first), which
        // also makes a cycle impossible to express.
        int            parentIndex = 0;
        nlohmann::json componentsData = nlohmann::json::object();
    };

    class PrefabAsset
    {
    public:
        std::string    name;
        uint32_t       version = 1;                          // prefab file format version (plan §2.2)
        nlohmann::json componentsData = nlohmann::json::object(); // ROOT entity's components; shape matches
                                                                    // the "components" object in *.prefab.json (plan §2.2)

        // Descendants of the root, parents before children (Phase 5). Empty for a
        // single-entity prefab, which is still saved in the v1 format so existing
        // files and tools are unaffected.
        std::vector<PrefabChildEntity> children;

        // Captures `entity` and all of its descendants (HierarchyComponent) as a
        // new PrefabAsset. Excludes PrefabInstanceComponent (plan §2.6) and fails if
        // any captured component declares an EntityRef field (plan §2.6) — except
        // the hierarchy itself, which is recorded as parent indices.
        static std::expected<PrefabAsset, EngineError>
        CaptureFromEntity(ECSRegistry& registry, Entity entity);

        // Creates brand new entities (root + children) and applies this prefab to
        // them. Returns the root. Atomic: on failure every newly created entity is
        // destroyed and nothing is left behind (plan §2.7) — this is a *different*
        // contract from ApplyToEntity's below.
        std::expected<Entity, EngineError>
        SpawnInto(ECSRegistry& registry) const;

        // Same as SpawnInto, but returns every spawned entity: [0] = root,
        // [i] = children[i - 1]. Commands use this to remember UUIDs for Redo.
        std::expected<std::vector<Entity>, EngineError>
        SpawnHierarchyInto(ECSRegistry& registry) const;

        // Applies root + children onto already-existing, freshly created entities
        // (entities.size() must be 1 + children.size(); same order as above) and
        // links children to their parents. Used by SpawnHierarchyInto and by Redo,
        // which recreates the entities with their original UUIDs first. No rollback:
        // on failure the caller destroys the fresh entities.
        std::expected<void, EngineError>
        ApplyHierarchyTo(ECSRegistry& registry, const std::vector<Entity>& entities) const;

        size_t EntityCount() const { return 1 + children.size(); }

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
