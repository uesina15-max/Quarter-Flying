#include "RevertPrefabInstanceCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../prefab/PrefabInstanceComponent.h"
#include "../../ecs/Hierarchy.h"
#include "../../core/Utf8Path.h"
#include "../../core/ProjectPath.h"

namespace Engine
{

RevertPrefabInstanceCommand::RevertPrefabInstanceCommand(ECSRegistry* registry, Entity entity, std::string projectRoot)
    : registry_(registry)
    , entity_(entity)
    , entityUUID_(registry && registry->IsValid(entity) ? registry->GetUUID(entity) : UUID(0))
    , projectRoot_(std::move(projectRoot))
{
}

std::expected<void, EngineError> RevertPrefabInstanceCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RevertPrefabInstanceCommand");
    }

    // Stop recreates runtime IDs; the old handle made Undo fail after preview.
    entity_ = registry_->GetEntityByUUID(entityUUID_);
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
        // prefabPath는 프로젝트 상대 경로(씬 version 2) 또는 옛 절대 경로다. 해석 규칙은 core/ProjectPath.h 한 곳.
        const std::filesystem::path resolved = ResolveProjectPath(meta->prefabPath, projectRoot_);
        auto loaded = PrefabAsset::LoadFromFile(resolved, projectRoot_);
        if (!loaded)
        {
            // 어느 단계가 틀렸는지 가를 수 있게 저장된 값·기준 폴더·최종 경로를 모두 남긴다(상대 경로인데 기준 폴더가
            // 비었거나 다른 프로젝트면 최종 경로가 엉뚱한 곳을 가리킨다).
            EngineError err = loaded.error();
            err.message += " (stored prefabPath='" + meta->prefabPath + "', project root='" + projectRoot_ +
                           "', resolved='" + Utf8FromPath(resolved) + "')";
            return std::unexpected(err);
        }
        loadedAsset_ = std::move(loaded.value());

        // Full-state snapshot (default SerializeOptions — same as ApplyToEntity's
        // own internal rollback snapshot) captured BEFORE the revert mutates
        // anything, so Undo can restore exactly this.
        auto snapshotResult = SerializeEntityComponents(*registry_, entity_, SerializeOptions{});
        preRevertSnapshot_ = snapshotResult ? snapshotResult.value() : nlohmann::json::object();
        preRevertName_ = registry_->GetEntityName(entity_);
        if (!loadedAsset_->children.empty())
        {
            auto subtree = CaptureSubtreeSnapshots(*registry_, entity_);
            preRevertDescendants_.assign(subtree.begin() + 1, subtree.end());
        }

        applied_ = true;
    }

    if (loadedAsset_->children.empty())
    {
        // loadedAsset_->ApplyToEntity already carries its own strong guarantee
        // (plan §2.4): on failure the entity is left exactly as it was before this
        // call, so there's nothing extra to roll back here.
        return loadedAsset_->ApplyToEntity(*registry_, entity_);
    }

    // Multi-entity prefab (Phase 5): root first (strong guarantee), then replace
    // the descendants with the prefab's children.
    auto rootResult = loadedAsset_->ApplyToEntity(*registry_, entity_);
    if (!rootResult)
    {
        return rootResult;
    }
    DestroyEntitiesReverse(*registry_, GetDescendants(*registry_, entity_));

    const bool firstApply = spawnedChildUUIDs_.empty();
    std::vector<Entity> entities{ entity_ };
    for (size_t i = 0; i < loadedAsset_->children.size(); ++i)
    {
        Entity child = firstApply ? registry_->CreateEntity()
                                  : registry_->CreateEntityWithUUID(spawnedChildUUIDs_[i]);
        entities.push_back(child);
        if (firstApply)
        {
            spawnedChildUUIDs_.push_back(registry_->GetUUID(child));
        }
    }

    auto hierarchyResult = loadedAsset_->ApplyHierarchyTo(*registry_, entities);
    if (!hierarchyResult)
    {
        // Put everything back the way Undo would, then report the original error.
        DestroyEntitiesReverse(*registry_, std::vector<Entity>(entities.begin() + 1, entities.end()));
        spawnedChildUUIDs_.clear();
        PrefabAsset restoreAsset;
        restoreAsset.componentsData = preRevertSnapshot_;
        (void)restoreAsset.ApplyToEntity(*registry_, entity_);
        registry_->SetEntityName(entity_, preRevertName_);
        (void)RestoreEntitySnapshots(*registry_, preRevertDescendants_);
        return hierarchyResult;
    }
    return {};
}

std::expected<void, EngineError> RevertPrefabInstanceCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RevertPrefabInstanceCommand");
    }

    // Stop recreates runtime IDs; the old handle made Undo fail after preview.
    entity_ = registry_->GetEntityByUUID(entityUUID_);
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
    if (!loadedAsset_->children.empty())
    {
        std::vector<Entity> spawnedChildren;
        for (UUID uuid : spawnedChildUUIDs_)
        {
            spawnedChildren.push_back(registry_->GetEntityByUUID(uuid));
        }
        DestroyEntitiesReverse(*registry_, spawnedChildren);
    }

    PrefabAsset restoreAsset;
    restoreAsset.componentsData = preRevertSnapshot_;
    auto rootResult = restoreAsset.ApplyToEntity(*registry_, entity_);
    if (!rootResult)
    {
        return rootResult;
    }

    if (!loadedAsset_->children.empty())
    {
        // ApplyHierarchyTo renamed the root to the prefab's name (only the multi-entity path does).
        registry_->SetEntityName(entity_, preRevertName_);
        auto restored = RestoreEntitySnapshots(*registry_, preRevertDescendants_);
        if (!restored)
        {
            return std::unexpected(restored.error());
        }
    }
    return {};
}

} // namespace Engine
