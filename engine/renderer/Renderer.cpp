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

#include "Renderer.h"
#include "../core/logging/Logger.h"
#include "../job/JobSystem.h"
#include "opengl/OpenGLCommandList.h"
#include <GL/glew.h>
#include <memory>

namespace Engine
{
    Renderer::Renderer()
        : mainCamera(nullptr)
        , frameInProgress(false)
        , enableFrustumCulling(true)
        , enableLOD(true)
        , enableOcclusionCulling(false)  // Disabled by default, needs explicit enable
        , enableDynamicResolution(false)  // Disabled by default
    {
    }

    Renderer::~Renderer()
    {
        if (frameInProgress)
        {
            EndFrame();
        }
    }

    Result<void> Renderer::Initialize()
    {
        Logger::Log(LogLevel::Info, "Renderer::Initialize - Initializing Renderer subsystem");

        // 전제조건: GLEW가 로드된 GL 컨텍스트가 current여야 한다(Engine은 CreateGraphicsContext 후에
        // 이 함수를 부른다).
        // 증상: RendererInstancingTest 3개가 SetUp()에서 "SEH exception with code 0xc0000005"로
        // 죽었다. 전체 스위트에서는 앞선 테스트가 남긴 GL 상태 덕에 1개만 실패해서 "기존 무관 실패"로
        // 방치돼 있었다(실행 순서에 따라 결과가 달라짐).
        // 원인: 컨텍스트가 없으면 GLEW 함수 포인터가 null이다. 그 상태로 아래
        // OpenGLCommandList::Reset()이 GL을 호출하다 크래시했다.
        // glGetString은 opengl32.dll이 직접 export하는 함수라 컨텍스트 없이 불러도 안전하며, 이때 NULL을 돌려준다.
        if (glGetString(GL_VERSION) == nullptr || glGenVertexArrays == nullptr)
        {
            Logger::Log(LogLevel::Error, "Renderer::Initialize - no current OpenGL context (or GLEW not initialized); "
                                         "create the graphics context before initializing the Renderer");
            return MakeUnexpected(EngineErrorCode::InvalidState,
                "Renderer::Initialize requires a current OpenGL context with GLEW initialized", "Renderer");
        }

        // Create RenderGraph
        renderGraph = std::make_unique<RenderGraph>();
        
        // Create OpenGL CommandList
        commandList = std::make_unique<OpenGLCommandList>();

        // Create performance optimization systems
        instancedBatchManager = std::make_unique<InstancedBatchManager>();
        auto batchResult = instancedBatchManager->Initialize();
        if (!batchResult)
        {
            Logger::Log(LogLevel::Error, "Renderer::Initialize - Failed to initialize InstancedBatchManager");
            return batchResult;
        }

        lodSystem = std::make_unique<LODSystem>();
        auto lodResult = lodSystem->Initialize();
        if (!lodResult)
        {
            Logger::Log(LogLevel::Error, "Renderer::Initialize - Failed to initialize LODSystem");
            return lodResult;
        }

        occlusionCullingSystem = std::make_unique<OcclusionCullingSystem>();
        auto occlusionResult = occlusionCullingSystem->Initialize();
        if (!occlusionResult)
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize OcclusionCullingSystem (may not be supported)");
            // Continue even if occlusion culling fails (may not be supported)
        }

