#include "AddComponentCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Reflection.h"

namespace Engine
{

AddComponentCommand::AddComponentCommand(ECSRegistry* registry, Entity entity, std::string componentType)
    : registry_(registry)
    , entity_(entity)
    , componentType_(std::move(componentType))
{
}

std::expected<void, EngineError> AddComponentCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "AddComponentCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "AddComponentCommand");
    }

    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentType_);
    if (!info)
    {
        return MakeError(EngineErrorCode::InvalidComponentType, "Unknown component type: " + componentType_, "AddComponentCommand");
    }

    if (info->hasComponent && info->hasComponent(*registry_, entity_))
    {
        return MakeError(EngineErrorCode::DuplicateComponent,
                         "Entity already has component: " + componentType_, "AddComponentCommand");
    }

    // Add component with default values by passing empty JSON
    if (info->deserialize)
    {
        info->deserialize(*registry_, entity_, nlohmann::json{});
    }

    return {};
}

std::expected<void, EngineError> AddComponentCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "AddComponentCommand");
    }

    if (!registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Target entity is not valid", "AddComponentCommand");
    }

    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentType_);
    if (!info)
    {
        return MakeError(EngineErrorCode::InvalidComponentType, "Unknown component type: " + componentType_, "AddComponentCommand");
    }

    if (info->remove)
    {
        info->remove(*registry_, entity_);
    }

    return {};
}

} // namespace Engine
