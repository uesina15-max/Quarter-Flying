#include "CreateEntityCommand.h"
#include "../../ecs/ECSRegistry.h"

namespace Engine
{

CreateEntityCommand::CreateEntityCommand(ECSRegistry* registry, std::string entityName)
    : registry_(registry)
    , entityName_(std::move(entityName))
    , createdEntity_()
    , savedUUID_(0)
{
}

std::expected<void, EngineError> CreateEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "CreateEntityCommand");
    }

    if (!applied_)
    {
        // 최초 Apply: 새 Entity 생성 후 UUID 저장
        createdEntity_ = registry_->CreateEntity();
        savedUUID_     = registry_->GetUUID(createdEntity_);
        applied_       = true;
    }
    else
    {
        // Redo: 동일한 UUID로 Entity 복원
        createdEntity_ = registry_->CreateEntityWithUUID(savedUUID_);
    }

    if (!registry_->IsValid(createdEntity_))
    {
        return MakeError(EngineErrorCode::OperationFailed, "Failed to create entity", "CreateEntityCommand");
    }

    if (!entityName_.empty())
    {
        registry_->SetEntityName(createdEntity_, entityName_);
    }

    return {};
}

std::expected<void, EngineError> CreateEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "CreateEntityCommand");
    }

    if (!registry_->IsValid(createdEntity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Entity is no longer valid", "CreateEntityCommand");
    }

    registry_->DestroyEntity(createdEntity_);
    return {};
}

} // namespace Engine
