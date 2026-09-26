#include "RevertPrefabInstanceCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../prefab/PrefabInstanceComponent.h"

namespace Engine
{

RevertPrefabInstanceCommand::RevertPrefabInstanceCommand(ECSRegistry* registry, Entity entity)
    : registry_(registry)
    , entity_(entity)
{
}

std::expected<void, EngineError> RevertPrefabInstanceCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RevertPrefabInstanceCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "RevertPrefabInstanceCommand");
    }

    if (!applied_)
    {
        // First Apply: resolve the prefab file, load it once, and snapshot the
        // entity's current state (for Undo) before touching anything.
        PrefabInstanceComponent* meta = registry_->GetComponent<PrefabInstanceComponent>(entity_);
        if (!meta)
        {
            return MakeError(EngineErrorCode::ComponentNotFound,
                "Entity is not a prefab instance (no PrefabInstanceComponent)", "RevertPrefabInstanceCommand");
        }

        // §2.9: if the source file is missing/unreadable, the entity must be left
        // completely untouched — LoadFromFile failing here means we return before
        // ApplyToEntity is ever called.
        auto loaded = PrefabAsset::LoadFromFile(meta->prefabPath);
        if (!loaded)
        {
            return std::unexpected(loaded.error());
        }
        loadedAsset_ = std::move(loaded.value());

        // Full-state snapshot (default SerializeOptions — same as ApplyToEntity's
        // own internal rollback snapshot) captured BEFORE the revert mutates
        // anything, so Undo can restore exactly this.
        auto snapshotResult = SerializeEntityComponents(*registry_, entity_, SerializeOptions{});
        preRevertSnapshot_ = snapshotResult ? snapshotResult.value() : nlohmann::json::object();

        applied_ = true;
    }

    // loadedAsset_->ApplyToEntity already carries its own strong guarantee
    // (plan §2.4): on failure the entity is left exactly as it was before this
    // call, so there's nothing extra to roll back here.
    return loadedAsset_->ApplyToEntity(*registry_, entity_);
}

std::expected<void, EngineError> RevertPrefabInstanceCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RevertPrefabInstanceCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "RevertPrefabInstanceCommand");
    }

    if (!applied_)
    {
        return MakeError(EngineErrorCode::InvalidState, "Nothing to undo", "RevertPrefabInstanceCommand");
    }

    // Reuse ApplyToEntity's Definition A + strong guarantee machinery by
    // wrapping the pre-revert snapshot in a throwaway PrefabAsset — the exact
    // same "sync entity to a target component-set JSON" primitive the revert
    // itself used, just retargeted at the snapshot instead of the prefab file.
    PrefabAsset restoreAsset;
    restoreAsset.componentsData = preRevertSnapshot_;
    return restoreAsset.ApplyToEntity(*registry_, entity_);
}

} // namespace Engine
