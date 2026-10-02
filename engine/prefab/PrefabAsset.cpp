#include "PrefabAsset.h"
#include "PrefabInstanceComponent.h"
#include "../ecs/Reflection.h"
#include "../ecs/Hierarchy.h"
#include "../core/logging/Logger.h"

#include <fstream>
#include <unordered_map>

namespace Engine
{
    // ============================================================
    // SerializeEntityComponents
    // ============================================================
    //
    // Deliberately does NOT reuse SerializeRegistry()'s internal loop (Reflection.cpp)
    // even though the two are structurally similar — sharing that loop would require
    // ecs/Reflection.h to know about SerializeOptions (a prefab/-owned type), which
    // would put a Reflection -> Prefab dependency in the wrong direction. The two
    // loops change for different reasons (SerializeRegistry for PIE-snapshot needs,
    // this one for prefab-capture policy), so a small amount of duplication here is
    // the accepted tradeoff — see the plan's "모듈 경계 정정" revision note.
    std::expected<nlohmann::json, EngineError>
    SerializeEntityComponents(ECSRegistry& registry, Entity entity, SerializeOptions options)
    {
        nlohmann::json compsJson = nlohmann::json::object();

        for (const auto& pair : ComponentRegistry::GetAllComponents())
        {
            const auto& info = pair.second;

            if (options.excludePrefabMetadata && info.name == kPrefabInstanceComponentName)
            {
                continue;
            }
            if (options.excludeHierarchy && info.name == kHierarchyComponentName)
            {
                continue;
            }

            bool entityHasComponent = info.hasComponent && info.hasComponent(registry, entity);

            if (options.rejectEntityRefs && entityHasComponent)
            {
                for (const auto& field : info.fields)
                {
                    if (field.type == FieldType::EntityRef)
                    {
                        return MakeError(EngineErrorCode::InvalidParameter,
                            "Component '" + info.name + "' has an EntityRef field; "
                            "v1 prefabs cannot capture entity references",
                            "PrefabAsset");
                    }
                }
            }

            // No hasComponent gate here beyond the EntityRef check above — mirrors
            // SerializeRegistry()'s original unconditional call: info.serialize's
            // generated body already no-ops (GetComponent returns null -> early
            // return) when the entity doesn't have this component type.
            if (info.serialize)
            {
                info.serialize(registry, entity, compsJson);
            }
        }

        return compsJson;
    }

    // ============================================================
    // ApplyToEntity internals (plan §2.4 — Definition A + strong guarantee)
    // ============================================================

    namespace
    {
        bool IsRemovalExempt(const std::string& componentName)
        {
            // PrefabInstanceComponent: removing it would erase the very fact that
            // this entity is a prefab instance. TransformComponent: a live scene
            // entity must always have a position; a malformed/hand-edited prefab
            // file missing a TransformComponent block must not strip it from an
            // otherwise-fine entity (plan §2.5).
            // HierarchyComponent (Phase 5): where the instance sits in the scene, not
            // prefab content — prefab files never contain it (capture records parent
            // indices instead). Without this exemption, Revert on an instance the
            // user had placed under another entity would silently unparent it.
            return componentName == kPrefabInstanceComponentName || componentName == "TransformComponent" ||
                   componentName == kHierarchyComponentName;
        }

        // Definition A, part 1: remove components the entity currently has that
        // are absent from targetComponents (except the two exemptions above).
        void RemoveMissingComponents(ECSRegistry& registry, Entity entity, const nlohmann::json& targetComponents)
        {
            for (const auto& pair : ComponentRegistry::GetAllComponents())
            {
                const auto& info = pair.second;

                if (IsRemovalExempt(info.name))
                {
                    continue;
                }

                if (!info.hasComponent || !info.hasComponent(registry, entity))
                {
                    continue;
                }

                if (!targetComponents.contains(info.name) && info.remove)
                {
                    info.remove(registry, entity);
                }
            }
        }

        // Definition A, part 2: apply every component present in targetComponents.
        // An unregistered component name is ignored (policy table row 1 — forward
        // compatibility). A registered component whose data doesn't match its
        // field types throws inside nlohmann::json (json::type_error) from within
        // the GE_BEGIN_COMPONENT-generated deserialize lambda — caught here and
        // turned into a controlled EngineError (policy table row 3) instead of an
        // uncaught exception reaching the caller.
        std::expected<void, EngineError>
        DeserializePrefabComponents(ECSRegistry& registry, Entity entity, const nlohmann::json& targetComponents)
        {
            for (const auto& item : targetComponents.items())
            {
                const std::string& compName = item.key();
                const ComponentInfo* info = ComponentRegistry::GetComponentInfo(compName);
                if (!info || !info->deserialize)
                {
                    continue; // unknown component — ignore (forward compatibility)
                }

                try
                {
                    info->deserialize(registry, entity, item.value());
                }
                catch (const std::exception& e)
                {
                    return MakeError(EngineErrorCode::ResourceLoadFailed,
                        "Failed to apply component '" + compName + "': " + e.what(),
                        "PrefabAsset");
                }
            }

            return {};
        }

