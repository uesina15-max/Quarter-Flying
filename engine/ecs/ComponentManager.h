#pragma once

#include "Entity.h"
#include "ComponentArray.h"
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <utility>

namespace Engine
{
    // ========================================
    // Component Manager
    // ========================================
    
    // Component 저장소를 관리하는 컴포넌트
    // 
    // 책임:
    // - Component 배열 생성 및 관리
    // - Component 추가/제거/조회
    // - Component 타입별 저장소 관리
    // 
    // 스레드 안전성:
    // - 현재 단일 스레드 전용
    // - 향후 읽기/쓰기 락 추가 가능
    
    class ComponentManager
    {
    public:
        ComponentManager();
        ~ComponentManager();

        // Component 추가
        // entity: 대상 Entity
        // component: 추가할 Component
        template<typename T>
        void AddComponent(Entity entity, const T& component);

        // Component 제거
        // entity: 대상 Entity
        template<typename T>
        void RemoveComponent(Entity entity);

        // Component 조회
        // entity: 대상 Entity
        // 반환값: Component 포인터 (없으면 nullptr)
        template<typename T>
        T* GetComponent(Entity entity);

        template<typename T>
        const T* GetComponent(Entity entity) const;

        // Component 존재 여부 확인
        // entity: 대상 Entity
        // 반환값: true = 존재, false = 없음
        template<typename T>
        bool HasComponent(Entity entity) const;

        // Entity의 모든 Component 제거
        // entity: 대상 Entity
        void RemoveAllComponents(Entity entity);

        // Component 배열 조회
        // 반환값: TypedComponentArray 포인터 (없으면 nullptr)
        template<typename T>
        TypedComponentArray<T>* GetComponentArray();

        template<typename T>
        const TypedComponentArray<T>* GetComponentArray() const;

        template<typename T>
        TypedComponentArray<T>* EnsureStorage() { return GetOrCreateComponentArray<T>(); }

        // Get all component type indices that an entity has
        // entity: 대상 Entity
        // 반환값: Entity가 가진 모든 Component의 type_index 벡터
        std::vector<std::type_index> GetEntityComponentTypes(Entity entity) const;

    private:
        // Component 배열 생성 또는 조회
        template<typename T>
        TypedComponentArray<T>* GetOrCreateComponentArray();

        // Component 타입별 저장소
        std::unordered_map<std::type_index, std::unique_ptr<ComponentArray>> componentArrays;
    };

    // ========================================
    // Template Implementation
    // ========================================

    template<typename T>
    void ComponentManager::AddComponent(Entity entity, const T& component)
    {
        auto* array = GetOrCreateComponentArray<T>();
        array->Add(entity.id, component);
    }

    template<typename T>
    void ComponentManager::RemoveComponent(Entity entity)
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it != componentArrays.end())
        {
            auto* array = static_cast<TypedComponentArray<T>*>(it->second.get());
            array->Remove(entity.id);
        }
    }

    template<typename T>
    T* ComponentManager::GetComponent(Entity entity)
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it == componentArrays.end())
        {
            return nullptr;
        }

        auto* array = static_cast<TypedComponentArray<T>*>(it->second.get());
        return array->Get(entity.id);
    }

    template<typename T>
    const T* ComponentManager::GetComponent(Entity entity) const
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it == componentArrays.end())
        {
            return nullptr;
        }

        auto* array = static_cast<const TypedComponentArray<T>*>(it->second.get());
        return array->Get(entity.id);
    }

    template<typename T>
    bool ComponentManager::HasComponent(Entity entity) const
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it == componentArrays.end())
        {
            return false;
        }

        return it->second->Has(entity.id);
    }

    template<typename T>
    TypedComponentArray<T>* ComponentManager::GetComponentArray()
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it == componentArrays.end())
        {
            return nullptr;
        }
        return static_cast<TypedComponentArray<T>*>(it->second.get());
    }

    template<typename T>
    const TypedComponentArray<T>* ComponentManager::GetComponentArray() const
    {
        auto it = componentArrays.find(std::type_index(typeid(T)));
        if (it == componentArrays.end())
        {
            return nullptr;
        }
        return static_cast<const TypedComponentArray<T>*>(it->second.get());
    }

    template<typename T>
    TypedComponentArray<T>* ComponentManager::GetOrCreateComponentArray()
    {
        std::type_index typeIndex(typeid(T));
        auto it = componentArrays.find(typeIndex);

        if (it == componentArrays.end())
        {
            auto newArray = std::make_unique<TypedComponentArray<T>>();
            auto* ptr = newArray.get();
            componentArrays[typeIndex] = std::move(newArray);
            return ptr;
        }

        return static_cast<TypedComponentArray<T>*>(it->second.get());
    }

} // namespace Engine
