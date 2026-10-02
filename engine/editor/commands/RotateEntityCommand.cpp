#include "RotateEntityCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Components.h"

namespace Engine
{

RotateEntityCommand::RotateEntityCommand(ECSRegistry* registry, Entity entity, Quaternion newRotation)
    : registry_(registry)
    , entity_(entity)
    , newRotation_(newRotation)
{
    if (registry_ && registry_->HasTransformComponent(entity_))
    {
        if (const TransformComponent* transform = registry_->GetTransformComponent(entity_))
        {
            oldRotation_ = transform->rotation;
        }
    }
}

std::expected<void, EngineError> RotateEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RotateEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "RotateEntityCommand");
    }
    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Stale entity handle", "RotateEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "RotateEntityCommand");
    }

    // 쿼터니언을 그대로 대입한다. 예전에는 SetTransformRotation(x, y, z)로 적용해서 w가 버려졌다
    // (당시 그 함수는 w=1로 저장). 그래서 Apply/Undo가 회전을 정확히 복원하지 못했다. 지금
    // SetTransformRotation은 오일러 각(도)을 받으므로 쿼터니언 성분을 넘기면 더더욱 틀린다.
    registry_->GetTransformComponent(entity_)->rotation = newRotation_;
    return {};
}

std::expected<void, EngineError> RotateEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RotateEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "RotateEntityCommand");
    }
    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Stale entity handle", "RotateEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "RotateEntityCommand");
    }

    registry_->GetTransformComponent(entity_)->rotation = oldRotation_;  // Apply() 주석 참고
    return {};
}

bool RotateEntityCommand::CanMergeWith(const ICommand& other) const
{
    const auto* otherRotate = dynamic_cast<const RotateEntityCommand*>(&other);
    return otherRotate != nullptr && otherRotate->entity_.id == entity_.id;
}

std::expected<void, EngineError> RotateEntityCommand::MergeWith(const ICommand& other)
{
    const auto* otherRotate = dynamic_cast<const RotateEntityCommand*>(&other);
    if (!otherRotate)
    {
        return MakeError(EngineErrorCode::OperationFailed, "Not mergeable", "RotateEntityCommand");
    }

    // oldRotation_ is intentionally left unchanged — preserves the original pre-session value
    newRotation_ = otherRotate->newRotation_;
    return {};
}

} // namespace Engine
