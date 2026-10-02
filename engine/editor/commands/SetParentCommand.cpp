#include "SetParentCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Hierarchy.h"

namespace Engine
{

SetParentCommand::SetParentCommand(ECSRegistry* registry, Entity child, Entity newParent)
    : registry_(registry)
{
    if (registry_)
    {
        childUuid_ = registry_->GetUUID(child);
        newParentUuid_ = newParent.IsValid() ? registry_->GetUUID(newParent) : UUID(0);
    }
}

std::expected<void, EngineError> SetParentCommand::SetParentByUUID(UUID parentUuid, bool keepWorld,
                                                                   const std::optional<TransformComponent>& local)
{
    const Entity child = registry_->GetEntityByUUID(childUuid_);
    if (!registry_->IsValid(child))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Child entity is not valid", "SetParentCommand");
    }
    Entity parent;
    if (parentUuid != UUID(0))
    {
        parent = registry_->GetEntityByUUID(parentUuid);
        if (!registry_->IsValid(parent))
        {
            return MakeError(EngineErrorCode::EntityNotFound, "Parent entity is not valid", "SetParentCommand");
        }
    }
    auto result = SetParent(*registry_, child, parent, keepWorld);
    if (result && local)
    {
        if (TransformComponent* t = registry_->GetComponent<TransformComponent>(child))
        {
            *t = *local;
        }
    }
    return result;
}

std::expected<void, EngineError> SetParentCommand::Apply()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "SetParentCommand");
    }

    if (!captured_)
    {
        const Entity child = registry_->GetEntityByUUID(childUuid_);
        if (!registry_->IsValid(child))
        {
            return MakeError(EngineErrorCode::EntityNotFound, "Child entity is not valid", "SetParentCommand");
        }
        const Entity oldParent = GetParent(*registry_, child);
        oldParentUuid_ = oldParent.IsValid() ? registry_->GetUUID(oldParent) : UUID(0);
        if (const TransformComponent* t = registry_->GetComponent<TransformComponent>(child))
        {
            oldLocal_ = *t;
        }

        // 첫 Apply만 월드 유지로 새 로컬 값을 계산하고, 그 결과를 Redo용으로 기억한다.
        auto result = SetParentByUUID(newParentUuid_, /*keepWorld=*/true, std::nullopt);
        if (!result)
        {
            return result;   // 거절(순환 등) - captured_를 세우지 않는다
        }
        if (const TransformComponent* t = registry_->GetComponent<TransformComponent>(
                registry_->GetEntityByUUID(childUuid_)))
        {
            newLocal_ = *t;
        }
        captured_ = true;
        return {};
    }
    return SetParentByUUID(newParentUuid_, false, newLocal_);
}

std::expected<void, EngineError> SetParentCommand::Undo()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", "SetParentCommand");
    }
    if (!captured_)
    {
        return MakeError(EngineErrorCode::InvalidState, "Nothing to undo", "SetParentCommand");
    }
    return SetParentByUUID(oldParentUuid_, false, oldLocal_);
}

} // namespace Engine
