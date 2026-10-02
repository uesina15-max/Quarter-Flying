#pragma once

#include <glm/vec3.hpp>
#include <utility>
#include <vector>

namespace Engine
{
    /// <summary>
    /// 한 프레임 동안 그릴 에디터용 디버그 선분 목록(카메라 기즈모 등). Renderer가 소유한다.
    /// ECS System이 Update에서 Add로 채우고, Renderer가 그 프레임을 그린 뒤 Clear한다.
    /// 에디터 카메라로 볼 때만 그린다(Renderer::SetDrawEditorGizmos). 게임 화면에는 나오지 않는다.
    /// </summary>
    class DebugLineBuffer
    {
    public:
        using Line = std::pair<glm::vec3, glm::vec3>;

        void Add(const glm::vec3& a, const glm::vec3& b) { lines_.emplace_back(a, b); }
        const std::vector<Line>& Lines() const { return lines_; }
        void Clear() { lines_.clear(); }

    private:
        std::vector<Line> lines_;
    };
}
