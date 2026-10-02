#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <utility>
#include <vector>

namespace Engine
{
    /// <summary>
    /// 스켈레톤의 본 계층을 GL_LINES로 그리는 디버그 렌더러 (착수 계약서 §C12).
    ///
    /// "line strip"이 아니다 - 본 하나당 독립된 선분 하나(parent.world.position ->
    /// bone.world.position)를 GL_LINES로 그린다(계약서가 명시적으로 "line strip이라는
    /// 표현은 사용하지 않는다"고 못박아 둠 - 전체 뼈대가 하나의 연속선처럼 오해되는 걸 막기 위함).
    ///
    /// DebugGridRenderer와 같은 이유로 CommandList 추상화(GL_TRIANGLES 하드코딩)를 거치지
    /// 않고 직접 OpenGL을 호출한다. 그리드와 달리 매 프레임 선 목록이 바뀌므로(포즈가
    /// 애니메이션됨) 정점 버퍼를 매 Render() 호출마다 다시 채운다.
    /// </summary>
    class BoneLineRenderer
    {
    public:
        BoneLineRenderer() = default;
        ~BoneLineRenderer();

        BoneLineRenderer(const BoneLineRenderer&) = delete;
        BoneLineRenderer& operator=(const BoneLineRenderer&) = delete;

        bool Initialize();
        void Shutdown();

        // lines: (parent.world.position, bone.world.position) 쌍의 목록.
        // color: 선 색(기본 = 본 라인용 밝은 노랑). 카메라 기즈모 등 다른 디버그 선도 이 렌더러를 쓴다.
        void Render(const glm::mat4& viewProjection, const std::vector<std::pair<glm::vec3, glm::vec3>>& lines,
                    const glm::vec3& color = glm::vec3(1.0f, 0.85f, 0.2f));

        bool IsInitialized() const { return initialized; }

    private:
        unsigned int CompileShaderStage(unsigned int type, const char* source);

        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int shaderProgram = 0;
        int uniformLocViewProjection = -1;
        int uniformLocColor = -1;
        bool initialized = false;
    };
}