        // Runs both halves of Definition A against targetComponents. Used both to
        // apply the prefab's own data and (via RestoreSnapshotOnFailure below) to
        // restore a pre-capture snapshot — same code path either way, so "applied"
        // and "restored" states can never drift apart from each other.
        std::expected<void, EngineError>
        SynchronizeComponents(ECSRegistry& registry, Entity entity, const nlohmann::json& targetComponents)
        {
            RemoveMissingComponents(registry, entity, targetComponents);
            return DeserializePrefabComponents(registry, entity, targetComponents);
        }

        // Full-state capture (default SerializeOptions — metadata included) used
        // only as ApplyToEntity's internal rollback point; never written to a
        // prefab file.
        nlohmann::json CaptureSnapshot(ECSRegistry& registry, Entity entity)
        {
            auto result = SerializeEntityComponents(registry, entity, SerializeOptions{});
            return result ? result.value() : nlohmann::json::object();
        }

        // Best-effort restore to a snapshot captured just before a failed
        // SynchronizeComponents call. Reuses SynchronizeComponents itself rather
        // than duplicating remove/deserialize logic. Not itself guaranteed to
        // succeed by the type system (replaying already-valid in-memory state
        // should succeed in practice) — see plan §2.4.
        void RestoreSnapshotOnFailure(ECSRegistry& registry, Entity entity, const nlohmann::json& snapshot)
        {
            auto restoreResult = SynchronizeComponents(registry, entity, snapshot);
            if (!restoreResult)
            {
                // Restoring already-valid in-memory state should succeed in
                // practice, but isn't guaranteed by the type system (plan §2.4) —
                // the entity may be left partially synced. Not escalated into
                // ApplyToEntity's own return value: the caller already gets the
                // original SynchronizeComponents error, which is the actionable one.
                Logger::Error("PrefabAsset: failed to restore entity to its pre-ApplyToEntity "
                               "snapshot after a failed apply: {}", restoreResult.error().message);
            }
        }

        // ApplyToEntity's body, shared with the per-child application in
        // ApplyHierarchyTo: Definition A + strong guarantee against one component set.
        std::expected<void, EngineError>
        ApplyComponentsWithRollback(ECSRegistry& registry, Entity entity, const nlohmann::json& components)
        {
            const nlohmann::json snapshot = CaptureSnapshot(registry, entity);

            auto syncResult = SynchronizeComponents(registry, entity, components);
            if (syncResult)
            {
                return {};
            }

            RestoreSnapshotOnFailure(registry, entity, snapshot);
            return std::unexpected(syncResult.error());
        }

        SerializeOptions PrefabCaptureOptions()
        {
            SerializeOptions options;
            options.excludePrefabMetadata = true;
            options.rejectEntityRefs = true;
            options.excludeHierarchy = true;
            return options;
        }
    } // namespace

    // ============================================================
    // PrefabAsset
    // ============================================================

    std::expected<PrefabAsset, EngineError>
    PrefabAsset::CaptureFromEntity(ECSRegistry& registry, Entity entity)
    {
        if (!registry.IsValid(entity))
        {
            return MakeError(EngineErrorCode::EntityNotFound,
                "Cannot capture an invalid entity into a prefab", "PrefabAsset");
        }

        const SerializeOptions options = PrefabCaptureOptions();

        auto compsResult = SerializeEntityComponents(registry, entity, options);
        if (!compsResult)
        {
            return std::unexpected(compsResult.error());
        }

        PrefabAsset asset;
        asset.name = registry.GetEntityName(entity);
        asset.componentsData = std::move(compsResult.value());

        // Phase 5: descendants, parents first. Each parent is guaranteed to already
        // have an index because GetDescendants is a pre-order traversal.
        std::unordered_map<EntityID, int> indexOf{ { entity.id, 0 } };
        for (Entity descendant : GetDescendants(registry, entity))
        {
            auto childComps = SerializeEntityComponents(registry, descendant, options);
            if (!childComps)
            {
                return std::unexpected(childComps.error());
            }
            auto parentIt = indexOf.find(GetParent(registry, descendant).id);
            if (parentIt == indexOf.end())
            {
                // Can't happen for a pre-order traversal of a valid tree; guards
                // against a cycle written directly into HierarchyComponent.
                return MakeError(EngineErrorCode::InvalidState,
                    "Cannot capture prefab: entity " + std::to_string(descendant.id) +
                    "'s parent was not visited before it (broken hierarchy)", "PrefabAsset");
            }

            PrefabChildEntity child;
            child.name = registry.GetEntityName(descendant);
            child.parentIndex = parentIt->second;
            child.componentsData = std::move(childComps.value());
            asset.children.push_back(std::move(child));
            indexOf[descendant.id] = static_cast<int>(asset.children.size());
        }

        asset.version = asset.children.empty() ? 1u : 2u;
        return asset;
    }

