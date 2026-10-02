#include "CameraGizmoSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "Hierarchy.h"
#include "../renderer/Camera.h"
#include "../renderer/DebugLineBuffer.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace Engine
{
    void AppendCameraGizmoLines(DebugLineBuffer& buffer, const TransformComponent& transform,
                                const CameraComponent& camera, float aspect, float length)
    {
        const glm::vec3 p(transform.position.x, transform.position.y, transform.position.z);
        const glm::quat q(transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z);
        const glm::vec3 forward = q * glm::vec3(0.0f, 0.0f, -1.0f);   // CameraSystem과 같은 규약
        const glm::vec3 up = q * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = q * glm::vec3(1.0f, 0.0f, 0.0f);

        const float fov = (camera.fov > 1.0f && camera.fov < 179.0f) ? camera.fov : 60.0f;
        const float halfH = std::tan(glm::radians(fov) * 0.5f) * length;
        const float halfW = halfH * (aspect > 0.0f ? aspect : 16.0f / 9.0f);
        const glm::vec3 c = p + forward * length;

        const glm::vec3 tl = c + up * halfH - right * halfW;
        const glm::vec3 tr = c + up * halfH + right * halfW;
        const glm::vec3 br = c - up * halfH + right * halfW;
        const glm::vec3 bl = c - up * halfH - right * halfW;

        // 꼭짓점 -> 먼 평면 네 모서리
        buffer.Add(p, tl); buffer.Add(p, tr); buffer.Add(p, br); buffer.Add(p, bl);
        // 먼 평면 사각형
        buffer.Add(tl, tr); buffer.Add(tr, br); buffer.Add(br, bl); buffer.Add(bl, tl);
        // 위쪽 표시 삼각형(카메라가 뒤집혀 있는지 한눈에 보이게)
        const glm::vec3 apex = c + up * (halfH * 1.5f);
        const glm::vec3 baseL = c + up * halfH - right * (halfW * 0.3f);
        const glm::vec3 baseR = c + up * halfH + right * (halfW * 0.3f);
        buffer.Add(baseL, apex); buffer.Add(apex, baseR); buffer.Add(baseR, baseL);
    }

    CameraGizmoSystem::CameraGizmoSystem(DebugLineBuffer* buffer, const Camera* aspectSource)
        : buffer_(buffer)
        , aspectSource_(aspectSource)
    {
    }

    void CameraGizmoSystem::Update(ECSRegistry& registry, float /*deltaTime*/)
    {
        if (!buffer_)
        {
            return;
        }
        auto* cameras = registry.GetComponentArray<CameraComponent>();
        auto* transforms = registry.GetComponentArray<TransformComponent>();
        if (!cameras || !transforms)
        {
            return;
        }
        const float aspect = aspectSource_ ? aspectSource_->getAspect() : 16.0f / 9.0f;
        for (size_t i = 0; i < cameras->Size(); ++i)
        {
            const EntityID id = cameras->GetEntityIDs()[i];
            if (transforms->Get(id))
            {
                // CameraSystem과 같은 월드 기준(부모 아래 카메라도 실제 보는 위치에 그린다)
                const TransformComponent world = ComputeWorldTransform(registry, Entity(id));
                AppendCameraGizmoLines(*buffer_, world, cameras->GetDenseArray()[i], aspect, kGizmoLength);
            }
        }
    }
}
