#pragma once

#include "System.h"
#include "Components.h"

#include "Entity.h"
#include <cstddef>
#include <glm/vec3.hpp>

namespace Engine
{
    class Camera;
    class ECSRegistry;

    struct ActiveCameraSelection
    {
        Entity entity;              // 활성 카메라. 후보가 없으면 IsValid() == false
        size_t candidateCount = 0;  // isMainCamera == true이고 Transform이 있는 엔티티 수
        bool tie = false;           // 최고 priority가 여럿이었다(가장 작은 엔티티 id를 골랐음)
    };

    // 활성 카메라 선택 규칙의 유일한 구현. CameraSystem과 에디터(파이썬 바인딩 SelectActiveCamera)가
    // 같은 함수를 쓴다. 규칙이 두 곳에 따로 있으면 한쪽만 고쳐지는 버그가 생긴다.
    //  1. 후보: isMainCamera == true이고 TransformComponent가 있는 엔티티
    //  2. 후보 중 priority가 가장 높은 것
    //  3. 동점이면 엔티티 id가 가장 작은 것. dense 배열 순서는 삭제 때마다 바뀌어서 결과가 흔들린다.
    // 예전에는 "dense 배열에서 처음 나온 isMainCamera"를 경고 없이 골랐다(docs/INGAME_CAMERA_PLAN.md C3).
    ActiveCameraSelection SelectActiveCamera(ECSRegistry& registry);

    /// <summary>
    /// ECS의 활성 카메라(CameraComponent.isMainCamera == true) -> 게임 카메라(Engine::defaultCamera).
    ///
    /// 예전에는 이 로직이 RenderSystem(렌더 배치 수집) 안에 섞여 있었다. 카메라 문제와 렌더링 문제를
    /// 로그와 테스트에서 구분하기 어려워서 분리했다(docs/INGAME_CAMERA_PLAN.md C1).
    ///
    /// 매 프레임 하는 일:
    ///  - 위치/시선: TransformComponent의 position과 rotation(-Z가 전방)으로 setPosition/lookAt.
    ///  - 렌즈: CameraComponent의 fov(도)/nearPlane/farPlane으로 setLens (C2). 종횡비는 Engine 소유라
    ///    건드리지 않는다(Camera.h 주석 참고).
    ///  - 블렌드(C4): 활성 카메라가 바뀌면, 새 카메라의 blendInSeconds 동안 이전 시점(위치, 시선 방향, fov)에서
    ///    새 시점으로 smoothstep 보간한다. 0이면 즉시 전환한다. 처음 활성 카메라가 생길 때는 보간하지 않는다
    ///    (넘어올 "이전 카메라"가 없음). 전환 방법(priority, isMainCamera, Inspector, 액션)과 무관하다.
    /// 활성 카메라가 없으면 Camera를 건드리지 않는다(마지막 값 유지).
    /// </summary>
    class CameraSystem : public System
    {
    public:
        // camera가 nullptr이면 Update는 아무 것도 하지 않는다(유닛테스트 등).
        explicit CameraSystem(Camera* camera);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "CameraSystem"; }

        // 공유 Camera 객체를 쓰므로 병렬 스케줄러에 맡기지 않는다.
        bool CanRunInParallel() const override { return false; }

        bool IsBlending() const { return blending_; }

    private:
        Camera* camera_;
        bool tieWarned_ = false;

        // 블렌드 상태
        Entity lastActive_;
        bool blending_ = false;
        float blendElapsed_ = 0.0f;
        float blendDuration_ = 0.0f;
        glm::vec3 blendFromPosition_{0.0f};
        glm::vec3 blendFromForward_{0.0f, 0.0f, -1.0f};
        float blendFromFovRadians_ = 0.0f;
        bool invalidLensWarned_ = false;
    };
}
