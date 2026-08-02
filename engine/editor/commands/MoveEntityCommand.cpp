#include "MoveEntityCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Components.h"

namespace Engine
{

MoveEntityCommand::MoveEntityCommand(ECSRegistry* registry, Entity entity, Vec3 newPosition)
    : registry_(registry)
    , entity_(entity)
    , newPosition_(newPosition)
{
    if (registry_ && registry_->HasTransformComponent(entity_))
    {
        if (const TransformComponent* transform = registry_->GetTransformComponent(entity_))
        {
            oldPosition_ = transform->position;
        }
    }
}

std::expected<void, EngineError> MoveEntityCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "MoveEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "MoveEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "MoveEntityCommand");
    }

    registry_->SetTransformPosition(entity_, newPosition_.x, newPosition_.y, newPosition_.z);
    return {};
}

std::expected<void, EngineError> MoveEntityCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "MoveEntityCommand");
    }
    if (!entity_.IsValid())
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "MoveEntityCommand");
    }
    if (!registry_->HasTransformComponent(entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound, "Entity has no TransformComponent", "MoveEntityCommand");
    }

    registry_->SetTransformPosition(entity_, oldPosition_.x, oldPosition_.y, oldPosition_.z);
    return {};
}

bool MoveEntityCommand::CanMergeWith(const ICommand& other) const
{
    const auto* otherMove = dynamic_cast<const MoveEntityCommand*>(&other);
    return otherMove != nullptr && otherMove->entity_.id == entity_.id;
}

std::expected<void, EngineError> MoveEntityCommand::MergeWith(const ICommand& other)
{
    const auto* otherMove = dynamic_cast<const MoveEntityCommand*>(&other);
    if (!otherMove)
    {
        return MakeError(EngineErrorCode::OperationFailed, "Not mergeable", "MoveEntityCommand");
    }

    newPosition_ = otherMove->newPosition_;
    return {};
}

} // namespace Engine
