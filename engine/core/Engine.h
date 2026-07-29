#pragma once

#include "Subsystem.h"
#include "memory/FrameAllocator.h"
#include "time/HighResolutionTimer.h"
#include "../platform/IPlatform.h"
#include "EngineError.h"
#include "EngineConfig.h"
#include <memory>
#include <expected>
#include <vector>
#include <typeindex>
#include <unordered_map>
#include <string>
#include <utility>
#include <mutex>

namespace Engine
{
    // Forward declarations
    class World;
    class WorldManager;
    class JobSystem;


    // ========================================
    // Engine Class
    // ========================================

    // Engine은 전체 엔진의 수명주기와 서브시스템을 관리하는 최상위 클래스입니다.
    // 
    // 수명주기:
    // 1. Initialize() - 플랫폼, 서브시스템 초기화
    // 2. Run() - 메인 루프 실행
    // 3. Shutdown() - 모든 시스템 종료
    //
    // 서브시스템 관리:
    // - RegisterSubsystem<T>() - 서브시스템 등록
    // - GetSubsystem<T>() - 서브시스템 조회

    class Engine
    {
    public:
        Engine();
        ~Engine();

        // 엔진 수명주기
        std::expected<void, EngineError> Initialize(const EngineConfig& config);
        std::expected<void, EngineError> InitializeFromWindowHandle(void* handle, const EngineConfig& config);
        void Run();

        // 엔진을 종료하고 모든 리소스를 해제합니다.
        // 이 함수 호출 이후 다시 Initialize()를 호출하여 엔진을 재시작(Restart)할 수 있습니다.
        void Shutdown();

        // 프레임 제어 (외부 루프용)
        void TickFrame();

        // 입력 처리 (외부 루프용)
        // 스레드 안전성 계약: Qt 메인 스레드 등 외부 UI 스레드에서 자유롭게 비동기 호출 가능합니다.
        // 내부 입력 큐에 thread-safe하게 적재되며, TickFrame() 호출 시 렌더 스레드에서 일괄 처리됩니다.
        void PushInputEvent(const InputEvent& event);

        // 서브시스템 관리
        template<typename T>
        void RegisterSubsystem(std::unique_ptr<T> subsystem);

        template<typename T>
        T* GetSubsystem();

        // World 관리
        World* CreateWorld(const std::string& name);
        void SetActiveWorld(World* world);
        World* GetActiveWorld();

        // 엔진 상태 조회
        bool IsRunning() const { return isRunning; }
        float GetDeltaTime() const { return deltaTime; }
        uint64_t GetFrameCount() const { return frameCount; }

        IPlatform* GetPlatform() const { return platform.get(); }
        WindowHandle GetMainWindow() const { return mainWindow; }

        // Test-only method to access frame allocator state
        #ifdef ENABLE_TESTS
        FrameAllocator* GetFrameAllocatorForTesting() { return frameAllocator.get(); }
        #endif

    private:
        // 공통 초기화 헬퍼 (코드 중복 제거)
        std::expected<void, EngineError> InitializeCoreSystems();

    private:
        // Core systems
        std::unique_ptr<IPlatform> platform;
        std::unique_ptr<FrameAllocator> frameAllocator;
        HighResolutionTimer timer;

        // Configuration
        EngineConfig config;

        // Subsystems
        std::vector<std::unique_ptr<Subsystem>> subsystems;
        std::unordered_map<std::type_index, Subsystem*> subsystemMap;

        // World management
        std::unique_ptr<WorldManager> worldManager;

        // Job system
        std::unique_ptr<JobSystem> jobSystem;

        // Engine state
        bool isRunning;
        bool initialized;
        float deltaTime;
        uint64_t frameCount;

        // Window handle
        WindowHandle mainWindow;

        // Input queue (Thread-safe between external UI thread and internal TickFrame thread)
        std::mutex inputMutex;
        std::vector<InputEvent> inputQueue;
    };

    // ========================================
    // Template Implementation
    // ========================================

    template<typename T>
    void Engine::RegisterSubsystem(std::unique_ptr<T> subsystem)
    {
        static_assert(std::is_base_of<Subsystem, T>::value, 
                      "T must derive from Subsystem");

        Subsystem* ptr = subsystem.get();
        subsystems.push_back(std::move(subsystem));
        subsystemMap[std::type_index(typeid(T))] = ptr;
    }

    template<typename T>
    T* Engine::GetSubsystem()
    {
        static_assert(std::is_base_of<Subsystem, T>::value, 
                      "T must derive from Subsystem");

        auto it = subsystemMap.find(std::type_index(typeid(T)));
        if (it != subsystemMap.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

} // namespace Engine
