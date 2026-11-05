#include "DestroyEntityCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Reflection.h"
#include <unordered_set>

namespace Engine
{

DestroyEntityCommand::DestroyEntityCommand(ECSRegistry* registry, Entity entity)
    : registry_(registry)
    , entity_(entity)
{
}

std::expected<void, EngineError> DestroyEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "DestroyEntityCommand");
    }

    // Redo may follow Stop, which invalidates the last restored runtime ID.
    if (snapshotCaptured_ && !snapshots_.empty())
        entity_ = registry_->GetEntityByUUID(snapshots_.front().uuid);
    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "DestroyEntityCommand");
    }

    // 최초 Apply: 대상 + 자손 스냅샷. Redo에서도 같은 스냅샷을 쓴다(Undo가 같은 UUID로 되살리므로).
    if (!snapshotCaptured_)
    {
        snapshots_ = CaptureSubtreeSnapshots(*registry_, entity_);
        // Deleting a camera target used to leave its old runtime ID in cameras
        // outside the subtree. Undo recreated the UUID with a new ID, so the
        // restored object existed but cameras no longer followed it (target=0
        // when serialized). Preserve inbound EntityRef fields by UUID as well.
        std::unordered_set<uint64_t> deleted;
        for (const auto& snapshot : snapshots_)
            deleted.insert(snapshot.uuid.GetValue());
        for (Entity owner : registry_->GetAllEntities())
        {
            const UUID ownerUUID = registry_->GetUUID(owner);
            if (deleted.contains(ownerUUID.GetValue())) continue;
            for (const auto& [name, info] : ComponentRegistry::GetAllComponents())
            {
                if (!info.hasComponent || !info.serialize || !info.patchField ||
                    !info.hasComponent(*registry_, owner)) continue;
                nlohmann::json data;
                info.serialize(*registry_, owner, data);
                if (!data.contains(name)) continue;
                for (const auto& field : info.fields)
                {
                    if (field.type != FieldType::EntityRef || !data[name].contains(field.name)) continue;
                    const auto uuid = data[name][field.name].get<uint64_t>();
                    if (deleted.contains(uuid))
                        externalReferences_.push_back({ownerUUID, name, field.name, UUID(uuid)});
                }
            }
        }
        snapshotCaptured_ = true;
    }

    // Redo 시점의 런타임 엔티티는 UUID로 찾는다(Undo가 새 런타임 id로 되살렸다).
    std::vector<Entity> toDestroy;
    toDestroy.reserve(snapshots_.size());
    for (const EntitySnapshot& snapshot : snapshots_)
    {
        toDestroy.push_back(registry_->GetEntityByUUID(snapshot.uuid));
    }
    WriteExternalReferences(false);
    DestroyEntitiesReverse(*registry_, toDestroy);
    return {};
}

std::expected<void, EngineError> DestroyEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "DestroyEntityCommand");
    }

    if (!snapshotCaptured_)
    {
        return MakeError(EngineErrorCode::InvalidState, "No snapshot available to restore", "DestroyEntityCommand");
    }

    auto restored = RestoreEntitySnapshots(*registry_, snapshots_);
    if (!restored)
    {
        return std::unexpected(restored.error());
    }

    // 복원된 entity를 추후 Redo에서 재삭제할 수 있도록 갱신
    entity_ = restored->front();
    WriteExternalReferences(true);
    return {};
}

void DestroyEntityCommand::WriteExternalReferences(bool restore)
{
    for (const auto& reference : externalReferences_)
    {
        const Entity owner = registry_->GetEntityByUUID(reference.owner);
        const ComponentInfo* info = ComponentRegistry::GetComponentInfo(reference.component);
        if (!registry_->IsValid(owner) || !info || !info->hasComponent || !info->patchField ||
            !info->hasComponent(*registry_, owner)) continue;
        info->patchField(*registry_, owner, StringHash(reference.field.c_str()),
                         nlohmann::json(restore ? reference.target.GetValue() : uint64_t(0)));
    }
}

} // namespace Engine
