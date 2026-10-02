#pragma once

#include "Subsystem.h"
#include "memory/FrameAllocator.h"
#include "time/HighResolutionTimer.h"
#include "../platform/IPlatform.h"
#include "../renderer/Camera.h"
#include "../renderer/RenderMode.h"
#include "../animation/MotionPreviewState.h"
#include "../input/InputState.h"
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

    // 뷰포트를 어느 카메라로 그릴지.
    //  - Game:   ECS의 isMainCamera 엔티티를 따라가는 카메라(RenderSystem이 매 프레임 갱신). 게임에서 보이는 화면.
    //  - Editor: 에디터가 마우스로 조작하는 카메라. ECS와 무관하며 RenderSystem이 덮어쓰지 않는다.
    // 두 카메라를 따로 두는 이유: 예전에는 카메라가 하나뿐이었고 RenderSystem이 매 프레임 Main Camera
    // 엔티티 값으로 덮어써서, 에디터 뷰포트를 마우스로 움직일 방법이 없었다.
    enum class ViewCamera : uint8_t { Game, Editor };

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

        // Motion Mixer 프리뷰 (Phase 4A, 착수 계약서 §C11) - Renderer 서브시스템으로 그대로 위임.
        MotionPreviewState* GetMotionPreviewState() const { return motionPreviewState.get(); }
        void SetRenderMode(RenderMode mode);
        RenderMode GetRenderMode();

        // 뷰포트 카메라 선택 / 에디터 카메라 조작 (ViewCamera 주석 참고)
        void SetViewCamera(ViewCamera which);
        ViewCamera GetViewCamera() const { return viewCamera; }
        void SetEditorCameraView(float eyeX, float eyeY, float eyeZ, float targetX, float targetY, float targetZ);

        // Test-only method to access frame allocator state
        #ifdef ENABLE_TESTS
        FrameAllocator* GetFrameAllocatorForTesting() { return frameAllocator.get(); }
        #endif

    private:
        // 공통 초기화 헬퍼 (코드 중복 제거)
        std::expected<void, EngineError> InitializeCoreSystems();

        // glViewport + 기본 카메라 종횡비 갱신 (WindowResize 이벤트 처리 공통 로직).
        void HandleWindowResize(uint32_t width, uint32_t height);

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

        // TickFrame에서 GL 컨텍스트 전환 실패를 이미 로그로 남겼는지 (중복 로그 방지)
        bool contextLossLogged = false;

        // TickFrame 재진입 감지용 (TickFrame 첫 부분 주석 참고)
        bool inTickFrame = false;

        // 임시 기본 카메라: ECS CameraComponent -> 렌더러 Camera를 잇는 RenderSystem이
        // 아직 없어서(ROADMAP.md 참고), 최소한 뷰포트에 뭔가 그려지도록 고정된 값으로
        // 만들어 Renderer::SetMainCamera에 넘긴다. ECS 연동은 별도 작업.
        std::unique_ptr<Camera> defaultCamera;

        // 에디터 전용 카메라(ViewCamera::Editor). 투영은 defaultCamera와 같게 유지한다(HandleWindowResize).
        std::unique_ptr<Camera> editorCamera;
        ViewCamera viewCamera = ViewCamera::Game;

        // Motion Mixer 프리뷰 상태 (Phase 4A). Renderer가 매 프레임 읽어서 그린다.
        std::unique_ptr<MotionPreviewState> motionPreviewState;

        // 이번 프레임 입력 상태. 게임 System(CameraRigSystem 등)이 포인터로 읽는다(InputState.h).
        InputState inputState;

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
