#include "TransformGizmo.h"
#include <glm/glm.hpp>
#include <cmath>
#include <limits>

namespace Engine
{
    namespace
    {
        constexpr float kTwoPi = 6.2831853f;
        constexpr int kRingSegments = 64;

        // dir에 수직인 두 단위 벡터
        void Perpendiculars(const glm::vec3& dir, glm::vec3& u, glm::vec3& v)
        {
            const glm::vec3 helper = std::abs(dir.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            u = glm::normalize(glm::cross(dir, helper));
            v = glm::cross(dir, u);
        }

        void AppendShaft(const glm::vec3& from, const glm::vec3& to, const glm::vec3& u, const glm::vec3& v, float r,
                         std::vector<std::pair<glm::vec3, glm::vec3>>& out)
        {
            out.emplace_back(from, to);
            for (const glm::vec3& offset : {u * r, -u * r, v * r, -v * r})
            {
                out.emplace_back(from + offset, to + offset);
            }
        }

        // 축 a, b, c 방향의 반 크기 h인 상자(모서리 12개)
        void AppendBox(const glm::vec3& center, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float h,
                       std::vector<std::pair<glm::vec3, glm::vec3>>& out)
        {
            glm::vec3 corner[8];
            for (int i = 0; i < 8; ++i)
            {
                corner[i] = center + a * ((i & 1) ? h : -h) + b * ((i & 2) ? h : -h) + c * ((i & 4) ? h : -h);
            }
            for (int i = 0; i < 8; ++i)
            {
                for (int bit : {1, 2, 4})
                {
                    if (!(i & bit)) out.emplace_back(corner[i], corner[i | bit]);
                }
            }
        }

        glm::vec3 RingPoint(const TransformGizmo& gizmo, const glm::vec3& u, const glm::vec3& v, float radius, int i)
        {
            const float a = kTwoPi * static_cast<float>(i) / kRingSegments;
            return gizmo.origin + (u * std::cos(a) + v * std::sin(a)) * radius;
        }

        // 월드 점 -> 화면 픽셀. 카메라 뒤면 nullopt.
        std::optional<glm::vec2> ToPixels(const glm::mat4& viewProjection, const glm::vec3& p, glm::vec2 viewport)
        {
            const glm::vec4 clip = viewProjection * glm::vec4(p, 1.0f);
            if (clip.w <= 1e-6f)
            {
                return std::nullopt;
            }
            const glm::vec2 ndc = glm::vec2(clip) / clip.w;
            return glm::vec2((ndc.x + 1.0f) * 0.5f * viewport.x, (1.0f - ndc.y) * 0.5f * viewport.y);
        }

        float DistanceToSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b)
        {
            const glm::vec2 ab = b - a;
            const float len2 = glm::dot(ab, ab);
            const float t = len2 > 0.0f ? glm::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
            return glm::length(p - (a + ab * t));
        }
    }

    glm::vec3 TransformGizmo::AxisDirection(GizmoHandle handle) const
    {
        glm::vec3 unit(0.0f);
        switch (handle)
        {
        case GizmoHandle::X: unit = glm::vec3(1, 0, 0); break;
        case GizmoHandle::Y: unit = glm::vec3(0, 1, 0); break;
        case GizmoHandle::Z: unit = glm::vec3(0, 0, 1); break;
        default: return unit;
        }
        return glm::normalize(orientation * unit);
    }

    std::vector<GizmoHandle> GizmoHandles(GizmoMode mode)
    {
        if (mode == GizmoMode::Scale)
        {
            return {GizmoHandle::X, GizmoHandle::Y, GizmoHandle::Z, GizmoHandle::Uniform};
        }
        return {GizmoHandle::X, GizmoHandle::Y, GizmoHandle::Z};
    }

    std::optional<TransformGizmo> MakeTransformGizmo(GizmoMode mode, const glm::vec3& origin, const glm::quat& orientation,
                                                     const glm::mat4& view, const glm::mat4& projection)
    {
        const float focal = projection[1][1];   // 1 / tan(세로 시야각 / 2), 직교 투영이면 2 / 높이
        if (!std::isfinite(focal) || std::abs(focal) < 1e-6f)
        {
            return std::nullopt;
        }
        const bool orthographic = std::abs(projection[2][3]) < 1e-6f;
        float visibleHeight;
        if (orthographic)
        {
            visibleHeight = 2.0f / std::abs(focal);
        }
        else
        {
            const float depth = -(view * glm::vec4(origin, 1.0f)).z;   // 카메라는 뷰 공간 -Z를 본다
            if (depth <= 1e-3f)
            {
                return std::nullopt;   // 카메라 뒤
            }
            visibleHeight = 2.0f * depth / std::abs(focal);
        }
        TransformGizmo gizmo;
        gizmo.mode = mode;
        gizmo.origin = origin;
        const float n = glm::length(orientation);
        gizmo.orientation = (mode == GizmoMode::Scale && std::isfinite(n) && n > 1e-6f)
                                ? orientation / n : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        gizmo.length = visibleHeight * TransformGizmo::kScreenFraction;
        return gizmo;
    }

