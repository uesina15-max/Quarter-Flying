#include "DestroyEntityCommand.h"
#include "../../ecs/ECSRegistry.h"

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

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "DestroyEntityCommand");
    }

    // 최초 Apply: 대상 + 자손 스냅샷. Redo에서도 같은 스냅샷을 쓴다(Undo가 같은 UUID로 되살리므로).
    if (!snapshotCaptured_)
    {
        snapshots_ = CaptureSubtreeSnapshots(*registry_, entity_);
        snapshotCaptured_ = true;
    }

    // Redo 시점의 런타임 엔티티는 UUID로 찾는다(Undo가 새 런타임 id로 되살렸다).
    std::vector<Entity> toDestroy;
    toDestroy.reserve(snapshots_.size());
    for (const EntitySnapshot& snapshot : snapshots_)
    {
        toDestroy.push_back(registry_->GetEntityByUUID(snapshot.uuid));
    }
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
    return {};
}

} // namespace Engine
