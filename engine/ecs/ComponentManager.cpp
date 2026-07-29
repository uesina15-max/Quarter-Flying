#include "ComponentManager.h"
#include <vector>

namespace Engine
{
    ComponentManager::ComponentManager()
    {
    }

    ComponentManager::~ComponentManager()
    {
    }

    void ComponentManager::RemoveAllComponents(Entity entity)
    {
        // Remove all components associated with this entity
        for (auto& pair : componentArrays)
        {
            pair.second->Remove(entity.id);
        }
    }

    std::vector<std::type_index> ComponentManager::GetEntityComponentTypes(Entity entity) const
    {
        std::vector<std::type_index> componentTypes;
        
        for (const auto& pair : componentArrays)
        {
            if (pair.second->Has(entity.id))
            {
                componentTypes.push_back(pair.first);
            }
        }
        
        return componentTypes;
    }

} // namespace Engine
