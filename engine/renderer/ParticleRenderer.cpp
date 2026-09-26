#include "ParticleRenderer.h"
#include "InstancedBatchManager.h"
#include "Camera.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>

namespace Engine
{
    namespace
    {
        // location 0~11을 쓰지 않는 것까지 전부 선언한다. 기존 인스턴스 셰이더
        // (pbr_instanced.vert, SceneMeshRenderer의 인라인 셰이더)의 관례이기도 하고,
        // VAO 레이아웃과 셰이더가 어긋났을 때 알아채기 쉽게 하기 위해서다.
        //
        // location 12(aAlpha)는 Phase 0에서 InstanceData의 _padding 자리에 넣은 것이다.
        // **이 셰이더가 그 attribute를 실제로 소비하는 첫 코드다** - 여기까지 값이
        // 도달하지 못하면 파티클이 수명 내내 같은 밝기로 보인다(사라지지 않는다).
        const char* kVertexSrc = R"GLSL(
#version 330 core
layout(location = 0)  in vec3  aPos;
layout(location = 1)  in vec3  aNormal;        // 안 씀
layout(location = 2)  in vec2  aTexCoords;
layout(location = 3)  in mat4  aModel;         // 3,4,5,6
layout(location = 7)  in vec3  aInstanceColor;
layout(location = 8)  in float aRoughness;     // 안 씀
layout(location = 9)  in float aMetallic;      // 안 씀
layout(location = 10) in uint  aEntityId;      // 안 씀
layout(location = 11) in uint  aIsSelected;    // 안 씀
layout(location = 12) in float aAlpha;

out vec2  TexCoords;
out vec3  Color;
out float Alpha;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    Color     = aInstanceColor;
    Alpha     = aAlpha;

    // 빌보드 회전은 CPU(ParticleSystem::CollectInstances)가 aModel에 이미 넣어뒀다.
    gl_Position = projection * view * aModel * vec4(aPos, 1.0);
}
)GLSL";

        // 텍스처를 쓰지 않는다(docs/VFX_LITE_PLAN.md §6.1 확정) - 스프라이트 모양을
        // 쿼드 UV로 절차적으로 만든다. 나중에 텍스처가 생기면 mask를
        // texture(uSprite, TexCoords).a로 바꾸는 것으로 끝난다.
        //
        // 가산 혼합(GL_ONE, GL_ONE)이라 출력의 알파 채널은 무시된다 - 수명에 따른 페이드는
        // **밝기**로 표현한다(Color * Alpha). 그래서 알파가 0에 가까워지면 파티클이 배경에
        // 녹아 사라진다. 알파 블렌딩을 쓰지 않는 이유는 구현 계획서 §2.2에 있다:
        // RenderBatches가 배치를 unordered_map 순서로 도는 탓에 배치 간 그리는 순서가
        // 비결정적인데, 가산 혼합은 교환법칙이 성립해서 그 문제를 회피한다.
        const char* kFragmentSrc = R"GLSL(
#version 330 core
in vec2  TexCoords;
in vec3  Color;
in float Alpha;
out vec4 FragColor;

void main()
{
    float d    = length(TexCoords - vec2(0.5));
    float mask = 1.0 - smoothstep(0.35, 0.5, d);
    if (mask <= 0.0) discard;   // 모서리 픽셀은 블렌딩 비용조차 내지 않는다

    FragColor = vec4(Color * Alpha * mask, 1.0);
}
)GLSL";
    }

    bool ParticleRenderer::Initialize()
    {
        if (initialized)
        {
            return true;
        }

        if (!shader.loadFromSource(kVertexSrc, kFragmentSrc))
        {
            Logger::Log(LogLevel::Error, "ParticleRenderer::Initialize - Shader compile/link failed");
            return false;
        }

        initialized = true;
        Logger::Log(LogLevel::Info, "ParticleRenderer::Initialize - Ready");
        return true;
    }

    void ParticleRenderer::Shutdown()
    {
        // Shader 소멸자가 glDeleteProgram을 처리한다 - 여기선 상태 플래그만 내린다.
        initialized = false;
    }

    void ParticleRenderer::Render(InstancedBatchManager& batchManager, const Camera& camera)
    {
        if (!initialized)
        {
            return;
        }

        batchManager.UpdateDynamicBatches();

        // 이전 상태를 읽어둔다 - DebugGridRenderer/BoneLineRenderer가 쓰는 관례.
        // 여기서 바꾼 상태를 흘리면 이 뒤에 그리는 모든 것(RenderGraph 패스 등)이
        // 영향을 받는다.
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        GLboolean depthMaskWas = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWas);
        GLint blendSrcWas = GL_ONE, blendDstWas = GL_ZERO;
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcWas);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstWas);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);   // 가산 혼합 - 순서와 무관하게 같은 결과가 나온다

        // Z-Test는 유지하고(파티클이 벽 뒤에 있으면 가려져야 한다) Z-Write만 끈다.
        // 켜두면 먼저 그려진 파티클이 뒤 파티클을 깊이로 잘라내서, 정렬이 없는 상태에서는
        // 프레임마다 결과가 달라진다.
        glDepthMask(GL_FALSE);

        shader.use();
        shader.setMat4("view", camera.getViewMatrix());
        shader.setMat4("projection", camera.getProjectionMatrix());

        // ForwardTransparent 배치만 그린다. SceneMeshRenderer의
        // RenderBatches(ForwardOpaque, ...)와 대상이 겹치지 않는다.
        batchManager.RenderBatches(PassType::ForwardTransparent, &shader);

        glDepthMask(depthMaskWas);
        glBlendFunc(static_cast<GLenum>(blendSrcWas), static_cast<GLenum>(blendDstWas));
        if (!blendWasEnabled)
        {
            glDisable(GL_BLEND);
        }
    }
}
