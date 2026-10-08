#include "DuplicateEntityCommand.h"
#include "../../ecs/ECSRegistry.h"

namespace Engine
{

DuplicateEntityCommand::DuplicateEntityCommand(ECSRegistry* registry, Entity source)
    : registry_(registry)
    , source_(source)
{
}

std::expected<void, EngineError> DuplicateEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "DuplicateEntityCommand");
    }

    // 최초 Apply: 원본(+자손)을 새 UUID로 재매핑한 복제 스냅샷을 만든다. Redo에서도 같은 스냅샷을
    // 쓴다(Undo가 같은 UUID로 파괴하므로). 원본이 그 사이 바뀌어도 복제본은 최초 시점 그대로.
    if (!prepared_)
    {
        if (!registry_->IsValid(source_))
        {
            return MakeError(EngineErrorCode::EntityNotFound, "Source entity is not valid", "DuplicateEntityCommand");
        }
        snapshots_ = MakeDuplicateSnapshots(*registry_, source_);
        prepared_  = true;
    }

    auto created = RestoreEntitySnapshots(*registry_, snapshots_);
    if (!created)
    {
        return std::unexpected(created.error());
    }
    duplicatedRoot_ = created->front();
    return {};
}

std::expected<void, EngineError> DuplicateEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "DuplicateEntityCommand");
    }

    if (!prepared_)
    {
        return MakeError(EngineErrorCode::InvalidState, "No duplicate to undo", "DuplicateEntityCommand");
    }

    // 복제본은 고정된 새 UUID를 가지므로 런타임 id가 바뀌어도 UUID로 다시 찾는다(DestroyEntityCommand와 동일).
    std::vector<Entity> toDestroy;
    toDestroy.reserve(snapshots_.size());
    for (const EntitySnapshot& snapshot : snapshots_)
    {
        toDestroy.push_back(registry_->GetEntityByUUID(snapshot.uuid));
    }
    DestroyEntitiesReverse(*registry_, toDestroy);
    return {};
}

} // namespace Engine
