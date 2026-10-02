#include "CameraRigSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "Hierarchy.h"
#include "../input/InputState.h"
#include "../core/logging/Logger.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace Engine
{
    namespace
    {
        glm::vec3 ToGlm(const Vec3& v) { return glm::vec3(v.x, v.y, v.z); }
        Vec3 FromGlm(const glm::vec3& v) { return Vec3(v.x, v.y, v.z); }

        // eye에서 direction을 바라보는 회전(카메라 전방 = rotation * (0,0,-1), CameraSystem과 같은 규약).
        Quaternion LookRotation(const glm::vec3& direction)
        {
            const glm::vec3 dir = glm::normalize(direction);
            // 시선이 거의 수직이면 up=(0,1,0)과 평행해서 회전이 정의되지 않는다. 그때는 다른 up을 쓴다.
            const glm::vec3 up = std::abs(glm::dot(dir, glm::vec3(0, 1, 0))) > 0.999f ? glm::vec3(0, 0, -1) : glm::vec3(0, 1, 0);
            const glm::quat q = glm::quatLookAtRH(dir, up);
            return Quaternion(q.x, q.y, q.z, q.w);
        }
    }

    CameraRigSystem::CameraRigSystem(const InputState* input)
        : input_(input)
    {
    }

    bool CameraRigSystem::ResolveTarget(ECSRegistry& registry, EntityID cameraId, const char* componentName,
                                        Entity target, Vec3& outPosition)
    {
        const TransformComponent* tc = target.IsValid() ? registry.GetComponent<TransformComponent>(target) : nullptr;
        if (!tc)
        {
            if (missingTargetWarned_.insert(cameraId).second)
            {
                Logger::Log(LogLevel::Warning,
                    "CameraRigSystem - {} on entity {} has no valid target (target id {} missing or without Transform); skipping",
                    componentName, cameraId, target.id);
            }
            return false;
        }
        missingTargetWarned_.erase(cameraId);
        // 대상이 다른 엔티티의 자식(예: 캐릭터의 머리)이면 로컬 위치가 아니라 월드 위치를 따라가야 한다.
        outPosition = ComputeWorldTransform(registry, target).position;
        return true;
    }

    bool CameraRigSystem::IsRootCamera(ECSRegistry& registry, EntityID cameraId, const char* componentName)
    {
        const Entity parent = GetParent(registry, Entity(cameraId));
        if (!parent.IsValid())
        {
            parentedCameraWarned_.erase(cameraId);
            return true;
        }
        if (parentedCameraWarned_.insert(cameraId).second)
        {
            Logger::Log(LogLevel::Warning,
                "CameraRigSystem - {} on entity {} is skipped: the camera has a parent (entity {}). "
                "Rigs write world positions, so a rig camera must be a root entity",
                componentName, cameraId, parent.id);
        }
        return false;
    }

    void CameraRigSystem::Update(ECSRegistry& registry, float deltaTime)
    {
        const float dt = deltaTime > 0.0f ? deltaTime : 0.0f;

        // ── C6: 플레이어 조작 궤도 카메라 ──
        if (auto* orbits = registry.GetComponentArray<CameraOrbitControlComponent>())
        {
            for (size_t i = 0; i < orbits->Size(); ++i)
            {
                const EntityID id = orbits->GetEntityIDs()[i];
                CameraOrbitControlComponent& oc = orbits->GetDenseArray()[i];
                TransformComponent* self = registry.GetComponent<TransformComponent>(Entity(id));
                Vec3 targetPos;
                if (!self || !IsRootCamera(registry, id, "CameraOrbitControlComponent") ||
                    !ResolveTarget(registry, id, "CameraOrbitControlComponent", oc.target, targetPos))
                {
                    continue;
                }

                if (input_)
                {
                    // 우클릭 필수면 "우클릭을 누른 채 움직인 양"만 쓴다(InputState::GetDragDeltaX 주석 참고).
                    const float dx = oc.requireRightMouse ? input_->GetDragDeltaX(MouseButton::Right) : input_->GetMouseDeltaX();
                    const float dy = oc.requireRightMouse ? input_->GetDragDeltaY(MouseButton::Right) : input_->GetMouseDeltaY();
                    oc.yaw -= dx * oc.sensitivity;
                    oc.pitch += dy * oc.sensitivity;
                    if (input_->GetWheelDelta() != 0.0f)
                    {
                        oc.distance *= std::pow(oc.zoomPerStep, input_->GetWheelDelta());
                    }
                }
                oc.pitch = glm::clamp(oc.pitch, -80.0f, 85.0f);
                const float minD = oc.minDistance > 0.01f ? oc.minDistance : 0.01f;
                const float maxD = oc.maxDistance > minD ? oc.maxDistance : minD;
                oc.distance = glm::clamp(oc.distance, minD, maxD);

                const glm::vec3 pivot = ToGlm(targetPos) + ToGlm(oc.targetOffset);
                const float yaw = glm::radians(oc.yaw), pitch = glm::radians(oc.pitch);
                const glm::vec3 offsetDir(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
                const glm::vec3 eye = pivot + offsetDir * oc.distance;

                self->position = FromGlm(eye);
                self->rotation = LookRotation(pivot - eye);
            }
        }

        auto* orbitsForSkip = registry.GetComponentArray<CameraOrbitControlComponent>();
        auto hasOrbit = [&](EntityID id) { return orbitsForSkip && orbitsForSkip->Get(id) != nullptr; };

        // ── C5: 추적 ──
        if (auto* follows = registry.GetComponentArray<CameraFollowComponent>())
        {
            for (size_t i = 0; i < follows->Size(); ++i)
            {
                const EntityID id = follows->GetEntityIDs()[i];
                if (hasOrbit(id))
                {
                    continue;
                }
                const CameraFollowComponent& fc = follows->GetDenseArray()[i];
                TransformComponent* self = registry.GetComponent<TransformComponent>(Entity(id));
                Vec3 targetPos;
                if (!self || !IsRootCamera(registry, id, "CameraFollowComponent") ||
                    !ResolveTarget(registry, id, "CameraFollowComponent", fc.target, targetPos))
                {
                    continue;
                }
                const glm::vec3 desired = ToGlm(targetPos) + ToGlm(fc.offset);
                glm::vec3 current = ToGlm(self->position);
                if (fc.damping <= 0.0f)
                {
                    current = desired;
                }
                else
                {
                    // 지수 감쇠: 프레임레이트와 무관하게 damping초마다 남은 거리가 1/e로 준다.
                    const float alpha = 1.0f - std::exp(-dt / fc.damping);
                    current += (desired - current) * alpha;
                }
                self->position = FromGlm(current);
            }
        }

        // ── C5: 주시 ──
        if (auto* looks = registry.GetComponentArray<CameraLookAtComponent>())
        {
            for (size_t i = 0; i < looks->Size(); ++i)
            {
                const EntityID id = looks->GetEntityIDs()[i];
                if (hasOrbit(id))
                {
                    continue;
                }
                const CameraLookAtComponent& lc = looks->GetDenseArray()[i];
                TransformComponent* self = registry.GetComponent<TransformComponent>(Entity(id));
                Vec3 targetPos;
                if (!self || !IsRootCamera(registry, id, "CameraLookAtComponent") ||
                    !ResolveTarget(registry, id, "CameraLookAtComponent", lc.target, targetPos))
                {
                    continue;
                }
                const glm::vec3 dir = ToGlm(targetPos) + ToGlm(lc.targetOffset) - ToGlm(self->position);
                if (glm::length(dir) > 1e-5f)
                {
                    self->rotation = LookRotation(dir);
                }
            }
        }
    }
}
