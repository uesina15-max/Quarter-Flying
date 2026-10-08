#include "EntitySnapshot.h"
#include "../../core/UUID.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Hierarchy.h"
#include "../../ecs/Reflection.h"
#include <unordered_map>

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

std::vector<EntitySnapshot> MakeDuplicateSnapshots(ECSRegistry& registry, Entity root)
{
    std::vector<EntitySnapshot> originals = CaptureSubtreeSnapshots(registry, root);

    // 원본 UUID -> 새 UUID. 전위 순서라 parent가 자식보다 먼저 들어가지만, 매핑은 어차피
    // 전체를 먼저 다 채운 뒤 참조를 바꾸므로 순서와 무관하다.
    std::unordered_map<uint64_t, uint64_t> remap;
    remap.reserve(originals.size());
    for (const EntitySnapshot& snap : originals)
    {
        remap[snap.uuid.GetValue()] = UUID::Generate().GetValue();
    }

    std::vector<EntitySnapshot> result;
    result.reserve(originals.size());
    for (const EntitySnapshot& snap : originals)
    {
        EntitySnapshot copy = snap;  // name, componentData를 복사한 뒤 UUID와 내부 참조만 바꾼다
        copy.uuid = UUID(remap[snap.uuid.GetValue()]);

        // EntityRef 필드만 골라서, 트리 안을 가리키는 참조를 새 UUID로 바꾼다. 어떤 필드가
        // EntityRef인지는 JSON만 봐서는 알 수 없으므로(UUID는 그냥 숫자다) 리플렉션 스키마로
        // 판정한다 - 숫자가 우연히 다른 엔티티 UUID와 같아도 엉뚱하게 바꾸지 않도록.
        for (const auto& [compName, info] : ComponentRegistry::GetAllComponents())
        {
            auto compIt = copy.componentData.find(compName);
            if (compIt == copy.componentData.end())
            {
                continue;
            }
            nlohmann::json& compJson = *compIt;
            for (const FieldInfo& field : info.fields)
            {
                if (field.type != FieldType::EntityRef || !compJson.contains(field.name))
                {
                    continue;
                }
                uint64_t refUuid = compJson[field.name].get<uint64_t>();
                auto mapped = remap.find(refUuid);
                if (mapped != remap.end())
                {
                    compJson[field.name] = mapped->second;  // 트리 안 → 복제본으로 재연결
                }
                // 트리 밖(루트의 부모 등)이면 그대로 둔다 - 복제본이 원본과 같은 부모 아래 들어간다.
            }
        }
        result.push_back(std::move(copy));
    }
    return result;
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