    void AppendGizmoHandleLines(const TransformGizmo& gizmo, GizmoHandle handle,
                                std::vector<std::pair<glm::vec3, glm::vec3>>& out)
    {
        const float L = gizmo.length;
        if (handle == GizmoHandle::Uniform)
        {
            if (gizmo.mode == GizmoMode::Scale)
            {
                AppendBox(gizmo.origin, gizmo.AxisDirection(GizmoHandle::X), gizmo.AxisDirection(GizmoHandle::Y),
                          gizmo.AxisDirection(GizmoHandle::Z), L * 0.08f, out);
            }
            return;
        }
        const glm::vec3 dir = gizmo.AxisDirection(handle);
        if (dir == glm::vec3(0.0f))
        {
            return;
        }
        glm::vec3 u, v;
        Perpendiculars(dir, u, v);

        switch (gizmo.mode)
        {
        case GizmoMode::Move:
        {
            const glm::vec3 shaftEnd = gizmo.origin + dir * (L * 0.78f);
            const glm::vec3 tip = gizmo.origin + dir * L;
            AppendShaft(gizmo.origin, shaftEnd, u, v, L * 0.012f, out);
            constexpr int kCone = 12;
            const float R = L * 0.075f;
            for (int i = 0; i < kCone; ++i)   // 화살촉: 원뿔 옆선 + 밑면 원
            {
                const float a0 = kTwoPi * static_cast<float>(i) / kCone;
                const float a1 = kTwoPi * static_cast<float>(i + 1) / kCone;
                const glm::vec3 p0 = shaftEnd + (u * std::cos(a0) + v * std::sin(a0)) * R;
                const glm::vec3 p1 = shaftEnd + (u * std::cos(a1) + v * std::sin(a1)) * R;
                out.emplace_back(tip, p0);
                out.emplace_back(p0, p1);
            }
            break;
        }
        case GizmoMode::Rotate:
            for (float radius : {L * TransformGizmo::kRingRadius, L * TransformGizmo::kRingRadius * 1.012f})
            {
                for (int i = 0; i < kRingSegments; ++i)
                {
                    out.emplace_back(RingPoint(gizmo, u, v, radius, i), RingPoint(gizmo, u, v, radius, i + 1));
                }
            }
            break;
        case GizmoMode::Scale:
        {
            const float h = L * 0.055f;
            const glm::vec3 boxCenter = gizmo.origin + dir * (L - h);
            AppendShaft(gizmo.origin, boxCenter - dir * h, u, v, L * 0.012f, out);
            AppendBox(boxCenter, dir, u, v, h, out);
            break;
        }
        }
    }

    GizmoHandle PickGizmoHandle(const TransformGizmo& gizmo, const glm::mat4& view, const glm::mat4& projection,
                                float ndcX, float ndcY, glm::vec2 viewportPixels)
    {
        if (viewportPixels.x <= 0.0f || viewportPixels.y <= 0.0f)
        {
            return GizmoHandle::None;
        }
        const glm::mat4 viewProjection = projection * view;
        const auto origin = ToPixels(viewProjection, gizmo.origin, viewportPixels);
        if (!origin)
        {
            return GizmoHandle::None;
        }
        const glm::vec2 mouse((ndcX + 1.0f) * 0.5f * viewportPixels.x, (1.0f - ndcY) * 0.5f * viewportPixels.y);
        const float tolerance = TransformGizmo::kPickTolerancePx;

        // 크기 모드의 가운데 상자가 축 손잡이보다 먼저다(세 축이 모두 가운데에서 시작하므로).
        if (gizmo.mode == GizmoMode::Scale && glm::length(mouse - *origin) <= tolerance * 1.5f)
        {
            return GizmoHandle::Uniform;
        }

        GizmoHandle best = GizmoHandle::None;
        float bestDistance = tolerance;
        for (GizmoHandle handle : {GizmoHandle::X, GizmoHandle::Y, GizmoHandle::Z})
        {
            float distance = std::numeric_limits<float>::infinity();
            if (gizmo.mode == GizmoMode::Rotate)
            {
                // 고리를 화면에 투영한 꺾은선까지의 거리. 고리가 화면과 거의 수직이면(선처럼 보임) 끄는 각도를 정하기
                // 어렵지만 잡기는 허용한다 - 끌기 쪽(에디터)이 화면 이동량으로 대신한다.
                glm::vec3 u, v;
                Perpendiculars(gizmo.AxisDirection(handle), u, v);
                const float radius = gizmo.length * TransformGizmo::kRingRadius;
                std::optional<glm::vec2> previous = ToPixels(viewProjection, RingPoint(gizmo, u, v, radius, 0), viewportPixels);
                for (int i = 1; i <= kRingSegments; ++i)
                {
                    const auto current = ToPixels(viewProjection, RingPoint(gizmo, u, v, radius, i), viewportPixels);
                    if (previous && current)
                    {
                        distance = (std::min)(distance, DistanceToSegment(mouse, *previous, *current));
                    }
                    previous = current;
                }
            }
            else
            {
                const auto tip = ToPixels(viewProjection, gizmo.AxisTip(handle), viewportPixels);
                if (!tip || glm::length(*tip - *origin) < 2.0f * tolerance)
                {
                    continue;   // 카메라를 향해 점처럼 보이는 축
                }
                distance = DistanceToSegment(mouse, *origin, *tip);
            }
            if (distance <= bestDistance)
            {
                best = handle;
                bestDistance = distance;
            }
        }
        return best;
    }
}