    std::expected<Entity, EngineError>
    PrefabAsset::SpawnInto(ECSRegistry& registry) const
    {
        auto spawned = SpawnHierarchyInto(registry);
        if (!spawned)
        {
            return std::unexpected(spawned.error());
        }
        return spawned->front();
    }

    std::expected<std::vector<Entity>, EngineError>
    PrefabAsset::SpawnHierarchyInto(ECSRegistry& registry) const
    {
        std::vector<Entity> entities;
        entities.reserve(EntityCount());
        for (size_t i = 0; i < EntityCount(); ++i)
        {
            entities.push_back(registry.CreateEntity());
        }

        auto applyResult = ApplyHierarchyTo(registry, entities);
        if (!applyResult)
        {
            // §2.7: SpawnInto's atomicity is a *separate* contract from
            // ApplyToEntity's strong guarantee — brand new entities can simply be
            // destroyed on failure, unlike an existing one. Children first.
            for (auto it = entities.rbegin(); it != entities.rend(); ++it)
            {
                registry.DestroyEntity(*it);
            }
            return std::unexpected(applyResult.error());
        }

        return entities;
    }

    std::expected<void, EngineError>
    PrefabAsset::ApplyHierarchyTo(ECSRegistry& registry, const std::vector<Entity>& entities) const
    {
        if (entities.size() != EntityCount())
        {
            return MakeError(EngineErrorCode::InvalidParameter,
                "ApplyHierarchyTo: expected " + std::to_string(EntityCount()) + " entities, got " +
                std::to_string(entities.size()), "PrefabAsset");
        }

        auto rootResult = ApplyToEntity(registry, entities[0]);
        if (!rootResult)
        {
            return rootResult;
        }
        if (!name.empty())
        {
            // Before Phase 5 spawned instances had no name and showed up as
            // "Entity_<id>" in the Scene Hierarchy. With children that tree is
            // unreadable, so the root gets the prefab's name.
            registry.SetEntityName(entities[0], name);
        }

        for (size_t i = 0; i < children.size(); ++i)
        {
            const PrefabChildEntity& child = children[i];
            const Entity entity = entities[i + 1];
            if (child.parentIndex < 0 || static_cast<size_t>(child.parentIndex) > i)
            {
                return MakeError(EngineErrorCode::InvalidParameter,
                    "Prefab child " + std::to_string(i + 1) + " has invalid parent index " +
                    std::to_string(child.parentIndex), "PrefabAsset");
            }

            auto childResult = ApplyComponentsWithRollback(registry, entity, child.componentsData);
            if (!childResult)
            {
                return childResult;
            }
            if (!child.name.empty())
            {
                registry.SetEntityName(entity, child.name);
            }
            auto parentResult = SetParent(registry, entity, entities[static_cast<size_t>(child.parentIndex)]);
            if (!parentResult)
            {
                return parentResult;
            }
        }
        return {};
    }

    std::expected<void, EngineError>
    PrefabAsset::ApplyToEntity(ECSRegistry& registry, Entity entity) const
    {
        if (!registry.IsValid(entity))
        {
            return MakeError(EngineErrorCode::EntityNotFound,
                "Cannot apply a prefab to an invalid entity", "PrefabAsset");
        }

        return ApplyComponentsWithRollback(registry, entity, componentsData);
    }

