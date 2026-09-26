#pragma once

#include <glm/mat4x4.hpp>

namespace Engine
{
    /// <summary>
    /// 3D 뷰포트에 바닥 그리드를 그리는 임시 디버그 렌더러.
    ///
    /// ECS RenderableComponent -> draw call을 잇는 RenderSystem이 아직 없어서
    /// (ROADMAP.md 참고), "렌더링 파이프라인 자체는 살아서 화면에 뭔가 그린다"를
    /// 눈으로 확인할 수 있는 최소 기준선 역할을 한다.
    ///
    /// CommandList 추상화의 DrawArrays/Draw는 지금 GL_TRIANGLES로 하드코딩되어 있어
    /// 라인 그리기에 쓸 수 없다 - 그래서 이 클래스는 CommandList를 거치지 않고
    /// OpenGL을 직접 호출한다 (자체 VAO/VBO/셰이더 소유).
    /// </summary>
    class DebugGridRenderer
    {
    public:
        DebugGridRenderer() = default;
        ~DebugGridRenderer();

        DebugGridRenderer(const DebugGridRenderer&) = delete;
        DebugGridRenderer& operator=(const DebugGridRenderer&) = delete;

        // halfSize/spacing 단위는 월드 유닛. 기본값: -10..10 범위에 1유닛 간격 격자.
        bool Initialize(float halfSize = 10.0f, float spacing = 1.0f);
        void Shutdown();

        // viewProjection = projectionMatrix * viewMatrix (모델은 항상 identity - 그리드는 원점 고정).
        void Render(const glm::mat4& viewProjection);

        bool IsInitialized() const { return initialized; }

    private:
        unsigned int CompileShaderStage(unsigned int type, const char* source);

        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int shaderProgram = 0;
        int vertexCount = 0;
        int uniformLocViewProjection = -1;
        bool initialized = false;
    };
}