        // Initialize dynamic resolution system with default 1080p
        dynamicResolutionSystem = std::make_unique<DynamicResolutionSystem>();
        auto drResult = dynamicResolutionSystem->Initialize(1920, 1080);
        if (!drResult)
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize DynamicResolutionSystem");
            // Continue even if dynamic resolution fails
        }

        // 임시 디버그 그리드 초기화. 실패해도 렌더러 자체는 계속 동작해야 하므로
        // (그리드는 있으면 좋은 진단용 시각화일 뿐 필수 기능이 아님) 치명 에러로 취급하지 않는다.
        debugGrid = std::make_unique<DebugGridRenderer>();
        if (!debugGrid->Initialize())
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize DebugGridRenderer, grid will not be drawn");
        }

        // Motion Mixer 본 라인 렌더러 (Phase 4A). 마찬가지로 실패해도 치명 에러 취급 안 함.
        boneLineRenderer = std::make_unique<BoneLineRenderer>();
        if (!boneLineRenderer->Initialize())
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize BoneLineRenderer, motion preview will not be drawn");
        }

        // ECS RenderSystem이 채운 InstancedBatchManager 배치를 실제로 그리는 렌더러 (ROADMAP.md P0-2).
        // 실패해도(셰이더 컴파일 실패 등) 치명 에러 취급 안 함 - 그리드/기존 기능은 계속 동작해야 함.
        sceneMeshRenderer = std::make_unique<SceneMeshRenderer>();
        if (!sceneMeshRenderer->Initialize())
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize SceneMeshRenderer, scene meshes will not be drawn");
        }

        // VFX Lite Phase 3. sceneMeshRenderer와 같은 이유로 실패해도 치명 에러 취급 안 함 -
        // 파티클이 안 그려질 뿐 그리드/씬 메시는 계속 나와야 한다.
        particleRenderer = std::make_unique<ParticleRenderer>();
        if (!particleRenderer->Initialize())
        {
            Logger::Log(LogLevel::Warning, "Renderer::Initialize - Failed to initialize ParticleRenderer, particles will not be drawn");
        }

        frameInProgress = false;

        Logger::Log(LogLevel::Info, "Renderer::Initialize - Renderer subsystem initialized successfully");
        return {};
    }

    void Renderer::Shutdown() noexcept
    {
        Logger::Log(LogLevel::Info, "Renderer::Shutdown - Shutting down Renderer subsystem");

        // End frame if in progress
        if (frameInProgress)
        {
            EndFrame();
        }

        // Clean up performance optimization systems
        if (instancedBatchManager)
        {
            instancedBatchManager->Shutdown();
            instancedBatchManager.reset();
        }

        if (lodSystem)
        {
            lodSystem->Shutdown();
            lodSystem.reset();
        }

        if (occlusionCullingSystem)
        {
            occlusionCullingSystem->Shutdown();
            occlusionCullingSystem.reset();
        }

        if (dynamicResolutionSystem)
        {
            dynamicResolutionSystem->Shutdown();
            dynamicResolutionSystem.reset();
        }

        if (debugGrid)
        {
            debugGrid->Shutdown();
            debugGrid.reset();
        }

        if (boneLineRenderer)
        {
            boneLineRenderer->Shutdown();
            boneLineRenderer.reset();
        }

        if (sceneMeshRenderer)
        {
            sceneMeshRenderer->Shutdown();
            sceneMeshRenderer.reset();
        }

        // Clean up RenderGraph
        if (renderGraph)
        {
            renderGraph->Reset();
            renderGraph.reset();
        }

        // Clean up CommandList
        if (commandList)
        {
            commandList.reset();
        }

        Logger::Log(LogLevel::Info, "Renderer::Shutdown - Renderer subsystem shutdown complete");
    }

    void Renderer::Tick(float deltaTime)
    {
        // Begin frame for rendering
        BeginFrame();
        
        // Update dynamic resolution based on previous frame time
        if (enableDynamicResolution && dynamicResolutionSystem)
        {
            // Convert deltaTime (seconds) to frame time (milliseconds)
            float frameTimeMs = deltaTime * 1000.0f;
            dynamicResolutionSystem->Update(frameTimeMs);
        }
        
        // Update performance optimization systems
        if (mainCamera)
        {
            // Update frustum for culling
            if (enableFrustumCulling)
            {
                glm::mat4 viewMatrix = mainCamera->getViewMatrix();
                glm::mat4 projectionMatrix = mainCamera->getProjectionMatrix();
                glm::mat4 viewProjection = projectionMatrix * viewMatrix;
                frustum.Update(viewProjection);
            }
            
            // Update LOD system
            if (enableLOD && lodSystem)
            {
                lodSystem->UpdateLODs(*mainCamera);
            }
        }
        else
        {
            Logger::Log(LogLevel::Warning, "Renderer::Tick - mainCamera is null, skipping camera-dependent updates");
        }
        
        // Update instance batch visibility with frustum culling
        if (enableFrustumCulling && instancedBatchManager)
        {
            instancedBatchManager->UpdateVisibility(frustum);
        }
        
        // Begin occlusion culling frame
        if (enableOcclusionCulling && occlusionCullingSystem)
        {
            occlusionCullingSystem->BeginFrame();
        }
    }

    void Renderer::LateTick(float deltaTime)
    {
        // Perform rendering and end frame
        Render();
        
        // End occlusion culling frame
        if (enableOcclusionCulling && occlusionCullingSystem)
        {
            occlusionCullingSystem->EndFrame();
        }
        
        EndFrame();
    }

    void Renderer::Render()
    {
        if (!renderGraph || !commandList)
        {
            Logger::Log(LogLevel::Warning, "Renderer::Render - RenderGraph or CommandList not initialized");
            return;
        }

        if (!frameInProgress)
        {
            Logger::Log(LogLevel::Warning, "Renderer::Render - No frame in progress, call BeginFrame first");
            return;
        }

        Logger::Log(LogLevel::Trace, "Renderer::Render - Starting render execution");

        // 매 프레임 glViewport 재적용 (Renderer.h의 SetViewportSize 주석 참고 - 이 프로세스
        // 안에 EngineViewport(=Engine 인스턴스, GL 컨텍스트)가 여러 개 있으면 다른 인스턴스가
        // 남긴 엉뚱한 크기의 viewport가 이 시점에 걸려있을 수 있다).
        if (viewportWidth > 0 && viewportHeight > 0)
        {
            glViewport(0, 0, static_cast<GLsizei>(viewportWidth), static_cast<GLsizei>(viewportHeight));
        }

        // 화면 지우기 + 디버그 그리드. RenderGraph에 등록된 패스가 없어도(현재는 아무도
        // 패스를 채워넣지 않는다 - RenderSystem 부재, ROADMAP.md 참고) 최소한 뷰포트에
        // 뭔가 그려지는 기준선을 보장한다.
        if (commandList)
        {
            commandList->Clear(0.10f, 0.11f, 0.13f, 1.0f);
        }
        if (debugGrid && debugGrid->IsInitialized() && mainCamera)
        {
            glm::mat4 viewProjection = mainCamera->getProjectionMatrix() * mainCamera->getViewMatrix();
            debugGrid->Render(viewProjection);
        }

        // Motion Mixer 프리뷰 (Phase 4A, §C11: "카메라·라이트는 SceneMode와 공유" -
        // 그래서 위 grid와 같은 카메라를 그대로 쓰고, Motion 모드일 때만 본 라인을 덧그린다).
        if (renderMode == RenderMode::Motion && motionPreviewState
            && boneLineRenderer && boneLineRenderer->IsInitialized() && mainCamera)
        {
            glm::mat4 viewProjection = mainCamera->getProjectionMatrix() * mainCamera->getViewMatrix();
            auto lines = motionPreviewState->ComputeBoneWorldLines();
            boneLineRenderer->Render(viewProjection, lines);
        }

        // ECS RenderSystem이 이번 프레임에 InstancedBatchManager로 채운 씬 메시를 그린다
        // (ROADMAP.md P0-2). Scene 모드일 때만 - Motion 모드에서는 본 라인이 그 자리를 대신한다.
        if (renderMode == RenderMode::Scene && sceneMeshRenderer && sceneMeshRenderer->IsInitialized()
            && instancedBatchManager && mainCamera)
        {
            sceneMeshRenderer->Render(*instancedBatchManager, *mainCamera);

            // 불투명 다음에 투명(VFX Lite 구현 계획서 §2.2 확정 ③). 배치 사이의 순서는
            // InstancedBatchManager가 보장하지 않으므로, 불투명 → 파티클 순서는 여기
            // 호출 순서로만 보장된다.
            if (particleRenderer && particleRenderer->IsInitialized())
            {
                particleRenderer->Render(*instancedBatchManager, *mainCamera);
            }
        }

        // Check if RenderGraph has passes to execute
        if (renderGraph->GetPassCount() == 0)
        {
            Logger::Log(LogLevel::Trace, "Renderer::Render - No render passes to execute");
            return;
        }

        // Compile RenderGraph if not already compiled
        if (!renderGraph->IsCompiled())
        {
            Logger::Log(LogLevel::Trace, "Renderer::Render - Compiling RenderGraph");
            renderGraph->Compile();

            if (!renderGraph->IsCompiled())
            {
                Logger::Log(LogLevel::Error, "Renderer::Render - Failed to compile RenderGraph");
                return;
            }
        }

        // Execute RenderGraph (without JobSystem for now)
        Logger::Log(LogLevel::Trace, "Renderer::Render - Executing RenderGraph");
        renderGraph->Execute(*commandList);

        Logger::Log(LogLevel::Trace, "Renderer::Render - Render execution completed");
    }

    void Renderer::RenderWithJobSystem(JobSystem* jobSystem)
    {
        if (!renderGraph || !commandList)
        {
            Logger::Log(LogLevel::Warning, "Renderer::RenderWithJobSystem - RenderGraph or CommandList not initialized");
            return;
        }

        if (!jobSystem)
        {
            Logger::Log(LogLevel::Warning, "Renderer::RenderWithJobSystem - JobSystem is null, falling back to sequential rendering");
            Render();
            return;
        }

        if (!frameInProgress)
        {
            Logger::Log(LogLevel::Warning, "Renderer::RenderWithJobSystem - No frame in progress, call BeginFrame first");
            return;
        }

        Logger::Log(LogLevel::Trace, "Renderer::RenderWithJobSystem - Starting parallel render execution");

        // Check if RenderGraph has passes to execute
        if (renderGraph->GetPassCount() == 0)
        {
            Logger::Log(LogLevel::Trace, "Renderer::RenderWithJobSystem - No render passes to execute");
            return;
        }

        // Compile RenderGraph if not already compiled
        if (!renderGraph->IsCompiled())
        {
            Logger::Log(LogLevel::Trace, "Renderer::RenderWithJobSystem - Compiling RenderGraph");
            renderGraph->Compile();

            if (!renderGraph->IsCompiled())
            {
                Logger::Log(LogLevel::Error, "Renderer::RenderWithJobSystem - Failed to compile RenderGraph");
                return;
            }
        }

        // Execute RenderGraph with JobSystem
        Logger::Log(LogLevel::Trace, "Renderer::RenderWithJobSystem - Executing RenderGraph with parallel jobs");
        renderGraph->ExecuteParallel(*commandList, jobSystem);

        Logger::Log(LogLevel::Trace, "Renderer::RenderWithJobSystem - Parallel render execution completed");
    }

    Result<void> Renderer::SubmitInstancedBatch(const RenderBatchKey& batchKey, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t baseInstance)
    {
        if (!commandList)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "CommandList is not initialized", "Renderer");
        }
        if (!frameInProgress)
        {
            return MakeUnexpected(EngineErrorCode::InvalidState, "No frame in progress", "Renderer");
        }
        if (batchKey.shaderId == 0 && batchKey.meshGuid == 0)
        {
            return MakeUnexpected(EngineErrorCode::InvalidBatchKey, "Invalid batch key provided", "Renderer");
        }

        commandList->SetShader(batchKey.shaderId);
        commandList->DrawArraysInstanced(vertexCount, instanceCount, startVertex, baseInstance);
        return {};
    }

    Result<void> Renderer::SubmitIndexedInstancedBatch(const RenderBatchKey& batchKey, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, uint32_t baseVertex, uint32_t baseInstance)
    {
        if (!commandList)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "CommandList is not initialized", "Renderer");
        }
        if (!frameInProgress)
        {
            return MakeUnexpected(EngineErrorCode::InvalidState, "No frame in progress", "Renderer");
        }
        if (batchKey.shaderId == 0 && batchKey.meshGuid == 0)
        {
            return MakeUnexpected(EngineErrorCode::InvalidBatchKey, "Invalid batch key provided", "Renderer");
        }

        commandList->SetShader(batchKey.shaderId);
        commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, baseInstance);
        return {};
    }

    void Renderer::BeginFrame()
    {
        if (frameInProgress)
        {
            Logger::Log(LogLevel::Warning, "Renderer::BeginFrame - Frame already in progress, ending previous frame");
            EndFrame();
        }

        Logger::Log(LogLevel::Trace, "Renderer::BeginFrame - Starting new render frame");

        // Reset RenderGraph for new frame
        if (renderGraph)
        {
            renderGraph->Reset();
        }

        // Reset CommandList for new frame
        if (commandList)
        {
            commandList->Reset();
        }

        frameInProgress = true;

        Logger::Log(LogLevel::Trace, "Renderer::BeginFrame - Render frame started");
    }

    void Renderer::EndFrame()
    {
        if (!frameInProgress)
        {
            Logger::Log(LogLevel::Warning, "Renderer::EndFrame - No frame in progress");
            return;
        }

        Logger::Log(LogLevel::Trace, "Renderer::EndFrame - Ending render frame");

        // Task 13.4: Transient Resource 정리 구현
        // 프레임 종료 시 리소스 해제 및 풀 반환
        if (renderGraph)
        {
            Logger::Log(LogLevel::Trace, "Renderer::EndFrame - Cleaning up transient resources");

            // 1. Transient Resource 해제
            renderGraph->ReleaseTransientResources();

            // 2. 리소스 풀 반환
            renderGraph->ReturnResourcesToPool();

            Logger::Log(LogLevel::Trace, "Renderer::EndFrame - Transient resource cleanup completed");
        }

        // Finalize CommandList
        if (commandList)
        {
            commandList->Finalize();
        }

        frameInProgress = false;

        Logger::Log(LogLevel::Trace, "Renderer::EndFrame - Render frame ended");
    }

} // namespace Engine