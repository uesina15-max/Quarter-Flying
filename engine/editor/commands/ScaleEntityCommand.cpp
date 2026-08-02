#include "ScaleEntityCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Components.h"

namespace Engine
{

ScaleEntityCommand::ScaleEntityCommand(ECSRegistry* registry, Entity entity, Vec3 newScale)
    : registry_(registry)
    , entity_(entity)
    , newScale_(newScale)
{
    if (registry_ && registry_->HasTransformComponent(entity_))
    {
        if (const TransformComponent* transform = registry_->GetTransformComponent(entity_))
        {
            oldScale_ = transform->scale;
        }
    }
}

std::expected<void, EngineError> ScaleEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "ScaleEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "ScaleEntityCommand");
    }
    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Stale entity handle", "ScaleEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "ScaleEntityCommand");
    }

    registry_->SetTransformScale(entity_, newScale_.x, newScale_.y, newScale_.z);
    return {};
}

std::expected<void, EngineError> ScaleEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "ScaleEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "ScaleEntityCommand");
    }
    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Stale entity handle", "ScaleEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "ScaleEntityCommand");
    }

    registry_->SetTransformScale(entity_, oldScale_.x, oldScale_.y, oldScale_.z);
    return {};
}

bool ScaleEntityCommand::CanMergeWith(const ICommand& other) const
{
    const auto* otherScale = dynamic_cast<const ScaleEntityCommand*>(&other);
    return otherScale != nullptr && otherScale->entity_.id == entity_.id;
}

std::expected<void, EngineError> ScaleEntityCommand::MergeWith(const ICommand& other)
{
    const auto* otherScale = dynamic_cast<const ScaleEntityCommand*>(&other);
    if (!otherScale)
    {
        return MakeError(EngineErrorCode::OperationFailed, "Not mergeable", "ScaleEntityCommand");
    }

    // oldScale_ is intentionally left unchanged — preserves the original pre-session value
    newScale_ = otherScale->newScale_;
    return {};
}

} // namespace Engine
