#include "RemoveComponentCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Reflection.h"

namespace Engine
{

RemoveComponentCommand::RemoveComponentCommand(ECSRegistry* registry, Entity entity, std::string componentType)
    : registry_(registry)
    , entity_(entity)
    , componentType_(std::move(componentType))
{
}

std::expected<void, EngineError> RemoveComponentCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RemoveComponentCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "RemoveComponentCommand");
    }

    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentType_);
    if (!info)
    {
        return MakeError(EngineErrorCode::InvalidComponentType, "Unknown component type: " + componentType_, "RemoveComponentCommand");
    }

    if (info->hasComponent && !info->hasComponent(*registry_, entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound,
                         "Entity does not have component: " + componentType_, "RemoveComponentCommand");
    }

    // Capture snapshot on first Apply only
    if (!snapshotCaptured_)
    {
        if (info->serialize)
        {
            info->serialize(*registry_, entity_, snapshot_);
            // snapshot_ now contains { "ComponentType": { ...fields... } }
            // Narrow it to just the component's data for deserialize
            if (snapshot_.contains(componentType_))
            {
                snapshot_ = snapshot_[componentType_];
            }
        }
        snapshotCaptured_ = true;
    }

    if (info->remove)
    {
        info->remove(*registry_, entity_);
    }

    return {};
}

std::expected<void, EngineError> RemoveComponentCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "RemoveComponentCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "RemoveComponentCommand");
    }

    if (!snapshotCaptured_)
    {
        return MakeError(EngineErrorCode::InvalidState, "No snapshot available to restore", "RemoveComponentCommand");
    }

    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentType_);
    if (!info)
    {
        return MakeError(EngineErrorCode::InvalidComponentType, "Unknown component type: " + componentType_, "RemoveComponentCommand");
    }

    // Restore component from snapshot
    if (info->deserialize)
    {
        info->deserialize(*registry_, entity_, snapshot_);
    }

    return {};
}

} // namespace Engine
