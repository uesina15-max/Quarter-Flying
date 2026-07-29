#pragma once

#include "../core/Subsystem.h"
#include "../core/EngineError.h"
#include "RenderGraph.h"
#include "CommandList.h"
#include "RenderBatchPolicy.h"
#include "InstancedBatchManager.h"
#include "LODSystem.h"
#include "OcclusionCulling.h"
#include "DynamicResolution.h"
#include "Frustum.h"
#include "Camera.h"
#include <memory>

namespace Engine
{
    // Forward declarations
    class JobSystem;

    /**
     * Renderer Subsystem
     * 
     * RenderGraph를 소유하고 관리하는 렌더링 서브시스템입니다.
     * 프레임마다 RenderGraph를 컴파일하고 실행하며,
     * 프레임 종료 시 Transient Resource를 정리합니다.
     * 
     * Performance optimizations:
     * - Frustum culling for large scenes
     * - LOD (Level of Detail) system
     * - Occlusion culling
     * - Instanced batch rendering
     * - Dynamic resolution scaling
     */
    class Renderer : public Subsystem
    {
    public:
        Renderer();
        virtual ~Renderer();

        // Subsystem interface
        Result<void> Initialize() override;
        void Shutdown() noexcept override;
        void Tick(float deltaTime) override;
        void LateTick(float deltaTime) override;

        // Rendering interface
        void Render();
        void RenderWithJobSystem(JobSystem* jobSystem);

        // Instanced batch rendering interface (PR-3)
        Result<void> SubmitInstancedBatch(const RenderBatchKey& batchKey, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t baseInstance = 0);
        Result<void> SubmitIndexedInstancedBatch(const RenderBatchKey& batchKey, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0, uint32_t baseVertex = 0, uint32_t baseInstance = 0);

        // RenderGraph access
        RenderGraph* GetRenderGraph() { return renderGraph.get(); }
        const RenderGraph* GetRenderGraph() const { return renderGraph.get(); }

        // Performance optimization systems access
        InstancedBatchManager* GetInstancedBatchManager() { return instancedBatchManager.get(); }
        const InstancedBatchManager* GetInstancedBatchManager() const { return instancedBatchManager.get(); }
        
        LODSystem* GetLODSystem() { return lodSystem.get(); }
        const LODSystem* GetLODSystem() const { return lodSystem.get(); }
        
        OcclusionCullingSystem* GetOcclusionCullingSystem() { return occlusionCullingSystem.get(); }
        const OcclusionCullingSystem* GetOcclusionCullingSystem() const { return occlusionCullingSystem.get(); }
        
        DynamicResolutionSystem* GetDynamicResolutionSystem() { return dynamicResolutionSystem.get(); }
        const DynamicResolutionSystem* GetDynamicResolutionSystem() const { return dynamicResolutionSystem.get(); }

        // Camera access for culling
        void SetMainCamera(Camera* camera) { mainCamera = camera; }
        Camera* GetMainCamera() const { return mainCamera; }

        // Frame lifecycle
        void BeginFrame();
        void EndFrame();

        // Performance optimization configuration
        void SetEnableFrustumCulling(bool enable) { enableFrustumCulling = enable; }
        bool IsFrustumCullingEnabled() const { return enableFrustumCulling; }
        
        void SetEnableLOD(bool enable) { enableLOD = enable; }
        bool IsLODEnabled() const { return enableLOD; }
        
        void SetEnableOcclusionCulling(bool enable) { enableOcclusionCulling = enable; }
        bool IsOcclusionCullingEnabled() const { return enableOcclusionCulling; }
        
        void SetEnableDynamicResolution(bool enable) { enableDynamicResolution = enable; }
        bool IsDynamicResolutionEnabled() const { return enableDynamicResolution; }

    private:
        // Core rendering components
        std::unique_ptr<RenderGraph> renderGraph;
        std::unique_ptr<CommandList> commandList;
        
        // Performance optimization systems
        std::unique_ptr<InstancedBatchManager> instancedBatchManager;
        std::unique_ptr<LODSystem> lodSystem;
        std::unique_ptr<OcclusionCullingSystem> occlusionCullingSystem;
        std::unique_ptr<DynamicResolutionSystem> dynamicResolutionSystem;
        
        // Camera for culling calculations
        Camera* mainCamera;
        
        // Frustum for culling
        Frustum frustum;
        
        // Feature flags
        bool frameInProgress;
        bool enableFrustumCulling;
        bool enableLOD;
        bool enableOcclusionCulling;
        bool enableDynamicResolution;
    };

} // namespace Engine