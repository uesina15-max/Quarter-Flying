#include "DestroyEntityCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Reflection.h"

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

    // 최초 Apply: 스냅샷 캡처
    if (!snapshotCaptured_)
    {
        snapshot_.uuid = registry_->GetUUID(entity_);
        snapshot_.name = registry_->GetEntityName(entity_);

        // ComponentRegistry를 순회하며 모든 컴포넌트 직렬화
        nlohmann::json componentData;
        for (auto& [name, info] : ComponentRegistry::GetAllComponents())
        {
            info.serialize(*registry_, entity_, componentData);
        }
        snapshot_.componentData = std::move(componentData);
        snapshotCaptured_ = true;
    }

    registry_->DestroyEntity(entity_);
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

    // 저장된 UUID로 Entity 복원
    Entity newEntity = registry_->CreateEntityWithUUID(snapshot_.uuid);
    if (!registry_->IsValid(newEntity))
    {
        return MakeError(EngineErrorCode::OperationFailed, "Failed to recreate entity", "DestroyEntityCommand");
    }

    // 이름 복원
    registry_->SetEntityName(newEntity, snapshot_.name);

    // 각 컴포넌트를 역직렬화로 복원
    for (auto& [name, info] : ComponentRegistry::GetAllComponents())
    {
        if (snapshot_.componentData.contains(name))
        {
            info.deserialize(*registry_, newEntity, snapshot_.componentData[name]);
        }
    }

    // 복원된 entity를 추후 Redo에서 재삭제할 수 있도록 갱신
    entity_ = newEntity;
    return {};
}

} // namespace Engine
