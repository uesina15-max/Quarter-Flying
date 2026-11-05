/*
 * Copyright 2026 Quarter Flying Game Engine Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Engine.h"
#include "logging/Logger.h"
#include "assert/Assert.h"
#include "memory/MemoryTracker.h"
#include "PlatformFactory.h"
#include "../ecs/WorldManager.h"
#include "../ecs/RenderSystem.h"
#include "../renderer/GameOverlay.h"
#include "../ecs/CameraSystem.h"
#include "../ecs/CameraRigSystem.h"
#include "../ecs/KeyboardMovementSystem.h"
#include <chrono>
#include "../animation/LocomotionSystem.h"
#include "../ecs/CameraGizmoSystem.h"
#include "../ecs/ParticleSystem.h"
#include "../job/JobSystem.h"
#include "../renderer/Renderer.h"
#include "../renderer/Texture2D.h"
#include "../asset/AssetManager.h"
#ifndef QF_NO_PYTHON_SCRIPTING
#include "../ecs/ScriptSystem.h"   // 테스트 타깃은 pybind11 없이 빌드하므로 제외(tests/CMakeLists.txt)
#endif
#include "../asset/importers/TextureImporter.h"
#include <string>
#include <memory>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

namespace Engine
{
    Engine::Engine()
        : isRunning(false)
        , initialized(false)
        , deltaTime(0.0f)
        , frameCount(0)
    {
    }

    Engine::~Engine()
    {
        if (initialized)
        {
            Shutdown();
        }
    }

    std::expected<void, EngineError> Engine::InitializeCoreSystems()
    {
        // 3. FrameAllocator 초기화
        frameAllocator = std::make_unique<FrameAllocator>();
        frameAllocator->Initialize(config.frameAllocatorSize);

        // 4. 타이머 초기화
        timer.Reset();

        // 5. Job System 초기화 (서브시스템 초기화 전)
        jobSystem = std::make_unique<JobSystem>();
        jobSystem->Initialize(config.numWorkerThreads);

        // 6. World Manager 초기화 (서브시스템 초기화 전)
        worldManager = std::make_unique<WorldManager>();
        worldManager->SetJobSystem(jobSystem.get());

        // 6-1. OpenGL 렌더링 컨텍스트 생성
        // Renderer(및 그 내부의 InstancedBatchManager 등)의 Initialize()가 glew/gl 함수를
        // 바로 호출하므로, 반드시 서브시스템 초기화 루프보다 먼저 컨텍스트가 살아있어야 한다.
        if (!platform->CreateGraphicsContext(mainWindow))
        {
            Logger::Error("Failed to create OpenGL graphics context");
            if (worldManager) { worldManager->Shutdown(); worldManager.reset(); }
            if (jobSystem) { jobSystem->Shutdown(); jobSystem.reset(); }
            if (frameAllocator) { frameAllocator->Shutdown(); frameAllocator.reset(); }
            return std::unexpected(EngineError(EngineErrorCode::PlatformInitFailed, "OpenGL graphics context creation failed"));
        }

        // 6-2. Renderer 서브시스템 등록
        // 지금까지 이 엔진 어디에도 Renderer가 등록된 적이 없었다 - RenderGraph에 패스가
        // 하나도 안 쌓이는 것과 별개로, Renderer::Tick/LateTick 자체가 호출되지 않고 있었다.
        RegisterSubsystem<Renderer>(std::make_unique<Renderer>());

        // 6-2-1. AssetManager 서브시스템(파일 -> CPU 데이터, 경로 중복 제거). 지금은 텍스처만 이 경로를
        // 쓴다(메시는 Mesh::loadFromFile). 예전엔 모듈이 빌드만 되고 어디에도 등록되지 않아서 임포터가
        // 픽셀을 버리는 버그(TextureImporter::Import 주석)도, GetAsset<T> 정의 누락도 드러나지 않았다.
        RegisterSubsystem<AssetManager>(std::make_unique<AssetManager>());

        // 6-3. 임시 기본 카메라
        // World가 아직 하나도 없는 시점(CreateWorld는 이 초기화 이후에 Python에서 호출됨)이라
        // RenderSystem이 ECS Main Camera를 읽어 갱신해주기 전까지의 시작값이 필요하다 -
        // 에디터가 만드는 기본 카메라 위치(0,5,15)와 맞춰 둠. World가 생성되고 그 안의
        // RenderSystem이 매 프레임 이 Camera를 갱신하기 시작하면(CreateWorld 참고)
        // 이 시작값은 첫 프레임 이후 ECS 쪽 값으로 덮어써진다.
        defaultCamera = std::make_unique<Camera>();
        defaultCamera->setPosition(glm::vec3(0.0f, 5.0f, 15.0f));
        defaultCamera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        float aspect = (config.windowHeight > 0)
            ? static_cast<float>(config.windowWidth) / static_cast<float>(config.windowHeight)
            : 16.0f / 9.0f;
        defaultCamera->setProjection(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

        // 6-3-1. 에디터 카메라(ViewCamera 주석 참고). 시작값은 게임 카메라와 같다.
        editorCamera = std::make_unique<Camera>();
        editorCamera->setPosition(glm::vec3(0.0f, 5.0f, 15.0f));
        editorCamera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f));
        editorCamera->setProjection(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

        // 6-4. Motion Mixer 프리뷰 상태 (Phase 4A)
        motionPreviewState = std::make_unique<MotionPreviewState>();

        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            renderer->SetMainCamera(viewCamera == ViewCamera::Editor ? editorCamera.get() : defaultCamera.get());
            renderer->SetDrawEditorGizmos(viewCamera == ViewCamera::Editor);
            renderer->SetDrawDebugGrid(config.showDebugGrid);
            renderer->SetMotionPreviewState(motionPreviewState.get());
            renderer->SetViewportSize(config.windowWidth, config.windowHeight);
        }

        // 7. 서브시스템 초기화 (등록된 순서대로)
        Logger::Info("Initializing subsystems...");
        std::vector<Subsystem*> initializedSubsystems;
        for (auto& subsystem : subsystems)
        {
            auto res = subsystem->Initialize();
            if (!res.has_value())
            {
                Logger::Error("Subsystem initialization failed: {}", res.error().message);
                // 롤백: 이미 초기화된 서브시스템들을 역순으로 안전 종료
                for (auto it = initializedSubsystems.rbegin(); it != initializedSubsystems.rend(); ++it)
                {
                    (*it)->Shutdown();
                }
                if (worldManager) { worldManager->Shutdown(); worldManager.reset(); }
                if (jobSystem) { jobSystem->Shutdown(); jobSystem.reset(); }
                if (frameAllocator) { frameAllocator->Shutdown(); frameAllocator.reset(); }
                if (mainWindow.IsValid()) { platform->DestroyWindow(mainWindow); mainWindow = WindowHandle(); }
                if (platform) { platform->Shutdown(); platform.reset(); }
                return std::unexpected(res.error());
            }
            initializedSubsystems.push_back(subsystem.get());
        }

        return {};
    }

    std::expected<void, EngineError> Engine::Initialize(const EngineConfig& cfg)
    {
        if (initialized)
        {
            Logger::Warning("Engine already initialized");
            return std::unexpected(EngineError(EngineErrorCode::AlreadyInitialized, "Engine already initialized"));
        }

        // Store configuration
        config = cfg;
        Logger::Info("Initializing Engine...");

        // 1. 플랫폼 초기화
        platform = PlatformFactory::CreatePlatform();
        if (!platform || !platform->Initialize())
        {
            Logger::Fatal("Failed to initialize platform");
            return std::unexpected(EngineError(EngineErrorCode::PlatformInitFailed, "Platform initialization failed"));
        }

        // 2. 메인 윈도우 생성
        WindowDesc windowDesc;
        windowDesc.title = config.windowTitle;
        windowDesc.width = config.windowWidth;
        windowDesc.height = config.windowHeight;
        windowDesc.fullscreen = config.windowFullscreen;

        mainWindow = platform->CreateWindow(windowDesc);
        if (!mainWindow.IsValid())
        {
            Logger::Fatal("Failed to create main window");
            return std::unexpected(EngineError(EngineErrorCode::WindowCreationFailed, "Window creation failed"));
        }
        if (config.cursorLock)
        {
            platform->SetCursorLockRequested(true);
        }

        // 3-7. 공통 코어 시스템 초기화
        auto coreResult = InitializeCoreSystems();
        if (!coreResult.has_value())
        {
            return std::unexpected(coreResult.error());
        }

        initialized = true;
        isRunning = false;
        frameCount = 0;

        Logger::Info("Engine initialized successfully");
        return {};
    }

    std::expected<void, EngineError> Engine::InitializeFromWindowHandle(void* handle, const EngineConfig& cfg)
    {
        if (initialized)
        {
            Logger::Warning("Engine already initialized");
            return std::unexpected(EngineError(EngineErrorCode::AlreadyInitialized, "Engine already initialized"));
        }

        config = cfg;
        Logger::Info("Initializing Engine from external handle...");

        // 1. 플랫폼 초기화
        platform = PlatformFactory::CreatePlatform();
        if (!platform || !platform->Initialize())
        {
            Logger::Fatal("Failed to initialize platform");
            return std::unexpected(EngineError(EngineErrorCode::PlatformInitFailed, "Platform initialization failed"));
        }

        // 2. 외부 핸들 설정
        platform->SetExternalWindowHandle(handle);
        mainWindow = WindowHandle(1, 0); // 외자 윈도우를 메인 윈도우로 간주 (ID 1)

        // 3-7. 공통 코어 시스템 초기화
        auto coreResult = InitializeCoreSystems();
        if (!coreResult.has_value())
        {
            return std::unexpected(coreResult.error());
        }

        initialized = true;
        isRunning = false;
        frameCount = 0;

        Logger::Info("Engine initialized from handle successfully");
        return {};
    }

    void Engine::Run()
    {
        Run(std::function<bool(float)>{});
    }

    void Engine::Run(const std::function<bool(float)>& afterFrame)
    {
        if (!initialized)
        {
            Logger::Error("Engine not initialized");
            return;
        }

        Logger::Info("Starting engine main loop...");
        isRunning = true;
        timer.Reset();

        while (isRunning)
        {
            TickFrame();
            if (isRunning && afterFrame && !afterFrame(deltaTime))
            {
                isRunning = false;
            }
        }

        Logger::Info("Engine main loop ended");
    }

    void Engine::PushInputEvent(const InputEvent& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        inputQueue.push_back(event);
    }

    bool Engine::DispatchInputEvent(const InputEvent& event)
    {
        if (gameOverlay) gameOverlay->QueuePointer(event);
        switch (event.type)
        {
        case InputEventType::WindowClose:
            isRunning = false;
            return false;
        case InputEventType::WindowResize:
            HandleWindowResize(event.windowWidth, event.windowHeight);
            return true;
        case InputEventType::FocusLost:
            HandleFocusLost();
            return true;
        default:
            inputState.Apply(event);
            return true;
        }
    }

    void Engine::HandleFocusLost()
    {
        // 포커스를 잃으면 그 창으로 KeyUp/ButtonUp이 오지 않는다. 그대로 두면 W를 누른 채 다른 창으로
        // 가는 순간 W가 영원히 눌린 상태로 남아 캐릭터가 계속 걷는다(되돌아와서 W를 다시 눌렀다 떼야 풀린다).
        // 정리 루틴은 여기서 하나씩 부른다.
        inputState.ReleaseAll();         // 키 전부, 마우스 버튼 3개, 마지막 마우스 위치
        inputActions.ResetHeldState();   // 다음 Update가 WasReleased를 만들지 않게(InputActions.h)
        if (platform)
        {
            platform->SuspendCursorLock();   // 적용만 푼다. 요청은 유지 - 창을 다시 클릭하면 다시 잠긴다
        }
        Logger::Debug("Engine: focus lost - released all held keys and mouse buttons");
    }

    void Engine::Shutdown()
    {
        if (!initialized)
        {
            return;
        }

        Logger::Info("Shutting down Engine...");
        if (gameOverlay && platform) platform->MakeGraphicsContextCurrent(mainWindow);
        gameOverlay.reset();

        isRunning = false;

        // 1. 서브시스템 종료 (초기화의 역순)
        Logger::Info("Shutting down subsystems...");
        for (auto it = subsystems.rbegin(); it != subsystems.rend(); ++it)
        {
            (*it)->Shutdown();
        }
        subsystems.clear();
        subsystemMap.clear();

        // 2. World Manager 종료
        if (worldManager)
        {
            worldManager->Shutdown();
            worldManager.reset();
        }

        // 3. Job System 종료
        if (jobSystem)
        {
            jobSystem->Shutdown(); // Shutdown should internally WaitIdle/Join workers
            jobSystem.reset();
        }

        // 4. FrameAllocator 종료
        if (frameAllocator)
        {
            frameAllocator->Shutdown();
            frameAllocator.reset();
        }

        // 5. 윈도우 파괴
        if (mainWindow.IsValid())
        {
            platform->DestroyWindow(mainWindow);
            mainWindow = WindowHandle();
        }

        // 6. 플랫폼 종료
        if (platform)
        {
            platform->Shutdown();
            platform.reset();
        }

        // 7. 메모리 누수 검사 (Leak Detector)
        // MemoryTracker 클래스 전체가 core/memory/MemoryTracker.h에서 #ifdef _DEBUG로
        // 감싸져 있어 Release 빌드에는 존재하지 않는다. 무가드 호출은 Release에서
        // "MemoryTracker는 클래스/네임스페이스 이름이 아님"(C2653) 컴파일 에러가 난다.
        Logger::Info("Running Memory Leak Detector...");
#ifdef _DEBUG
        MemoryTracker::Get().ReportLeaks();
#endif

        initialized = false;
        Logger::Info("Engine shutdown complete");
    }

    void Engine::TickFrame()
    {
        // 0. 재진입 가드. TickFrame 도중 호스트(Qt) 코드가 실행되어 같은 Engine의 TickFrame을
        // 다시 부르면(예: 핸들러 안의 QApplication::processEvents()가 FPS 타이머를 돌림),
        // frameAllocator 리셋, RenderGraph 리셋 등 프레임 상태가 중간에 뒤엎어져
        // access violation으로 죽었다(Win32Platform::PollEvents 주석의 실제 크래시).
        // 펌프 쪽 원인은 고쳤지만, 같은 가정이 다른 경로로 깨졌을 때 조용히 틀리게 돌지 않도록
        // 여기서 감지하고 이유를 남긴 뒤 건너뛴다(CLAUDE.md 관례 3번).
        if (inTickFrame)
        {
            Logger::Error("TickFrame - re-entered while a frame is already in progress on this engine "
                          "(host code called TickFrame from inside TickFrame); skipping nested call");
            return;
        }
        struct TickGuard
        {
            bool& flag;
            explicit TickGuard(bool& f) : flag(f) { flag = true; }
            ~TickGuard() { flag = false; }
        } tickGuard(inTickFrame);

        // 1. 프레임 시간 계산
        deltaTime = timer.GetDeltaTime();
        frameCount++;

        // 2. 프레임 시작 - FrameAllocator 리셋
        frameAllocator->Reset();

        // 3. 입력 처리
        if (!platform->PollEvents())
        {
            isRunning = false;
            return;
        }

        // 3-0. 이 Engine의 GL 컨텍스트를 current로 만든다. 아래 입력 처리(HandleWindowResize의
        // glViewport)부터 GL을 쓰므로 그보다 먼저 해야 한다. PollEvents()보다 뒤에 두는 이유는,
        // 펌프 도중 실행된 코드가 컨텍스트를 바꿔 놓아도 이번 프레임의 GL 호출은 항상 자기
        // 컨텍스트에서 하게 하기 위해서다.
        //
        // 증상: Play Mode 뷰포트(두 번째 Engine)가 처음으로 초기화에 성공하자(Win32Platform의
        // ERROR_CLASS_ALREADY_EXISTS 허용 이후), Play Mode 탭을 누르는 순간 에디터가
        // Segmentation fault로 죽었다. 로그의 마지막 줄은 Scene 뷰포트 쪽의
        // "InstancedBatchManager::UploadInstanceBuffer - Uploaded 80 instances"였다.
        // 그 전에는 두 번째 뷰포트가 초기화에 실패해 우아하게 폴백했으므로 이 크래시가
        // 가려져 있었다.
        // 원인: wglMakeCurrent는 스레드 단위 상태인데, 이 호출은 CreateGraphicsContext()에서
        // 한 번만 있었다. 두 번째 Engine이 컨텍스트를 만들면, 같은 Qt 메인 스레드에서 도는 첫 번째
        // Engine도 그 뒤로 남의 컨텍스트에 자기 버퍼/VAO 이름으로 GL 호출을 했다.
        // (Renderer::SetViewportSize 주석의 "glViewport가 엉뚱한 크기" 증상도 같은 원인이다.)
        if (!platform->MakeGraphicsContextCurrent(mainWindow))
        {
            // 남의 컨텍스트에 조용히 그리느니 이번 프레임을 건너뛴다(CLAUDE.md 관례 3번).
            // 로그는 첫 실패 때만 남긴다 - 매 프레임 찍으면 60줄/초로 로그가 묻힌다.
            if (!contextLossLogged)
            {
                Logger::Error("TickFrame - this engine's GL context could not be made current; skipping frames until it can");
                contextLossLogged = true;
            }
            return;
        }
        contextLossLogged = false;

        // 일시정지 중에도 게임 UI는 화면에 남아야 한다. 예전에는 Play 상태에서만 만들어서, Pause를 누르면 UI가 통째로
        // 사라졌다가(오버레이 상태까지 파괴) 재개하면 다시 나타났다.
        World* overlayWorld = GetActiveWorld();
        if (viewCamera == ViewCamera::Game && overlayWorld &&
            overlayWorld->GetEditorState() != World::EditorState::Edit)
        {
            if (!gameOverlay) gameOverlay = std::make_unique<GameOverlay>();
        }
        else
        {
            gameOverlay.reset();
        }

        // 입력 상태: 이번 프레임 이동량을 0으로 되돌리고, 아래 두 경로의 이벤트를 모두 반영한다(InputState.h).
        inputState.BeginFrame();

        // 두 입력 경로(플랫폼 큐 = Player 창, 외부 큐 = 에디터 Qt)는 모두 DispatchInputEvent를 거친다.
        // 예전에는 경로마다 이벤트 종류별 분기가 따로 있었다(플랫폼 쪽은 Apply를 먼저 부르고, 외부 쪽은
        // 나중에 불렀다). 새 이벤트 종류(FocusLost)를 한쪽에만 넣으면 에디터와 Player가 다르게 동작한다.
        InputEvent event;
        while ((event = platform->GetNextInputEvent()).type != InputEventType::None)
        {
            if (!DispatchInputEvent(event))
            {
                return;
            }
        }

        // 3-1. 외부 UI 스레드로부터 큐잉된 비동기 입력 이벤트 일괄 처리 (Method A: Lock 최소화)
        // Qt가 소유한 외부 HWND(EngineViewport 임베딩)는 우리 WindowProc을 거치지 않으므로 위
        // platform->GetNextInputEvent() 경로로는 이벤트가 오지 않는다 - viewport.py가
        // PushInputEvent()로 여기(비동기 큐)에 넣는다(크기 변경·포커스 상실 포함).
        std::vector<InputEvent> localInputQueue;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            std::swap(localInputQueue, inputQueue);
        }

        for (const auto& queuedEvent : localInputQueue)
        {
            if (!DispatchInputEvent(queuedEvent))
            {
                return;
            }
        }

        // 3-2. 액션 상태를 프레임당 한 번 계산한다(스냅샷). 아래 World의 C++ 시스템과 Python 스크립트가 모두
        // 같은 값을 읽는다 - 읽는 쪽이 각자 계산하거나 읽기가 엣지를 소비하면, 먼저 읽은 쪽만 WasPressed를 보고
        // 나중에 읽은 쪽은 놓친다. 반드시 이벤트 적용 뒤, World 업데이트 전이어야 한다.
        inputActions.Update(inputState);
        using FrameClock = std::chrono::steady_clock;
        const auto simulationStart = FrameClock::now();

        // 4. ECS 업데이트 (입력 처리 → ECS 업데이트 → 렌더링 순서)
        if (worldManager)
        {
            // Python 스크립트의 quarterflying.input은 이 범위 안에서만 이 엔진의 액션 맵을 읽는다(InputActions::Current).
            InputActions::ScopedCurrent currentInput(&inputActions);
            // 에디터 선택 강조는 Edit 상태에서만. Play 중에도 선택(Inspector 읽기 전용)은 유지되지만 게임 화면에
            // 노란 강조가 남으면 안 된다.
            if (World* active = worldManager->GetActiveWorld())
            {
                if (RenderSystem* rs = active->GetSystem<RenderSystem>())
                {
                    const bool editing = active->GetEditorState() == World::EditorState::Edit;
                    rs->SetSelectedEntities(editing ? selectedEntities : std::vector<Entity>{});
                }
            }
            worldManager->Update(deltaTime);
            UpdateTransformGizmo();   // World 갱신 뒤 - 이번 프레임의 최종 위치(드래그 결과 포함)에 그린다
        }

        // 5. 서브시스템 Tick (Renderer::Tick에서 BeginFrame 호출)
        const auto rendererStart = FrameClock::now();
        for (auto& subsystem : subsystems)
        {
            subsystem->Tick(deltaTime);
        }

        // 6. 서브시스템 LateTick (Renderer::LateTick에서 Render + EndFrame 호출)
        for (auto& subsystem : subsystems)
        {
            subsystem->LateTick(deltaTime);
        }

        if (gameOverlay && overlayWorld == GetActiveWorld())
        {
            // on_ui_click 등도 게임 스크립트다 - on_update처럼 quarterflying.input을 읽을 수 있어야 한다.
            // 증상: "시작" 버튼의 on_ui_click에서 set_cursor_locked를 부르면 "quarterflying.input.set_cursor_locked:
            // no engine frame is running"으로 실패했다(로그에만 남고 버튼은 아무 일도 안 하는 것처럼 보임).
            // 원인: 위 World 업데이트의 ScopedCurrent 범위 밖에서 콜백을 불렀다.
            InputActions::ScopedCurrent currentInput(&inputActions);
            gameOverlay->Frame(*overlayWorld->GetRegistry(), config.assetRoot,
                               overlayWorld->GetEditorState() == World::EditorState::Play);
        }

        // 스크립트가 이번 프레임에 커서 잠금을 바꿨으면(quarterflying.input.set_cursor_locked) 플랫폼에 전한다.
        // 스크립트는 플랫폼에 직접 닿지 않는다 - 요청은 InputActions에 쌓이고, 적용 판단(포커스·임베드)은 플랫폼이 한다.
        // UI 콜백이 낸 요청도 받도록 오버레이 뒤에서 처리한다.
        if (auto lockRequest = inputActions.TakeCursorLockRequest(); lockRequest && platform)
        {
            platform->SetCursorLockRequested(*lockRequest);
        }

        const auto presentStart = FrameClock::now();
        // 6-1. 더블 버퍼 스왑 - 이번 프레임에 그린 내용을 화면에 표시
        if (platform)
        {
            platform->PresentFrame(mainWindow);
        }
        using Milliseconds = std::chrono::duration<double,std::milli>;
        frameCosts = {Milliseconds(rendererStart-simulationStart).count(),
                      Milliseconds(presentStart-rendererStart).count(),
                      Milliseconds(FrameClock::now()-presentStart).count()};

        // 프레임 시간 로깅 (설정된 간격마다)
        if (config.logFrameInterval > 0 && frameCount % config.logFrameInterval == 0)
        {
            float fps = (deltaTime > 0.0f) ? 1.0f / deltaTime : 0.0f;
            Logger::Info("Frame: {}, FPS: {:.2f}, DeltaTime: {:.2f}ms", frameCount, fps, deltaTime * 1000.0f);
        }
    }

    void Engine::HandleWindowResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        // glViewport는 CreateGraphicsContext() 시점에 딱 한 번만 설정되고 그 뒤로는 아무도
        // 갱신하지 않고 있었다 - 창 크기가 바뀌면(또는 Qt가 위젯을 다른 레이아웃/스플리터로
        // 재부모 이동시켜 실제 클라이언트 영역 크기가 바뀌면) 렌더링이 예전 크기 기준으로
        // 잘리거나 완전히 안 보이게 된다.
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

        // 종횡비가 안 맞으면 그림이 눌리거나 늘어나 보인다. Engine은 종횡비만 갱신한다.
        // 렌즈(fov/near/far)는 CameraSystem이 활성 CameraComponent 값으로 관리하므로 여기서 덮어쓰지 않는다
        // (예전에는 60도 고정 투영으로 통째로 덮어써서 CameraComponent.fov가 반영되지 않았다).
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        viewportPixelWidth = width;
        viewportPixelHeight = height;
        if (defaultCamera)
        {
            defaultCamera->setAspect(aspect);
        }
        if (editorCamera)
        {
            editorCamera->setAspect(aspect);
        }

        // Renderer가 매 프레임 재적용할 수 있도록 최신 크기를 전달 (Renderer::SetViewportSize
        // 주석 참고 - 한 프로세스 안에 여러 EngineViewport/GL 컨텍스트가 있을 때 다른 인스턴스가
        // glViewport를 밟고 지나가는 문제 대응).
        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            renderer->SetViewportSize(width, height);
        }
    }

    World* Engine::CreateWorld(const std::string& name)
    {
        if (!worldManager)
        {
            Logger::Error("Engine::CreateWorld - WorldManager not initialized");
            return nullptr;
        }
        World* world = worldManager->CreateWorld(name);
        if (world)
        {
            world->Initialize();

            // ROADMAP.md P0-2: TransformComponent+RenderableComponent를 실제 draw call로
            // 옮기는 RenderSystem을 모든 World에 자동 등록한다(World가 생성되는 유일한
            // 경로가 여기라서, 여기 한 곳만 있으면 Python이 만드는 World도 빠짐없이 커버됨).
            // GetSubsystem<Renderer>()가 아직 없으면(Renderer 서브시스템 등록 실패 등)
            // batchManager는 nullptr로 넘어가고, RenderSystem::Update()는 그 경우 아무 것도
            // 안 하고 조용히 리턴한다 - 시각화가 안 될 뿐 크래시로 이어지진 않는다.
            Renderer* renderer = GetSubsystem<Renderer>();

            // 텍스처 로더: asset/(파일 해석, 픽셀) -> renderer/(GL 업로드)를 여기서 잇는다. 두 모듈이
            // 서로 모르게 하려고 연결은 조립 지점(Engine)에 둔다. 로더는 RenderSystem::Update 안에서
            // 불리므로 이 Engine의 GL 컨텍스트가 current다(TickFrame이 매 프레임 보장).
            RenderSystem::TextureLoader textureLoader;
            if (AssetManager* assets = GetSubsystem<AssetManager>())
            {
                textureLoader = [assets](const std::string& fullPath) -> RenderSystem::LoadedTexture {
                    auto handle = assets->LoadAsset(fullPath);
                    if (!handle)
                    {
                        Logger::Warning("Texture load failed: {}", handle.error().message);
                        return {};
                    }
                    auto data = assets->GetAsset<TextureData>(handle.value());
                    if (!data || !data.value())
                    {
                        Logger::Warning("Texture load failed: {}",
                                        data ? std::string("null texture data") : data.error().message);
                        return {};
                    }
                    const TextureData& tex = *data.value();
                    auto gpu = Texture2D::Create(tex.width, tex.height, tex.channels, tex.pixels.data());
                    if (!gpu)
                    {
                        return {};
                    }
                    const uint32_t id = gpu->GetId();
                    return { id, std::move(gpu) };
                };
            }

            auto renderSystem = std::make_unique<RenderSystem>(
                renderer ? renderer->GetInstancedBatchManager() : nullptr,
                config.assetRoot, RenderSystem::MeshLoader{}, textureLoader,
                renderer ? renderer->GetSceneLightingTarget() : nullptr);
            // 머티리얼 파일(R-4). 파서는 asset/MaterialAsset 하나 - 에디터·Player가 같은 코드를 지난다.
            renderSystem->SetMaterialLoader([](const std::string& fullPath) { return LoadMaterialFile(fullPath); });
            world->RegisterSystem(std::move(renderSystem));

            // ECS 활성 카메라 -> 게임 카메라(위치/시선/렌즈). RenderSystem에서 분리했다(CameraSystem.h).
            world->RegisterSystem(std::make_unique<CameraSystem>(defaultCamera.get()));

            // 추적/주시/플레이어 조작 카메라가 카메라 엔티티의 Transform을 먼저 계산한다(CameraRigSystem.h).
            world->RegisterSystem(std::make_unique<CameraRigSystem>(&inputState));
            world->RegisterSystem(std::make_unique<KeyboardMovementSystem>(&inputState));
            world->RegisterSystem(std::make_unique<LocomotionSystem>(config.assetRoot));

            // 에디터 카메라 기즈모(카메라마다 위치/시야 선). 그릴지는 Renderer가 뷰 카메라에 따라 정한다.
            world->RegisterSystem(std::make_unique<CameraGizmoSystem>(
                renderer ? &renderer->GetDebugLineBuffer() : nullptr, defaultCamera.get()));

            // VFX Lite Phase 3A(docs/VFX_LITE_PHASE3_RENDERING_PLAN.md §3.8) - 같은 자리에
            // ParticleSystem도 등록한다. Phase 2에서 시뮬레이션은 다 만들었지만 여기까지
            // 연결되지 않아서 그동안 아무도 Update()를 부르지 않고 있었다.
            // RenderSystem과 같은 nullptr 허용 규약이라, Renderer 서브시스템이 없으면
            // 인스턴스 수집만 건너뛰고 시뮬레이션은 정상적으로 돈다.
            // GetPriority()가 100이라 RenderSystem(0) 뒤에 돈다 - RenderSystem이 갱신한
            // 카메라를 같은 프레임에 쓰기 위해서다(§3.9).
            world->RegisterSystem(std::make_unique<ParticleSystem>(
                renderer ? renderer->GetInstancedBatchManager() : nullptr,
                defaultCamera.get(), std::move(textureLoader), config.assetRoot));

            // Python 스크립트(ScriptComponent). 예전에는 ScriptSystem이 구현만 되어 있고 어떤 World에도
            // 등록되지 않아서, ScriptComponent를 붙여도 on_create/on_update가 한 번도 호출되지 않았다
            // (에러도 로그도 없이 "아무 일도 안 일어남"). Play 상태에서만 돈다(ScriptSystem::RunsInEditMode).
#ifndef QF_NO_PYTHON_SCRIPTING
            world->RegisterSystem(std::make_unique<ScriptSystem>(config.scriptHotReload));
#endif
        }
        return world;
    }

    void Engine::SetActiveWorld(World* world)
    {
        if (worldManager)
        {
            worldManager->SetActiveWorld(world);
        }
    }

    void Engine::SetAssetRoot(const std::string& assetRoot)
    {
        config.assetRoot = assetRoot;
        if (worldManager)
        {
            worldManager->ForEachWorld([&](World& world) {
                if (auto* particles = world.GetSystem<ParticleSystem>())
                    particles->SetAssetRoot(assetRoot);
                if (auto* locomotion = world.GetSystem<LocomotionSystem>())
                    locomotion->SetAssetRoot(assetRoot);
                if (RenderSystem* rs = world.GetSystem<RenderSystem>())
                {
                    rs->SetAssetRoot(assetRoot);
                }
            });
        }
    }

    World* Engine::GetActiveWorld()
    {
        return worldManager ? worldManager->GetActiveWorld() : nullptr;
    }

    void Engine::SetViewCamera(ViewCamera which)
    {
        viewCamera = which;
        Camera* camera = (which == ViewCamera::Editor) ? editorCamera.get() : defaultCamera.get();
        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            if (!camera)
            {
                // Initialize 전에 불렸다 - 선택만 기억해 두고 초기화 때 적용된다(InitializeCoreSystems 6-3).
                return;
            }
            renderer->SetMainCamera(camera);
            renderer->SetDrawEditorGizmos(which == ViewCamera::Editor);
        }
    }

    void Engine::SetEditorCameraView(float eyeX, float eyeY, float eyeZ, float targetX, float targetY, float targetZ)
    {
        if (!editorCamera)
        {
            Logger::Warning("Engine::SetEditorCameraView - called before Initialize; ignored");
            return;
        }
        editorCamera->setPosition(glm::vec3(eyeX, eyeY, eyeZ));
        editorCamera->lookAt(glm::vec3(targetX, targetY, targetZ));
    }

    std::optional<PickRay> Engine::GetViewRay(float ndcX, float ndcY) const
    {
        const Camera* camera = (viewCamera == ViewCamera::Editor) ? editorCamera.get() : defaultCamera.get();
        if (!camera)
        {
            return std::nullopt;   // Initialize 전
        }
        return MakePickRay(camera->getViewMatrix(), camera->getProjectionMatrix(), ndcX, ndcY);
    }

    std::optional<PickHit> Engine::PickEntityAtScreen(float ndcX, float ndcY)
    {
        World* world = GetActiveWorld();
        const auto ray = GetViewRay(ndcX, ndcY);
        if (!world || !ray)
        {
            return std::nullopt;
        }
        // 메시 경계는 그 메시를 그리는 RenderSystem에 묻는다 - 그려진 모양과 피킹 모양이 같은 곳에서 나온다.
        const RenderSystem* renderSystem = world->GetSystem<RenderSystem>();
        MeshBoundsLookup bounds;
        if (renderSystem)
        {
            bounds = [renderSystem](const RenderableComponent& rc) { return renderSystem->GetMeshBounds(rc); };
        }
        return PickEntity(*world->GetRegistry(), *ray, bounds);
    }

    void Engine::UpdateTransformGizmo()
    {
        gizmo.reset();
        gizmoTarget = Entity();
        World* world = GetActiveWorld();
        if (!world || !editorCamera || viewCamera != ViewCamera::Editor
            || world->GetEditorState() != World::EditorState::Edit || selectedEntities.size() != 1)
        {
            return;
        }
        ECSRegistry& registry = *world->GetRegistry();
        const Entity target = selectedEntities.front();
        if (!registry.GetTransformComponent(target))
        {
            return;   // 지워졌거나(Undo 등) 위치가 없는 엔티티
        }
        const TransformComponent worldTransform = ComputeWorldTransform(registry, target);
        const glm::vec3 origin = glm::vec3(ComputeWorldMatrix(registry, target)[3]);
        const glm::quat orientation(worldTransform.rotation.w, worldTransform.rotation.x, worldTransform.rotation.y, worldTransform.rotation.z);
        gizmo = MakeTransformGizmo(gizmoMode, origin, orientation, editorCamera->getViewMatrix(),
                                   editorCamera->getProjectionMatrix());
        if (!gizmo)
        {
            return;   // 카메라 뒤
        }
        gizmoTarget = target;
        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            // X 빨강, Y 초록, Z 파랑, 전체 크기(가운데 상자) 밝은 회색. 마우스가 올라간(또는 끌고 있는) 손잡이는 노랑.
            auto colorOf = [](GizmoHandle handle) {
                switch (handle)
                {
                case GizmoHandle::X: return glm::vec3(0.95f, 0.30f, 0.30f);
                case GizmoHandle::Y: return glm::vec3(0.40f, 0.90f, 0.35f);
                case GizmoHandle::Z: return glm::vec3(0.35f, 0.55f, 1.0f);
                default: return glm::vec3(0.85f);
                }
            };
            for (GizmoHandle handle : GizmoHandles(gizmoMode))
            {
                Renderer::ColoredLines group;
                group.color = handle == gizmoHighlight ? glm::vec3(1.0f, 0.85f, 0.2f) : colorOf(handle);
                AppendGizmoHandleLines(*gizmo, handle, group.lines);
                renderer->GetOverlayLines().push_back(std::move(group));
            }
        }
    }

    std::optional<Engine::GizmoPick> Engine::PickGizmoAt(float ndcX, float ndcY) const
    {
        if (!gizmo || !editorCamera)
        {
            return std::nullopt;
        }
        const glm::vec2 viewport(static_cast<float>(viewportPixelWidth ? viewportPixelWidth : config.windowWidth),
                                 static_cast<float>(viewportPixelHeight ? viewportPixelHeight : config.windowHeight));
        const GizmoHandle handle = PickGizmoHandle(*gizmo, editorCamera->getViewMatrix(),
                                                   editorCamera->getProjectionMatrix(), ndcX, ndcY, viewport);
        if (handle == GizmoHandle::None)
        {
            return std::nullopt;
        }
        return GizmoPick{handle, gizmoTarget, *gizmo};
    }

    void Engine::SetRenderMode(RenderMode mode)
    {
        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            renderer->SetRenderMode(mode);
        }
    }

    RenderMode Engine::GetRenderMode()
    {
        if (Renderer* renderer = GetSubsystem<Renderer>())
        {
            return renderer->GetRenderMode();
        }
        return RenderMode::Scene;
    }

} // namespace Engine
