#pragma once

#include "Entity.h"
#include "EntityManager.h"
#include "ComponentManager.h"
#include "ComponentTypeRegistry.h"
#include "Components.h"
#include "../core/assert/Assert.h"
#include "../core/StringHash.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine
{
    // ========================================
    // ECS Registry
    // ========================================
    
    class ECSRegistry
    {
    public:
        ECSRegistry();
        ~ECSRegistry();

        // ========================================
        // Entity Management
        // ========================================
        
        Entity CreateEntity();
        Entity CreateEntityWithUUID(UUID uuid);

        UUID GetUUID(Entity entity) const;
        Entity GetEntityByUUID(UUID uuid) const;

        void DestroyEntity(Entity entity);

        // Entity 유효성 검사
        // entity.IsValid() 먼저 확인 후 EntityManager에 살아있는지 확인
        bool IsValid(Entity entity) const;

        // ========================================
        // Component Management
        // ========================================
        
        template<typename T>
        void AddComponent(Entity entity, const T& component);

        template<typename T>
        void RemoveComponent(Entity entity);

        template<typename T>
        T* GetComponent(Entity entity);

        template<typename T>
        const T* GetComponent(Entity entity) const;

        // ========================================
        // New Modern ECS API
        // ========================================

        template<typename T>
        T& get(Entity entity);

        template<typename T>
        const T& get(Entity entity) const;

        template<typename T>
        T* try_get(Entity entity) noexcept;

        template<typename T>
        const T* try_get(Entity entity) const noexcept;

        template<typename T>
        T& unchecked_get(Entity entity) noexcept;

        template<typename T>
        const T& unchecked_get(Entity entity) const noexcept;

        void OnComponentModified(Entity entity, StringHash componentHash);
        void OnComponentModified(Entity entity, const char* componentName);
        void OnComponentModified(Entity entity, const std::string& componentName);

        template<typename T>
        bool HasComponent(Entity entity) const;

        // ========================================
        // Component Array Access
        // ========================================
        
        template<typename T>
        TypedComponentArray<T>* GetComponentArray();

        template<typename T>
        const TypedComponentArray<T>* GetComponentArray() const;

        template<typename T>
        ComponentTypeID EnsureMetadata();

        template<typename T>
        TypedComponentArray<T>* EnsureStorage();


        // ========================================
        // Performance and Statistics
        // ========================================
        
        size_t GetEntityCount() const;

        // ========================================
        // Registry Reset
        // ========================================

        // 레지스트리 내용 초기화 (PIE Stop용)
        // 모든 Entity와 Component를 제거하고 내부 상태를 초기화합니다.
        void Clear();
        

        // ========================================
        // Editor API
        // ========================================

        std::vector<Entity> GetAllEntities() const;

        void SetEntityName(Entity entity, const std::string& name);
        std::string GetEntityName(Entity entity) const;

        bool HasTransformComponent(Entity entity) const;
        TransformComponent* GetTransformComponent(Entity entity);

        void SetTransformPosition(Entity entity, float x, float y, float z);
        void SetTransformRotation(Entity entity, float x, float y, float z);
        void SetTransformScale   (Entity entity, float x, float y, float z);

        bool HasRenderableComponent(Entity entity) const;
        RenderableComponent* GetRenderableComponent(Entity entity);

        bool HasCameraComponent(Entity entity) const;
        CameraComponent* GetCameraComponent(Entity entity);


        ComponentTypeRegistry* GetComponentTypeRegistry() { return typeRegistry.get(); }

    private:
        // ========================================
        // Component Instances
        // ========================================
        
        std::unique_ptr<EntityManager> entityManager;
        std::unique_ptr<ComponentManager> componentManager;
        
        std::unique_ptr<ComponentTypeRegistry> typeRegistry;
        std::unordered_map<EntityID, std::string> entityNames;

        
        // ========================================
        // Internal Coordination Methods
        // ========================================
        
        void InitializeComponents();
    };

    // ========================================
    // Template Implementation
    // ========================================

    template<typename T>
    void ECSRegistry::AddComponent(Entity entity, const T& component)
    {
        EnsureMetadata<T>();
        componentManager->AddComponent(entity, component);
    }

    template<typename T>
    void ECSRegistry::RemoveComponent(Entity entity)
    {
        componentManager->RemoveComponent<T>(entity);
    }

    template<typename T>
    T* ECSRegistry::GetComponent(Entity entity)
    {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    const T* ECSRegistry::GetComponent(Entity entity) const
    {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    T& ECSRegistry::get(Entity entity)
    {
        T* ptr = componentManager->GetComponent<T>(entity);
        ENGINE_ASSERT(ptr != nullptr, "Entity does not have the requested component!");
        return *ptr;
    }

    template<typename T>
    const T& ECSRegistry::get(Entity entity) const
    {
        const T* ptr = componentManager->GetComponent<T>(entity);
        ENGINE_ASSERT(ptr != nullptr, "Entity does not have the requested component!");
        return *ptr;
    }

    template<typename T>
    T* ECSRegistry::try_get(Entity entity) noexcept
    {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    const T* ECSRegistry::try_get(Entity entity) const noexcept
    {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    T& ECSRegistry::unchecked_get(Entity entity) noexcept
    {
        return *componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    const T& ECSRegistry::unchecked_get(Entity entity) const noexcept
    {
        return *componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    bool ECSRegistry::HasComponent(Entity entity) const
    {
        return componentManager->HasComponent<T>(entity);
    }

    template<typename T>
    TypedComponentArray<T>* ECSRegistry::GetComponentArray()
    {
        return componentManager->GetComponentArray<T>();
    }

    template<typename T>
    const TypedComponentArray<T>* ECSRegistry::GetComponentArray() const
    {
        return componentManager->GetComponentArray<T>();
    }

    template<typename T>
    ComponentTypeID ECSRegistry::EnsureMetadata()
    {
        return typeRegistry->RegisterComponentType<T>();
    }

    template<typename T>
    TypedComponentArray<T>* ECSRegistry::EnsureStorage()
    {
        return componentManager->EnsureStorage<T>();
    }

} // namespace Engine
