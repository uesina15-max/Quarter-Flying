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
#include "../job/JobSystem.h"
#include <string>
#include <memory>

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
        }

        Logger::Info("Engine main loop ended");
    }

    void Engine::PushInputEvent(const InputEvent& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        inputQueue.push_back(event);
    }

    void Engine::Shutdown()
    {
        if (!initialized)
        {
            return;
        }

        Logger::Info("Shutting down Engine...");

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
        Logger::Info("Running Memory Leak Detector...");
        MemoryTracker::Get().ReportLeaks();

        initialized = false;
        Logger::Info("Engine shutdown complete");
    }

    void Engine::TickFrame()
    {
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

        InputEvent event;
        while ((event = platform->GetNextInputEvent()).type != InputEventType::None)
        {
            if (event.type == InputEventType::WindowClose)
            {
                isRunning = false;
                return;
            }
        }

        // 3-1. 외부 UI 스레드로부터 큐잉된 비동기 입력 이벤트 일괄 처리 (Method A: Lock 최소화)
        std::vector<InputEvent> localInputQueue;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            std::swap(localInputQueue, inputQueue);
        }

        for (const auto& queuedEvent : localInputQueue)
        {
            if (queuedEvent.type == InputEventType::WindowClose)
            {
                isRunning = false;
                return;
            }
            // 필요한 경우 향후 InputSystem/World 등으로 이벤트 전달
        }

        // 4. ECS 업데이트 (입력 처리 → ECS 업데이트 → 렌더링 순서)
        if (worldManager)
        {
            worldManager->Update(deltaTime);
        }

        // 5. 서브시스템 Tick (Renderer::Tick에서 BeginFrame 호출)
        for (auto& subsystem : subsystems)
        {
            subsystem->Tick(deltaTime);
        }

        // 6. 서브시스템 LateTick (Renderer::LateTick에서 Render + EndFrame 호출)
        for (auto& subsystem : subsystems)
        {
            subsystem->LateTick(deltaTime);
        }

        // 프레임 시간 로깅 (설정된 간격마다)
        if (config.logFrameInterval > 0 && frameCount % config.logFrameInterval == 0)
        {
            float fps = (deltaTime > 0.0f) ? 1.0f / deltaTime : 0.0f;
            Logger::Info("Frame: {}, FPS: {:.2f}, DeltaTime: {:.2f}ms", frameCount, fps, deltaTime * 1000.0f);
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

    World* Engine::GetActiveWorld()
    {
        return worldManager ? worldManager->GetActiveWorld() : nullptr;
    }

} // namespace Engine