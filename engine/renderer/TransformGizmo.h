#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Engine
{
    // 에디터 변환 기즈모(이동·회전·크기, docs/VIEWPORT_MANIPULATION_PLAN.md). 그리기와 잡기(피킹)가 같은 형상을
    // 쓰도록 계산을 여기 한 곳에 둔다 - 그린 손잡이와 잡히는 손잡이가 어긋나면 "눌렀는데 안 잡힘"이 된다.
    // GL을 쓰지 않는다(Renderer가 선분을 받아 그린다). 화면에서의 크기는 카메라 거리와 상관없이 일정하다.
    //
    //   Move  : X·Y·Z 화살표. 월드 축.
    //   Rotate: 축마다 고리(그 축에 수직인 원). 월드 축.
    //   Scale : 끝에 상자가 달린 X·Y·Z 손잡이 + 가운데 상자(전체 크기). 엔티티의 로컬 축(크기는 로컬 값이다).

    enum class GizmoMode : uint8_t { Move = 0, Rotate = 1, Scale = 2 };
    enum class GizmoHandle : uint8_t { None = 0, X = 1, Y = 2, Z = 3, Uniform = 4 };

    struct TransformGizmo
    {
        GizmoMode mode = GizmoMode::Move;
        glm::vec3 origin{0.0f};
        glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};   // 축 방향(Move/Rotate는 항등 = 월드 축)
        float length = 1.0f;   // 손잡이 하나의 월드 길이(카메라 거리에 비례)

        static constexpr float kScreenFraction = 0.12f;   // 손잡이 길이 = 화면 높이의 이 비율
        static constexpr float kPickTolerancePx = 9.0f;   // 손잡이 선에서 이 픽셀 안이면 잡힌다
        static constexpr float kRingRadius = 0.9f;        // 회전 고리 반지름(length 배)

        // X·Y·Z 손잡이의 월드 방향(단위 벡터). Uniform·None은 0.
        glm::vec3 AxisDirection(GizmoHandle handle) const;
        glm::vec3 AxisTip(GizmoHandle handle) const { return origin + AxisDirection(handle) * length; }
    };

    // 카메라(view, projection)로 볼 때 origin에 놓을 기즈모. orientation은 Scale 모드에서만 쓰인다(나머지는 월드 축).
    // origin이 카메라 뒤(또는 near 안)면 nullopt - 그리지도 잡지도 않는다.
    std::optional<TransformGizmo> MakeTransformGizmo(GizmoMode mode, const glm::vec3& origin, const glm::quat& orientation,
                                                     const glm::mat4& view, const glm::mat4& projection);

    // 손잡이 하나의 선분. 선 굵기 1px로도 잡을 곳이 보이게 축대는 가는 관, 고리는 두 겹으로 그린다.
    void AppendGizmoHandleLines(const TransformGizmo& gizmo, GizmoHandle handle,
                                std::vector<std::pair<glm::vec3, glm::vec3>>& out);

    // 이 모드에 있는 손잡이들(그리는 순서).
    std::vector<GizmoHandle> GizmoHandles(GizmoMode mode);

    // 화면 점(정규화 장치 좌표) 아래의 손잡이. 화면에서 가장 가까운 것을 고르고, 허용 거리(픽셀) 밖이면 None.
    // 카메라 쪽을 똑바로 향해 화면에서 점처럼 보이는 축은 잡지 않는다(끌어도 방향이 정해지지 않는다).
    GizmoHandle PickGizmoHandle(const TransformGizmo& gizmo, const glm::mat4& view, const glm::mat4& projection,
                                float ndcX, float ndcY, glm::vec2 viewportPixels);
}
