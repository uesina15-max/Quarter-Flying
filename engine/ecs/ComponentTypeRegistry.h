#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <string>
#include "../core/StringHash.h"

namespace Engine
{
    // ========================================
    // Component Type Registry
    // ========================================
    //
    // ARCHITECTURAL INVARIANT (Canonical Registration Pipeline & Key Roles):
    // ComponentTypeRegistry is the canonical source of truth and entry point for component registration.
    // - ComponentTypeID: Used as the runtime canonical dense key for ECS storage (ECSRegistry/ComponentArray).
    // - StringHash (via ComponentRegistry): Used as the stable lookup key for reflection, JSON serialization, and Python bindings.
    // - std::type_index: Used only as an auxiliary verification aid during local registration (not across DSO boundaries).
    // - name: Human-readable debug key.
    //
    // Reflection and script lookups (ComponentRegistry) operate as projection/view layers over this registration pipeline.
    
    // Component type ID for efficient component identification
    using ComponentTypeID = uint32_t;
    
    // Invalid component type ID constant
    constexpr ComponentTypeID INVALID_COMPONENT_TYPE_ID = 0;
    
    // Component metadata structure
    struct ComponentMetadata
    {
        ComponentTypeID id;
        std::type_index typeIndex;
        size_t size;
        size_t alignment;
        const char* name;
        StringHash typeHash;
        std::function<void(void*)> destructor;
        
        ComponentMetadata() 
            : id(INVALID_COMPONENT_TYPE_ID)
            , typeIndex(typeid(void))
            , size(0)
            , alignment(0)
            , name(nullptr)
            , typeHash()
            , destructor(nullptr)
        {}
        
        ComponentMetadata(ComponentTypeID id, std::type_index typeIndex, size_t size, 
                         size_t alignment, const char* name, StringHash typeHash, std::function<void(void*)> destructor)
            : id(id)
            , typeIndex(typeIndex)
            , size(size)
            , alignment(alignment)
            , name(name)
            , typeHash(typeHash)
            , destructor(destructor)
        {}
    };
    
    // Centralized component type metadata management with thread-safe operations
    class ComponentTypeRegistry
    {
    public:
        ComponentTypeRegistry();
        ~ComponentTypeRegistry();
        
        template<typename T>
        ComponentTypeID RegisterComponentType();
        
        template<typename T>
        ComponentTypeID GetComponentTypeID() const;
        
        const ComponentMetadata& GetMetadata(ComponentTypeID typeID) const;
        
        // Get component type ID by StringHash
        ComponentTypeID GetComponentTypeIDByHash(StringHash typeHash) const;
        
        // Get component type ID by Name
        ComponentTypeID GetComponentTypeIDByName(const std::string& name) const;

        // Get component metadata by StringHash
        const ComponentMetadata& GetMetadataByHash(StringHash typeHash) const;

        // Get component metadata by Name
        const ComponentMetadata& GetMetadataByName(const std::string& name) const;
        
        std::vector<ComponentTypeID> GetAllComponentTypes() const;
        
        size_t GetRegisteredTypeCount() const;
        
    private:
        ComponentTypeID RegisterComponentTypeImpl(std::type_index typeIndex, 
                                                 size_t size, size_t alignment, 
                                                 const char* name, 
                                                 std::function<void(void*)> destructor);
        
        mutable std::mutex typeToIDMutex;
        std::unordered_map<std::type_index, ComponentTypeID> typeToID;
        std::unordered_map<StringHash, ComponentTypeID> hashToID;
        std::unordered_map<std::string, ComponentTypeID> nameToID;
        
        mutable std::mutex metadataMutex;
        std::vector<ComponentMetadata> metadata;
        
        std::atomic<ComponentTypeID> nextTypeID;
        
        static const ComponentMetadata invalidMetadata;
    };
    
    // ========================================
    // Template Implementation
    // ========================================
    
    template<typename T>
    ComponentTypeID ComponentTypeRegistry::RegisterComponentType()
    {
        // Create destructor function for this type
        auto destructor = [](void* ptr) {
            if (ptr != nullptr) {
                static_cast<T*>(ptr)->~T();
            }
        };
        
        return RegisterComponentTypeImpl(
            std::type_index(typeid(T)),
            sizeof(T),
            alignof(T),
            typeid(T).name(),
            destructor
        );
    }
    
    template<typename T>
    ComponentTypeID ComponentTypeRegistry::GetComponentTypeID() const
    {
        std::type_index typeIndex(typeid(T));
        
        std::lock_guard<std::mutex> lock(typeToIDMutex);
        auto it = typeToID.find(typeIndex);
        if (it != typeToID.end()) {
            return it->second;
        }
        
        return INVALID_COMPONENT_TYPE_ID;
    }

} // namespace Engine