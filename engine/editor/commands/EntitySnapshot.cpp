#include "EntitySnapshot.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Hierarchy.h"
#include "../../ecs/Reflection.h"

namespace Engine
{

EntitySnapshot CaptureEntitySnapshot(ECSRegistry& registry, Entity entity)
{
    EntitySnapshot snapshot;
    snapshot.uuid = registry.GetUUID(entity);
    snapshot.name = registry.GetEntityName(entity);

    nlohmann::json componentData = nlohmann::json::object();
    for (auto& [name, info] : ComponentRegistry::GetAllComponents())
    {
        info.serialize(registry, entity, componentData);
    }
    snapshot.componentData = std::move(componentData);
    return snapshot;
}

std::expected<std::vector<Entity>, EngineError>
RestoreEntitySnapshots(ECSRegistry& registry, const std::vector<EntitySnapshot>& snapshots)
{
    std::vector<Entity> created;
    created.reserve(snapshots.size());
    for (const EntitySnapshot& snapshot : snapshots)
    {
        Entity entity = registry.CreateEntityWithUUID(snapshot.uuid);
        if (!registry.IsValid(entity))
        {
            DestroyEntitiesReverse(registry, created);
            return MakeError(EngineErrorCode::OperationFailed, "Failed to recreate entity", "EntitySnapshot");
        }
        registry.SetEntityName(entity, snapshot.name);
        created.push_back(entity);
    }

    for (size_t i = 0; i < snapshots.size(); ++i)
    {
        for (auto& [name, info] : ComponentRegistry::GetAllComponents())
        {
            if (snapshots[i].componentData.contains(name))
            {
                info.deserialize(registry, created[i], snapshots[i].componentData[name]);
            }
        }
    }
    return created;
}

std::vector<EntitySnapshot> CaptureSubtreeSnapshots(ECSRegistry& registry, Entity root)
{
    std::vector<EntitySnapshot> snapshots;
    snapshots.push_back(CaptureEntitySnapshot(registry, root));
    for (Entity descendant : GetDescendants(registry, root))
    {
        snapshots.push_back(CaptureEntitySnapshot(registry, descendant));
    }
    return snapshots;
}

void DestroyEntitiesReverse(ECSRegistry& registry, const std::vector<Entity>& entities)
{
    for (auto it = entities.rbegin(); it != entities.rend(); ++it)
    {
        if (registry.IsValid(*it))
        {
            registry.DestroyEntity(*it);
        }
    }
}

} // namespace Engine
