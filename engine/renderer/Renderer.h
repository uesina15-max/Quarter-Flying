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
#include "DebugGridRenderer.h"
#include "BoneLineRenderer.h"
#include "SceneMeshRenderer.h"
#include "ParticleRenderer.h"
#include "RenderMode.h"
#include "DebugLineBuffer.h"
#include "../animation/MotionPreviewState.h"
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
        // Submit*에 넘길 shaderId를 만든다(0 = 실패). 이 함수가 생기기 전에는 호출자가 유효한
        // shaderId를 얻을 공개 경로가 없어서 Submit*는 사실상 테스트 전용 API였다.
        uint32_t CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
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

        // Motion Mixer 프리뷰 (Phase 4A, 착수 계약서 §C11)
        void SetRenderMode(RenderMode mode) { renderMode = mode; }
        RenderMode GetRenderMode() const { return renderMode; }

        void SetMotionPreviewState(MotionPreviewState* state) { motionPreviewState = state; }

        // 에디터 기즈모(카메라 시야 선 등). ECS System이 매 프레임 채우고, 여기서 그린 뒤 EndFrame에서 비운다.
        // 에디터 카메라로 볼 때만 그린다(Engine::SetViewCamera가 켜고 끔). 게임 화면에는 나오지 않는다.
        DebugLineBuffer& GetDebugLineBuffer() { return debugLines; }
        void SetDrawEditorGizmos(bool draw) { drawEditorGizmos = draw; }
        bool IsDrawingEditorGizmos() const { return drawEditorGizmos; }
        MotionPreviewState* GetMotionPreviewState() const { return motionPreviewState; }

        // ROADMAP.md P0-2에서 발견: 씬 에디터/플레이 모드가 각자 별도의 EngineViewport(=
        // 별도 Engine 인스턴스, 별도 GL 컨텍스트)를 갖고 같은 Qt 메인 스레드에서 각자의
        // QTimer로 독립적으로 TickFrame()을 돈다. wglMakeCurrent는 스레드 단위 상태라
        // 어느 한쪽이 자기 컨텍스트를 만들면(InitializeFromWindowHandle) 그 뒤로 다른 쪽이
        // 이전에 만들어둔 "현재 컨텍스트"를 밀어낼 수 있다 - glViewport는 컨텍스트별 상태라
        // 이 경우 실제로 그리는 시점에 완전히 다른(엉뚱한 크기의) viewport가 걸려있을 수
        // 있다(실측: 96x480, 카메라 projection은 1.3 종횡비를 가정 - 실제로 이 버그로
        // 씬 메시가 가로로 심하게 눌려 보였다). 근본 해결(컨텍스트 전환/공유 정리)은
        // 더 큰 작업이라 후속 과제로 남기고, 여기서는 매 프레임 그리기 직전에 우리가 알고
        // 있는 올바른 크기로 glViewport를 다시 걸어서 증상을 막는다.
        void SetViewportSize(uint32_t width, uint32_t height) { viewportWidth = width; viewportHeight = height; }

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

        // 임시 디버그 그리드 (RenderSystem이 생기기 전까지의 최소 렌더링 기준선)
        std::unique_ptr<DebugGridRenderer> debugGrid;

        // ECS RenderSystem이 InstancedBatchManager에 채운 배치를 실제로 그린다 (ROADMAP.md P0-2).
        std::unique_ptr<SceneMeshRenderer> sceneMeshRenderer;

        // ParticleSystem이 채운 ForwardTransparent 배치를 그린다 (VFX Lite Phase 3).
        std::unique_ptr<ParticleRenderer> particleRenderer;

        // 매 프레임 glViewport 재적용용 (SetViewportSize 주석 참고). 0이면 아직 아무도
        // 설정 안 한 것이므로 재적용을 건너뛴다(엔진 초기화 극초반 등).
        uint32_t viewportWidth = 0;
        uint32_t viewportHeight = 0;

        // Motion Mixer 프리뷰 (Phase 4A)
        RenderMode renderMode = RenderMode::Scene;
        MotionPreviewState* motionPreviewState = nullptr;  // 소유하지 않음 - Engine이 소유
        std::unique_ptr<BoneLineRenderer> boneLineRenderer;

        // 에디터 기즈모 (GetDebugLineBuffer 주석 참고)
        DebugLineBuffer debugLines;
        bool drawEditorGizmos = false;
        
        // Feature flags
        bool frameInProgress;
        bool enableFrustumCulling;
        bool enableLOD;
        bool enableOcclusionCulling;
        bool enableDynamicResolution;
    };

} // namespace Engine