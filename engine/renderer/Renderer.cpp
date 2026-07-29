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