    nlohmann::json PrefabAsset::ToJson() const
    {
        nlohmann::json j;
        j["name"] = name;
        if (children.empty())
        {
            // Single entity: keep writing v1 so files stay readable by older builds.
            j["version"] = version;
            j["components"] = componentsData;
            return j;
        }

        // v2 (Phase 5): "entities"[0] is the root; "parent" is an index into the array.
        j["version"] = 2;
        nlohmann::json entities = nlohmann::json::array();
        entities.push_back({ { "name", name }, { "components", componentsData } });
        for (const PrefabChildEntity& child : children)
        {
            entities.push_back({ { "name", child.name }, { "parent", child.parentIndex },
                                 { "components", child.componentsData } });
        }
        j["entities"] = std::move(entities);
        return j;
    }

    // v2 (Phase 5): {"version": 2, "name", "entities": [{"name","components"}, {"name","parent","components"}, ...]}
    static std::expected<PrefabAsset, EngineError> FromJsonV2(const nlohmann::json& json)
    {
        auto malformed = [](const std::string& why) {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Malformed prefab JSON (v2 'entities'): " + why, "PrefabAsset");
        };

        const nlohmann::json& entities = json["entities"];
        if (!entities.is_array() || entities.empty())
        {
            return malformed("'entities' must be a non-empty array");
        }

        PrefabAsset asset;
        asset.version = 2;
        asset.name = json.value("name", std::string());
        for (size_t i = 0; i < entities.size(); ++i)
        {
            const nlohmann::json& e = entities[i];
            const std::string where = "entity " + std::to_string(i);
            if (!e.is_object() || !e.contains("components") || !e["components"].is_object())
            {
                return malformed(where + " must be an object with a 'components' object");
            }
            if (i == 0)
            {
                if (e.contains("parent"))
                {
                    return malformed("entity 0 is the root and must not have a 'parent'");
                }
                asset.componentsData = e["components"];
                continue;
            }
            if (!e.contains("parent") || !e["parent"].is_number_integer())
            {
                return malformed(where + " needs an integer 'parent' index");
            }
            const auto parent = e["parent"].get<nlohmann::json::number_integer_t>();
            // "부모가 항상 앞에 온다" 규칙이 손으로 고친 파일의 순환을 원천적으로 막는다.
            if (parent < 0 || static_cast<size_t>(parent) >= i)
            {
                return malformed(where + " has parent " + std::to_string(parent) +
                                 "; a parent must come earlier in the array");
            }
            PrefabChildEntity child;
            child.name = e.value("name", std::string());
            child.parentIndex = static_cast<int>(parent);
            child.componentsData = e["components"];
            asset.children.push_back(std::move(child));
        }
        return asset;
    }

    std::expected<PrefabAsset, EngineError>
    PrefabAsset::FromJson(const nlohmann::json& json)
    {
        if (json.is_object() && json.contains("entities"))
        {
            return FromJsonV2(json);
        }

        if (!json.is_object() || !json.contains("version") || !json.contains("components"))
        {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Malformed prefab JSON: missing required top-level 'version'/'components' fields",
                "PrefabAsset");
        }

        if (!json["version"].is_number_integer() && !json["version"].is_number_unsigned())
        {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Malformed prefab JSON: 'version' must be an integer", "PrefabAsset");
        }

        // is_number_integer() is also true for negative values (e.g. -1) — without
        // this check, the .get<uint32_t>() below would silently wrap a negative
        // version into a huge nonsensical one (-1 -> 4294967295) instead of being
        // rejected as malformed.
        if (json["version"].is_number_integer() && json["version"].get<nlohmann::json::number_integer_t>() < 0)
        {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Malformed prefab JSON: 'version' must not be negative", "PrefabAsset");
        }

        if (!json["components"].is_object())
        {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Malformed prefab JSON: 'components' must be an object", "PrefabAsset");
        }

        PrefabAsset asset;
        asset.version = json["version"].get<uint32_t>();
        asset.name = json.value("name", std::string());
        asset.componentsData = json["components"];
        return asset;
    }

    std::expected<void, EngineError>
    PrefabAsset::SaveToFile(const std::filesystem::path& path) const
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            return MakeError(EngineErrorCode::FileNotFound,
                "Failed to open '" + path.string() + "' for writing", "PrefabAsset");
        }

        file << ToJson().dump(4);
        return {};
    }

    std::expected<PrefabAsset, EngineError>
    PrefabAsset::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return MakeError(EngineErrorCode::FileNotFound,
                "Prefab file not found: '" + path.string() + "'", "PrefabAsset");
        }

        nlohmann::json parsed;
        try
        {
            file >> parsed;
        }
        catch (const std::exception& e)
        {
            return MakeError(EngineErrorCode::ResourceLoadFailed,
                "Failed to parse prefab JSON '" + path.string() + "': " + e.what(), "PrefabAsset");
        }

        return FromJson(parsed);
    }
}
