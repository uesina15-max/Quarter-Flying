#pragma once

#include "World.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace Engine
{
    // Forward declarations
    class JobSystem;

    // ========================================
    // WorldManager Class
    // ========================================
    //
    // WorldManager는 여러 World 인스턴스의 생성, 전환, 수명주기를 관리합니다.
    //
    // 책임:
    // - World 생성 및 소유
    // - 활성 World 전환
    // - 모든 World 수명주기 관리
    // - Job System 연결 (병렬 실행용)

    class WorldManager
    {
    public:
        WorldManager();
        ~WorldManager();

        // ========================================
        // World Management
        // ========================================

        // 새 World 생성 및 등록
        // name: World 식별자
        // 반환값: 생성된 World 포인터
        World* CreateWorld(const std::string& name);

        // 이름으로 World 조회
        World* GetWorld(const std::string& name);

        // 활성 World 설정
        // world: 활성화할 World 포인터 (WorldManager가 소유한 World여야 함)
        void SetActiveWorld(World* world);

        // 활성 World 조회
        World* GetActiveWorld() { return activeWorld; }
        const World* GetActiveWorld() const { return activeWorld; }

        // World 제거
        void DestroyWorld(const std::string& name);

        // ========================================
        // Lifecycle
        // ========================================

        // 활성 World 업데이트 (매 프레임)
        void Update(float deltaTime);

        // 모든 World 종료
        void Shutdown();

        // ========================================
        // Configuration
        // ========================================

        // Job System 설정 (생성되는 모든 World에 전달)
        void SetJobSystem(JobSystem* js) { jobSystem = js; }

        // 병렬 실행 활성화/비활성화
        void SetParallelExecution(bool enabled) { parallelExecution = enabled; }

        // 등록된 World 수
        size_t GetWorldCount() const { return worlds.size(); }

    private:
        std::unordered_map<std::string, std::unique_ptr<World>> worlds;
        World* activeWorld = nullptr;
        JobSystem* jobSystem = nullptr;
        bool parallelExecution = false;
    };

} // namespace Engine
