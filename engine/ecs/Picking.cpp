#include "Picking.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "Components.h"
#include "Hierarchy.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <limits>

namespace Engine
{
    std::optional<PickRay> MakePickRay(const glm::mat4& view, const glm::mat4& projection, float ndcX, float ndcY)
    {
        const glm::mat4 viewProjection = projection * view;
        if (std::abs(glm::determinant(viewProjection)) < 1e-12f)
        {
            return std::nullopt;
        }
        const glm::mat4 inverse = glm::inverse(viewProjection);
        // GL 깊이 규약(-1 = near, 1 = far). GLM_FORCE_DEPTH_ZERO_TO_ONE은 쓰지 않는다.
        glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < 1e-12f || std::abs(farPoint.w) < 1e-12f)
        {
            return std::nullopt;
        }
        const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
        const glm::vec3 delta = glm::vec3(farPoint) / farPoint.w - origin;
        const float length = glm::length(delta);
        if (!std::isfinite(length) || length < 1e-12f)
        {
            return std::nullopt;
        }
        return PickRay{origin, delta / length};
    }

    std::optional<float> IntersectRayBounds(const PickRay& ray, const glm::mat4& model, const LocalBounds& bounds)
    {
        // 상자를 월드로 옮기는 대신 광선을 로컬로 옮긴다. 선형 변환이라 로컬에서 구한 t가 월드 t와 같다
        // (방향을 다시 정규화하지 않는 한).
        if (std::abs(glm::determinant(model)) < 1e-12f)
        {
            return std::nullopt;   // 스케일 0 - 화면에도 보이지 않는다
        }
        const glm::mat4 toLocal = glm::inverse(model);
        const glm::vec3 origin = glm::vec3(toLocal * glm::vec4(ray.origin, 1.0f));
        const glm::vec3 direction = glm::vec3(toLocal * glm::vec4(ray.direction, 0.0f));

        float tNear = -std::numeric_limits<float>::infinity();
        float tFar = std::numeric_limits<float>::infinity();
        for (int axis = 0; axis < 3; ++axis)
        {
            // 두께 0인 메시(평면)도 맞을 수 있게 아주 조금 두껍게 본다.
            float lo = bounds.min[axis];
            float hi = bounds.max[axis];
            if (hi - lo < 1e-4f)
            {
                lo -= 5e-5f;
                hi += 5e-5f;
            }
            if (std::abs(direction[axis]) < 1e-12f)
            {
                if (origin[axis] < lo || origin[axis] > hi)
                {
                    return std::nullopt;   // 이 축과 평행하고 상자 밖
                }
                continue;
            }
            float t1 = (lo - origin[axis]) / direction[axis];
            float t2 = (hi - origin[axis]) / direction[axis];
            if (t1 > t2) std::swap(t1, t2);
            tNear = (std::max)(tNear, t1);
            tFar = (std::min)(tFar, t2);
            if (tNear > tFar)
            {
                return std::nullopt;
            }
        }
        if (tFar < 0.0f)
        {
            return std::nullopt;   // 상자가 광선 뒤에 있다
        }
        return tNear >= 0.0f ? tNear : tFar;   // 시작점이 상자 안이면 빠져나가는 곳
    }

    std::optional<PickHit> PickEntity(ECSRegistry& registry, const PickRay& ray, const MeshBoundsLookup& meshBounds)
    {
        std::optional<PickHit> best;
        auto consider = [&](Entity entity, const glm::mat4& model, const LocalBounds& bounds) {
            if (auto t = IntersectRayBounds(ray, model, bounds); t && (!best || *t < best->distance))
            {
                best = PickHit{entity, *t, ray.origin + ray.direction * *t};
            }
        };

        auto* transforms = registry.GetComponentArray<TransformComponent>();
        if (!transforms)
        {
            return std::nullopt;
        }
        for (size_t i = 0; i < transforms->Size(); ++i)
        {
            const Entity entity(transforms->GetEntityIDs()[i]);
            if (const RenderableComponent* renderable = registry.GetComponent<RenderableComponent>(entity))
            {
                consider(entity, ComputeWorldMatrix(registry, entity),
                         meshBounds ? meshBounds(*renderable) : kUnitCubeBounds);
            }
            else if (registry.HasComponent<CameraComponent>(entity) || registry.HasComponent<ParticleEffectComponent>(entity))
            {
                const glm::vec3 position = glm::vec3(ComputeWorldMatrix(registry, entity)[3]);
                consider(entity, glm::translate(glm::mat4(1.0f), position),
                         LocalBounds{glm::vec3(-kPickProxyHalfExtent), glm::vec3(kPickProxyHalfExtent)});
            }
        }
        return best;
    }
}
