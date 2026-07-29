#pragma once

#include <cstdint>
#include <functional>
#include "../core/UUID.h"

namespace Engine
{
    // ========================================
    // Entity Type
    // ========================================
    
    // Entity ID (Runtime)
    using EntityID = uint32_t;
    // UUID (Persistent) - defined in ../core/UUID.h

    struct Entity
    {
        EntityID id;

        Entity() : id(0) {}
        explicit Entity(EntityID id) : id(id) {}

        bool IsValid() const { return id != 0; }
        
        bool operator==(const Entity& other) const { return id == other.id; }
        bool operator!=(const Entity& other) const { return id != other.id; }
    };

    // 직렬화를 위한 구조체
    struct EntityMeta
    {
        UUID uuid;                 // Persistent ID (저장/참조용, 절대 변경 불가)
        Entity runtime_id;         // Runtime ID (ECS 내부용, 매 실행마다 재생성)
    };

} // namespace Engine

// ========================================
// Hash Function for Entity
// ========================================

namespace std
{
    template<>
    struct hash<Engine::Entity>
    {
        size_t operator()(const Engine::Entity& entity) const noexcept
        {
            return std::hash<Engine::EntityID>{}(entity.id);
        }
    };
}
