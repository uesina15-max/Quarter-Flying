#include "ComponentTypeRegistry.h"
#include <cassert>
#include <vector>

namespace Engine
{
    // ========================================
    // Static Members
    // ========================================
    
    const ComponentMetadata ComponentTypeRegistry::invalidMetadata = ComponentMetadata();
    
    // ========================================
    // Constructor/Destructor
    // ========================================
    
    ComponentTypeRegistry::ComponentTypeRegistry()
        : nextTypeID(1) // Start from 1, reserve 0 as invalid ID
    {
        // Reserve space for metadata to avoid frequent reallocations
        metadata.reserve(64);
    }
    
    ComponentTypeRegistry::~ComponentTypeRegistry()
    {
        // Destructor is automatically handled by RAII
        // No manual cleanup needed for standard containers and atomics
    }
    
    // ========================================
    // Public Interface
    // ========================================
    
    const ComponentMetadata& ComponentTypeRegistry::GetMetadata(ComponentTypeID typeID) const
    {
        if (typeID == INVALID_COMPONENT_TYPE_ID) {
            return invalidMetadata;
        }
        
        std::lock_guard<std::mutex> lock(metadataMutex);
        
        // ComponentTypeID is 1-based, convert to 0-based index
        size_t index = typeID - 1;
        if (index >= metadata.size()) {
            return invalidMetadata;
        }
        
        return metadata[index];
    }
    
    ComponentTypeID ComponentTypeRegistry::GetComponentTypeIDByHash(StringHash typeHash) const
    {
        std::lock_guard<std::mutex> lock(typeToIDMutex);
        auto it = hashToID.find(typeHash);
        if (it != hashToID.end()) {
            return it->second;
        }
        return INVALID_COMPONENT_TYPE_ID;
    }
    
    ComponentTypeID ComponentTypeRegistry::GetComponentTypeIDByName(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(typeToIDMutex);
        auto it = nameToID.find(name);
        if (it != nameToID.end()) {
            return it->second;
        }
        return INVALID_COMPONENT_TYPE_ID;
    }

    const ComponentMetadata& ComponentTypeRegistry::GetMetadataByHash(StringHash typeHash) const
    {
        ComponentTypeID id = GetComponentTypeIDByHash(typeHash);
        return GetMetadata(id);
    }

    const ComponentMetadata& ComponentTypeRegistry::GetMetadataByName(const std::string& name) const
    {
        ComponentTypeID id = GetComponentTypeIDByName(name);
        return GetMetadata(id);
    }

    std::vector<ComponentTypeID> ComponentTypeRegistry::GetAllComponentTypes() const
    {
        std::lock_guard<std::mutex> lock(metadataMutex);
        
        std::vector<ComponentTypeID> result;
        result.reserve(metadata.size());
        
        for (const auto& meta : metadata) {
            if (meta.id != INVALID_COMPONENT_TYPE_ID) {
                result.push_back(meta.id);
            }
        }
        
        return result;
    }
    
    size_t ComponentTypeRegistry::GetRegisteredTypeCount() const
    {
        std::lock_guard<std::mutex> lock(typeToIDMutex);
        return typeToID.size();
    }
    
    // ========================================
    // Private Implementation
    // ========================================
    
    ComponentTypeID ComponentTypeRegistry::RegisterComponentTypeImpl(
        std::type_index typeIndex, 
        size_t size, 
        size_t alignment, 
        const char* name, 
        std::function<void(void*)> destructor)
    {
        // First check if type is already registered (fast path with read lock)
        {
            std::lock_guard<std::mutex> lock(typeToIDMutex);
            auto it = typeToID.find(typeIndex);
            if (it != typeToID.end()) {
                // Type already registered, return existing ID
                return it->second;
            }
        }
        
        // Type not registered, need to create new registration
        // Use double-checked locking pattern to avoid race conditions
        
        // Generate new unique ID
        ComponentTypeID newID = nextTypeID.fetch_add(1, std::memory_order_relaxed);
        
        // Validate ID didn't overflow (extremely unlikely but safety first)
        assert(newID != INVALID_COMPONENT_TYPE_ID && "ComponentTypeID overflow");
        
        StringHash typeHash(name);
        
        // Create metadata
        ComponentMetadata newMetadata(newID, typeIndex, size, alignment, name, typeHash, destructor);
        
        // Critical section: Update both maps atomically
        {
            std::lock_guard<std::mutex> typeToIDLock(typeToIDMutex);
            std::lock_guard<std::mutex> metadataLock(metadataMutex);
            
            // Double-check that type wasn't registered by another thread
            auto it = typeToID.find(typeIndex);
            if (it != typeToID.end()) {
                return it->second;
            }
            
            // Register the new type
            typeToID[typeIndex] = newID;
            hashToID[typeHash] = newID;
            nameToID[std::string(name)] = newID;
            
            // Ensure metadata vector is large enough
            if (metadata.size() < newID) {
                metadata.resize(newID);
            }
            
            // Store metadata at 0-based index (ID is 1-based)
            metadata[newID - 1] = newMetadata;
        }
        
        return newID;
    }

} // namespace Engine