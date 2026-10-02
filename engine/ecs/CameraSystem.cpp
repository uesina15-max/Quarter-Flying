#include "CameraSystem.h"
#include "ECSRegistry.h"
#include "ComponentArray.h"
#include "Hierarchy.h"
#include "../renderer/Camera.h"
#include "../core/logging/Logger.h"
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    ActiveCameraSelection SelectActiveCamera(ECSRegistry& registry)
    {
        ActiveCameraSelection result;
        auto* cameras = registry.GetComponentArray<CameraComponent>();
        auto* transforms = registry.GetComponentArray<TransformComponent>();
        if (!cameras || !transforms)
        {
            return result;
        }

        bool haveBest = false;
        int bestPriority = 0;
        EntityID bestId = 0;
        for (size_t i = 0; i < cameras->Size(); ++i)
        {
            const CameraComponent& cc = cameras->GetDenseArray()[i];
            const EntityID id = cameras->GetEntityIDs()[i];
            if (!cc.isMainCamera || !transforms->Get(id))
            {
                continue;  // 후보 아님(위치를 알 수 없는 카메라 포함)
            }
            ++result.candidateCount;
            if (!haveBest || cc.priority > bestPriority)
            {
                haveBest = true;
                bestPriority = cc.priority;
                bestId = id;
                result.tie = false;
            }
            else if (cc.priority == bestPriority)
            {
                result.tie = true;
                if (id < bestId)
                {
                    bestId = id;
                }
            }
        }
        if (haveBest)
        {
            result.entity = Entity(bestId);
        }
        return result;
    }

    CameraSystem::CameraSystem(Camera* camera)
        : camera_(camera)
    {
    }

    void CameraSystem::Update(ECSRegistry& registry, float deltaTime)
    {
        if (!camera_)
        {
            return;
        }

        const ActiveCameraSelection selection = SelectActiveCamera(registry);
        if (selection.tie && !tieWarned_)
        {
            Logger::Log(LogLevel::Warning,
                "CameraSystem - several main cameras share the highest priority; using entity {} (lowest id). "
                "Set CameraComponent.priority to choose explicitly", selection.entity.id);
        }
        tieWarned_ = selection.tie;

        if (!selection.entity.IsValid())
        {
            return;
        }
        const CameraComponent* active = registry.GetComponent<CameraComponent>(selection.entity);
        if (!active || !registry.GetComponent<TransformComponent>(selection.entity))
        {
            return;
        }
        // 부모 아래에 있는 카메라(예: 차량에 붙은 카메라)는 월드 기준 위치/방향으로 본다(Hierarchy.h).
        const TransformComponent world = ComputeWorldTransform(registry, selection.entity);
        const TransformComponent* activeTransform = &world;

        glm::vec3 targetPosition(activeTransform->position.x, activeTransform->position.y, activeTransform->position.z);
        glm::quat rotation(activeTransform->rotation.w, activeTransform->rotation.x,
                           activeTransform->rotation.y, activeTransform->rotation.z);
        glm::vec3 targetForward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));

        // 렌즈. 잘못된 값으로 투영 행렬을 만들면 화면이 통째로 사라지거나 뒤집히는데 GL 에러는 없다.
        // 그래서 적용하지 않고 한 번 경고한다(CLAUDE.md 관례 3번).
        const bool lensValid = active->fov > 1.0f && active->fov < 179.0f &&
                               active->nearPlane > 0.0f && active->farPlane > active->nearPlane;
        if (!lensValid && !invalidLensWarned_)
        {
            Logger::Log(LogLevel::Warning,
                "CameraSystem - invalid lens on main camera (fov={}, near={}, far={}); keeping previous projection",
                active->fov, active->nearPlane, active->farPlane);
        }
        invalidLensWarned_ = !lensValid;
        const float targetFov = lensValid ? glm::radians(active->fov) : camera_->getFovRadians();

        // 활성 카메라 전환 감지 -> 블렌드 시작. 출발점은 "지금 화면에 보이는 시점"이다(블렌드 도중에
        // 또 바뀌어도 튀지 않는다).
        if (selection.entity != lastActive_)
        {
            const bool hadPrevious = lastActive_.IsValid();
            lastActive_ = selection.entity;
            if (hadPrevious && active->blendInSeconds > 0.0f)
            {
                blending_ = true;
                blendElapsed_ = 0.0f;
                blendDuration_ = active->blendInSeconds;
                blendFromPosition_ = camera_->getPosition();
                const glm::vec3 look = camera_->getTarget() - camera_->getPosition();
                blendFromForward_ = glm::length(look) > 1e-6f ? glm::normalize(look) : targetForward;
                blendFromFovRadians_ = camera_->getFovRadians();
            }
            else
            {
                blending_ = false;
            }
        }

        glm::vec3 position = targetPosition;
        glm::vec3 forward = targetForward;
        float fov = targetFov;
        if (blending_)
        {
            blendElapsed_ += (deltaTime > 0.0f ? deltaTime : 0.0f);
            float t = blendDuration_ > 0.0f ? glm::clamp(blendElapsed_ / blendDuration_, 0.0f, 1.0f) : 1.0f;
            if (t >= 1.0f)
            {
                blending_ = false;
            }
            else
            {
                const float s = t * t * (3.0f - 2.0f * t);   // smoothstep: 시작과 끝에서 부드럽게
                position = glm::mix(blendFromPosition_, targetPosition, s);
                const glm::vec3 mixed = glm::mix(blendFromForward_, targetForward, s);
                // 정반대 방향 사이의 보간이면 중간에 길이가 0이 될 수 있다 - 그때는 목표 방향을 쓴다.
                forward = glm::length(mixed) > 1e-4f ? glm::normalize(mixed) : targetForward;
                fov = glm::mix(blendFromFovRadians_, targetFov, s);
            }
        }

        camera_->setPosition(position);
        camera_->lookAt(position + forward);
        if (lensValid)
        {
            camera_->setLens(fov, active->nearPlane, active->farPlane);
        }
    }
}
