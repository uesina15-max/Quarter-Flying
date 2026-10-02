#include "InstantiatePrefabCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../prefab/PrefabInstanceComponent.h"
#include "EntitySnapshot.h"

namespace Engine
{

InstantiatePrefabCommand::InstantiatePrefabCommand(ECSRegistry* registry,
                                                     std::filesystem::path prefabPath,
                                                     std::optional<Vec3> position)
    : registry_(registry)
    , prefabPath_(std::move(prefabPath))
    , position_(position)
    , spawnedEntity_()
{
}

std::expected<void, EngineError> InstantiatePrefabCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "InstantiatePrefabCommand");
    }

    if (!applied_)
    {
        // First Apply: load the prefab file once and spawn a brand new entity.
        // §2.7: SpawnInto is atomic — on failure nothing is left in the registry,
        // so we can just propagate the error as-is.
        auto loaded = PrefabAsset::LoadFromFile(prefabPath_);
        if (!loaded)
        {
            return std::unexpected(loaded.error());
        }
        loadedAsset_ = std::move(loaded.value());

        auto spawned = loadedAsset_->SpawnHierarchyInto(*registry_);
        if (!spawned)
        {
            return std::unexpected(spawned.error());
        }

        spawnedEntity_ = spawned->front();
        savedUUIDs_.clear();
        for (Entity e : spawned.value())
        {
            savedUUIDs_.push_back(registry_->GetUUID(e));
        }
        applied_       = true;
    }
    else
    {
        // Redo: recreate the entity with the same UUID (CreateEntityCommand's
        // pattern), then re-apply the SAME already-loaded PrefabAsset via
        // ApplyToEntity (its strong guarantee, plan §2.4, means a failure here
        // leaves the freshly (re)created — and thus still bare — entity exactly
        // as it was, which we then destroy below).
        std::vector<Entity> recreated;
        for (UUID uuid : savedUUIDs_)
        {
            Entity e = registry_->CreateEntityWithUUID(uuid);
            if (!registry_->IsValid(e))
            {
                DestroyEntitiesReverse(*registry_, recreated);
                return MakeError(EngineErrorCode::OperationFailed,
                    "Failed to recreate entity for prefab instantiation redo", "InstantiatePrefabCommand");
            }
            recreated.push_back(e);
        }
        spawnedEntity_ = recreated.front();

        auto applyResult = loadedAsset_->ApplyHierarchyTo(*registry_, recreated);
        if (!applyResult)
        {
            DestroyEntitiesReverse(*registry_, recreated);
            return std::unexpected(applyResult.error());
        }
    }

    // §2.3/§2.8: tag the entity as a prefab instance. Applied on both the first
    // Apply and every Redo, since ApplyToEntity's Definition A treats
    // PrefabInstanceComponent as a removal exemption but doesn't add it itself.
    // Guarded with HasComponent rather than an unconditional AddComponent in
    // case a hand-edited prefab file's componentsData already embeds one
    // (ApplyToEntity would have applied it) — AddComponent on an entity that
    // already has the component would be a bug (ECS asserts / silently
    // corrupts the dense array in Release), not just redundant.
    if (!registry_->HasComponent<PrefabInstanceComponent>(spawnedEntity_))
    {
        registry_->AddComponent(spawnedEntity_, PrefabInstanceComponent{});
    }
    if (auto* meta = registry_->GetComponent<PrefabInstanceComponent>(spawnedEntity_))
    {
        meta->prefabPath          = prefabPath_.string();
        meta->sourcePrefabVersion = loadedAsset_->version;
    }

    // §2.8: position argument, when given, overrides only the position — the
    // prefab's own rotation/scale are left exactly as SpawnInto/ApplyToEntity
    // already applied them.
    if (position_.has_value())
    {
        registry_->SetTransformPosition(spawnedEntity_, position_->x, position_->y, position_->z);
    }

    return {};
}

std::expected<void, EngineError> InstantiatePrefabCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "InstantiatePrefabCommand");
    }

    if (!registry_->IsValid(spawnedEntity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Entity is no longer valid", "InstantiatePrefabCommand");
    }

    std::vector<Entity> spawned;
    for (UUID uuid : savedUUIDs_)
    {
        spawned.push_back(registry_->GetEntityByUUID(uuid));
    }
    DestroyEntitiesReverse(*registry_, spawned);
    return {};
}

} // namespace Engine
