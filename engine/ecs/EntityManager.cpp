#include "EntityManager.h"
#include <vector>

namespace Engine
{
    EntityManager::EntityManager()
        : nextEntityID(1) // Start from 1, 0 is reserved for invalid entity
        , entityCount(0)
    {
    }

    EntityManager::~EntityManager()
    {
    }

    Entity EntityManager::CreateEntity()
    {
        Entity entity;
        entity.id = nextEntityID.fetch_add(1, std::memory_order_relaxed);
        entityCount.fetch_add(1, std::memory_order_relaxed);
        activeEntities.insert(entity.id);
        
        UUID uuid = GenerateUUID();
        entityToUUID[entity.id] = uuid;
        uuidToEntity[uuid] = entity.id;

        return entity;
    }

    Entity EntityManager::CreateEntityWithUUID(UUID uuid)
    {
        Entity entity;
        entity.id = nextEntityID.fetch_add(1, std::memory_order_relaxed);
        entityCount.fetch_add(1, std::memory_order_relaxed);
        activeEntities.insert(entity.id);
        
        entityToUUID[entity.id] = uuid;
        uuidToEntity[uuid] = entity.id;

        return entity;
    }

    UUID EntityManager::GetUUID(Entity entity) const
    {
        auto it = entityToUUID.find(entity.id);
        if (it != entityToUUID.end())
            return it->second;
        return 0;
    }

    Entity EntityManager::GetEntityByUUID(UUID uuid) const
    {
        auto it = uuidToEntity.find(uuid);
        if (it != uuidToEntity.end())
            return Entity(it->second);
        return Entity(0);
    }

    void EntityManager::DestroyEntity(Entity entity)
    {
        if (entity.IsValid())
        {
            entityCount.fetch_sub(1, std::memory_order_relaxed);
            activeEntities.erase(entity.id);
            
            auto it = entityToUUID.find(entity.id);
            if (it != entityToUUID.end()) {
                uuidToEntity.erase(it->second);
                entityToUUID.erase(it);
            }
        }
    }

    bool EntityManager::IsEntityValid(Entity entity) const
    {
        return entity.IsValid() && entity.id < nextEntityID.load(std::memory_order_relaxed);
    }

    uint32_t EntityManager::GetEntityCount() const
    {
        return entityCount.load(std::memory_order_relaxed);
    }

    std::vector<Entity> EntityManager::GetAllActiveEntities() const
    {
        std::vector<Entity> result;
        result.reserve(activeEntities.size());
        for (EntityID id : activeEntities)
        {
            result.emplace_back(id);
        }
        return result;
    }

} // namespace Engine
