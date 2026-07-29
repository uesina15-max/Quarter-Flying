#pragma once

#include "ECSRegistry.h"
#include "System.h"
#include <memory>
#include <vector>
#include <algorithm>
#include "Reflection.h"
#include <nlohmann/json.hpp>

namespace Engine
{
    // Forward declarations
    class JobSystem;

    // ========================================
    // World Class
    // ========================================
    
    // World는 ECS의 최상위 컨테이너로서 ECSRegistry와 System들을 관리합니다.
    // 
    // 책임:
    // - ECSRegistry 소유 및 수명주기 관리
    // - System 등록, 제거, 업데이트 순서 관리
    // - System 업데이트 실행 (순차 및 병렬)
    // - World 수명주기 관리 (Initialize, Update, Shutdown)
    // 
    // 설계 원칙:
    // - Single Responsibility: World는 ECS 전체 조정만 담당
    // - Composition: ECSRegistry와 System들을 소유
    // - Delegation: 실제 ECS 작업은 ECSRegistry에 위임
    // - Extensibility: 새로운 System 추가 용이
    
    class World
    {
    public:
        World();
        ~World();

        // ========================================
        // World Lifecycle
        // ========================================
        
        enum class EditorState { Edit, Play, Pause };

        // PIE 상태 제어
        void Play();
        void Stop();
        void Pause();
        void SetEditorState(EditorState state) { m_EditorState = state; }
        EditorState GetEditorState() const { return m_EditorState; }
        
        // World 초기화
        // 모든 등록된 System을 초기화합니다.
        void Initialize();

        // World 업데이트 (매 프레임)
        // 모든 System을 우선순위 순서로 업데이트합니다.
        // deltaTime: 이전 프레임으로부터 경과한 시간 (초 단위)
        void Update(float deltaTime);

        // World 종료
        // 모든 System을 등록의 역순으로 종료합니다.
        void Shutdown();

        // ========================================
        // System Management
        // ========================================
        
        // System 등록
        // System은 우선순위 순서로 정렬되어 저장됩니다.
        // system: 등록할 System (unique_ptr로 소유권 이전)
        void RegisterSystem(std::unique_ptr<System> system);

        // System 제거
        // 등록된 System을 제거합니다.
        // T: 제거할 System 타입
        template<typename T>
        void UnregisterSystem();

        // System 조회
        // 등록된 System을 타입으로 조회합니다.
        // T: 조회할 System 타입
        // 반환값: System 포인터 (없으면 nullptr)
        template<typename T>
        T* GetSystem();

        template<typename T>
        const T* GetSystem() const;

        // ========================================
        // ECS Registry Access
        // ========================================
        
        // ECSRegistry 조회
        // Entity와 Component 관리를 위한 ECSRegistry에 접근합니다.
        // 반환값: ECSRegistry 포인터
        ECSRegistry* GetRegistry() { return registry.get(); }
        const ECSRegistry* GetRegistry() const { return registry.get(); }

        // ========================================
        // World State
        // ========================================
        
        // World 초기화 상태 확인
        bool IsInitialized() const { return initialized; }

        // 등록된 System 개수 반환
        size_t GetSystemCount() const { return systems.size(); }

        // ========================================
        // Parallel Execution Support
        // ========================================
        
        // Job System 설정 (병렬 실행용)
        // 설정하지 않으면 순차 실행됩니다.
        // jobSystem: Job System 포인터 (소유권 없음)
        void SetJobSystem(JobSystem* jobSystem) { this->jobSystem = jobSystem; }

        // 병렬 업데이트 활성화/비활성화
        void SetParallelExecution(bool enabled) { parallelExecution = enabled; }
        bool IsParallelExecutionEnabled() const { return parallelExecution; }

    private:
        // ========================================
        // Internal Methods
        // ========================================
        
        // System 우선순위 정렬
        void SortSystemsByPriority();

        // 순차 System 업데이트
        void UpdateSystemsSequential(float deltaTime);

        // 병렬 System 업데이트 (향후 구현)
        void UpdateSystemsParallel(float deltaTime);

        // 병렬 실행 가능한 System 그룹 갱신
        void RebuildParallelGroups();

        // ========================================
        // Member Variables
        // ========================================
        
        // ECS Registry (소유)
        std::unique_ptr<ECSRegistry> registry;

        // System 목록 (소유)
        std::vector<std::unique_ptr<System>> systems;

        // Job System (참조만, 소유하지 않음)
        JobSystem* jobSystem;

        // World 상태
        bool initialized;
        bool parallelExecution;
        EditorState m_EditorState = EditorState::Edit;
        nlohmann::json m_Snapshot;
        
        // 스케줄링 상태 (캐시)
        std::vector<std::vector<System*>> parallelGroups;    // 병렬 실행 가능한 System 그룹들
    };

    // ========================================
    // Template Implementation
    // ========================================

    template<typename T>
    void World::UnregisterSystem()
    {
        static_assert(std::is_base_of<System, T>::value, 
                      "T must derive from System");

        auto it = std::find_if(systems.begin(), systems.end(),
            [](const std::unique_ptr<System>& system) {
                return dynamic_cast<T*>(system.get()) != nullptr;
            });

        if (it != systems.end())
        {
            // System이 초기화되어 있다면 종료 호출
            if (initialized && registry)
            {
                (*it)->Shutdown(*registry);
            }
            systems.erase(it);
        }
    }

    template<typename T>
    T* World::GetSystem()
    {
        static_assert(std::is_base_of<System, T>::value, 
                      "T must derive from System");

        for (const auto& system : systems)
        {
            T* castedSystem = dynamic_cast<T*>(system.get());
            if (castedSystem != nullptr)
            {
                return castedSystem;
            }
        }
        return nullptr;
    }

    template<typename T>
    const T* World::GetSystem() const
    {
        static_assert(std::is_base_of<System, T>::value, 
                      "T must derive from System");

        for (const auto& system : systems)
        {
            const T* castedSystem = dynamic_cast<const T*>(system.get());
            if (castedSystem != nullptr)
            {
                return castedSystem;
            }
        }
        return nullptr;
    }

} // namespace Engine