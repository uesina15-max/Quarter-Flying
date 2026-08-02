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

    registry_->SetTransformRotation(entity_, newRotation_.x, newRotation_.y, newRotation_.z);
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

    registry_->SetTransformRotation(entity_, oldRotation_.x, oldRotation_.y, oldRotation_.z);
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
