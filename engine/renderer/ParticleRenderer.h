#pragma once

#include "Shader.h"

namespace Engine
{
    class InstancedBatchManager;
    class Camera;

    /// <summary>
    /// VFX Lite 파티클을 그린다(docs/VFX_LITE_PHASE3_RENDERING_PLAN.md §3.6/§3.7).
    ///
    /// `ParticleSystem`(ecs)이 `InstancedBatchManager`에 채워둔 `ForwardTransparent` 배치를
    /// 받아서 셰이더와 렌더 상태만 담당한다 — `SceneMeshRenderer`가 `RenderSystem`에 대해
    /// 하는 것과 정확히 같은 역할이고, 같은 이유로 `renderer/`는 ECS를 알지 않는다.
    ///
    /// 셰이더를 파일이 아니라 소스 문자열로 컴파일하는 것도 `SceneMeshRenderer`/
    /// `DebugGridRenderer`와 같은 이유다 — 상대경로가 프로세스 CWD 기준인데 에디터는
    /// `engine/editor/`에서 실행되므로 파일 경로 방식은 항상 깨진다.
    ///
    /// **렌더 상태**: 이 클래스가 자기 draw 앞뒤에서만 블렌드/깊이 쓰기를 바꾸고 반드시
    /// 원복한다. `InstancedBatchManager`에 상태 전환을 넣지 않은 이유는, 그러면 지금
    /// 유일하게 잘 도는 불투명 경로(`ForwardOpaque`)까지 영향을 받기 때문이다
    /// (구현 계획서 §2.2 확정 ①).
    /// </summary>
    class ParticleRenderer
    {
    public:
        ParticleRenderer() = default;
        ~ParticleRenderer() = default;

        ParticleRenderer(const ParticleRenderer&) = delete;
        ParticleRenderer& operator=(const ParticleRenderer&) = delete;

        bool Initialize();
        void Shutdown();
        bool IsInitialized() const { return initialized; }

        // ParticleSystem이 이번 프레임에 채워둔 파티클 배치를 그린다. SceneMeshRenderer와
        // 마찬가지로 UpdateDynamicBatches()(dirty 배치 GPU 업로드)까지 이 안에서 처리한다.
        void Render(InstancedBatchManager& batchManager, const Camera& camera);

    private:
        Shader shader;
        bool initialized = false;
    };
}
