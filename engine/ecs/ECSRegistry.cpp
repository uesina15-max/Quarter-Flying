#include "ECSRegistry.h"
#include <vector>
#include <algorithm>
#include <string>

namespace Engine
{
    ECSRegistry::ECSRegistry()
        : entityManager(std::make_unique<EntityManager>())
        , componentManager(std::make_unique<ComponentManager>())
        , typeRegistry(std::make_unique<ComponentTypeRegistry>())
    {
    }

    ECSRegistry::~ECSRegistry()
    {
    }


    Entity ECSRegistry::CreateEntity()
    {
        return entityManager->CreateEntity();
    }

    Entity ECSRegistry::CreateEntityWithUUID(UUID uuid)
    {
        return entityManager->CreateEntityWithUUID(uuid);
    }

    UUID ECSRegistry::GetUUID(Entity entity) const
    {
        return entityManager->GetUUID(entity);
    }

    Entity ECSRegistry::GetEntityByUUID(UUID uuid) const
    {
        return entityManager->GetEntityByUUID(uuid);
    }

    void ECSRegistry::DestroyEntity(Entity entity)
    {

        // Remove all components first
        componentManager->RemoveAllComponents(entity);
        
        // Then destroy entity
        entityManager->DestroyEntity(entity);
    }

    bool ECSRegistry::IsValid(Entity entity) const
    {
        return entityManager->IsEntityValid(entity);
    }

    void ECSRegistry::OnComponentModified(Entity entity, StringHash componentHash)
    {
        // 렌더링 시스템, 물리 시스템 등에 Dirty 상태 알림
        // O(1) 정수 해시를 통한 고속 이벤트 알림 처리
    }

    void ECSRegistry::OnComponentModified(Entity entity, const char* componentName)
    {
        OnComponentModified(entity, StringHash(componentName));
    }

    void ECSRegistry::OnComponentModified(Entity entity, const std::string& componentName)
    {
        OnComponentModified(entity, StringHash(componentName));
    }


    size_t ECSRegistry::GetEntityCount() const
    {
        return entityManager->GetEntityCount();
    }

    void ECSRegistry::Clear()
    {
        // 모든 Entity 제거 (Component도 함께 제거됨)
        auto entities = GetAllEntities();
        for (Entity entity : entities)
        {
            DestroyEntity(entity);
        }

        // Entity 이름 매핑 초기화
        entityNames.clear();

        // ComponentManager 내부 상태 초기화 (필요시)
        // EntityManager 내부 상태는 이미 DestroyEntity로 정리됨
    }


    // ========================================
    // Editor API Implementation
    // ========================================

    std::vector<Entity> ECSRegistry::GetAllEntities() const
    {
        return entityManager->GetAllActiveEntities();
    }

    void ECSRegistry::SetEntityName(Entity entity, const std::string& name)
    {
        entityNames[entity.id] = name;
    }

    std::string ECSRegistry::GetEntityName(Entity entity) const
    {
        auto it = entityNames.find(entity.id);
        if (it != entityNames.end())
            return it->second;
        return "Entity_" + std::to_string(entity.id);
    }

    // --- TransformComponent ---
    bool ECSRegistry::HasTransformComponent(Entity entity) const
    {
        return componentManager->HasComponent<TransformComponent>(entity);
    }

    TransformComponent* ECSRegistry::GetTransformComponent(Entity entity)
    {
        return componentManager->GetComponent<TransformComponent>(entity);
    }

    void ECSRegistry::SetTransformPosition(Entity entity, float x, float y, float z)
    {
        auto* tc = componentManager->GetComponent<TransformComponent>(entity);
        if (tc) { tc->position = Vec3(x, y, z); }
    }

    void ECSRegistry::SetTransformRotation(Entity entity, float x, float y, float z)
    {
        // Euler angles stored as XYZ in rotation field (simplified)
        auto* tc = componentManager->GetComponent<TransformComponent>(entity);
        if (tc) { tc->rotation = Quaternion(x, y, z, 1.0f); }
    }

    void ECSRegistry::SetTransformScale(Entity entity, float x, float y, float z)
    {
        auto* tc = componentManager->GetComponent<TransformComponent>(entity);
        if (tc) { tc->scale = Vec3(x, y, z); }
    }

    // --- RenderableComponent ---
    bool ECSRegistry::HasRenderableComponent(Entity entity) const
    {
        return componentManager->HasComponent<RenderableComponent>(entity);
    }

    RenderableComponent* ECSRegistry::GetRenderableComponent(Entity entity)
    {
        return componentManager->GetComponent<RenderableComponent>(entity);
    }

    // --- CameraComponent ---
    bool ECSRegistry::HasCameraComponent(Entity entity) const
    {
        return componentManager->HasComponent<CameraComponent>(entity);
    }

    CameraComponent* ECSRegistry::GetCameraComponent(Entity entity)
    {
        return componentManager->GetComponent<CameraComponent>(entity);
    }



} // namespace Engine

