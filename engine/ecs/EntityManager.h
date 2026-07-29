#pragma once

#include "Entity.h"
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "core/UUID.h"

namespace Engine
{
    // ========================================
    // Entity Manager
    // ========================================
    
    // Entity 생성 및 파괴를 관리하는 컴포넌트
    // 
    // 책임:
    // - Entity ID 생성 (고유성 보장)
    // - Entity 수명주기 관리
    // - Entity ID 재사용 (선택적)
    // 
    // 스레드 안전성:
    // - nextEntityID: atomic 연산 사용
    
    class EntityManager
    {
    public:
        EntityManager();
        ~EntityManager();

        // Entity 생성 (자동 UUID 발급)
        // 반환값: 새로운 Entity
        Entity CreateEntity();

        // Entity 복원 생성 (수동 UUID 주입 - Pass1 직렬화 복원용)
        // 반환값: 지정한 UUID가 연결된 새로운 역직렬화 Entity
        Entity CreateEntityWithUUID(UUID uuid);

        // UUID 조회
        UUID GetUUID(Entity entity) const;

        // UUID로 Entity 찾기
        Entity GetEntityByUUID(UUID uuid) const;

        // Entity 파괴 마킹
        // entity: 파괴할 Entity
        // 참고: 실제 Component 제거는 ComponentManager가 담당
        void DestroyEntity(Entity entity);

        // Entity 유효성 확인
        // entity: 확인할 Entity
        // 반환값: true = 유효, false = 무효
        bool IsEntityValid(Entity entity) const;

        // 생성된 Entity 수 조회
        // 반환값: 총 생성된 Entity 수
        uint32_t GetEntityCount() const;

        // 모든 활성 Entity 목록 반환 (에디터 씬 계층 뷰용)
        std::vector<Entity> GetAllActiveEntities() const;

    private:
        // 다음 Entity ID
        std::atomic<uint32_t> nextEntityID;
        
        // 생성된 Entity 수
        std::atomic<uint32_t> entityCount;

        // 활성 Entity 집합 (에디터 조회용)
        std::unordered_set<EntityID> activeEntities;
        mutable std::atomic<bool> activeEntitiesDirty{false};

        // UUID 매핑 테이블
        std::unordered_map<EntityID, UUID> entityToUUID;
        std::unordered_map<UUID, EntityID> uuidToEntity;
    };

} // namespace Engine
