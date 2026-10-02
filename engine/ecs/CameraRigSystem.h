#pragma once

#include "System.h"
#include "Components.h"
#include <unordered_set>

namespace Engine
{
    class InputState;

    /// <summary>
    /// 카메라 리그: 카메라 엔티티의 TransformComponent를 계산한다(docs/INGAME_CAMERA_PLAN.md C5, C6).
    ///  - CameraOrbitControlComponent: 플레이어 마우스 입력(InputState)으로 대상 주위를 도는 카메라. 위치와 회전 모두.
    ///  - CameraFollowComponent: 대상 위치 + offset으로 따라간다(damping = 시간 상수, 초).
    ///  - CameraLookAtComponent: 대상을 바라보도록 회전만 정한다.
    /// OrbitControl이 있는 엔티티는 Follow/LookAt을 무시한다(두 규칙이 같은 Transform을 덮어쓰지 않게).
    ///
    /// 역할 분리: 이 System은 ECS Transform만 바꾸고, 그 Transform을 렌더 카메라로 옮기는 일은 CameraSystem이
    /// 한다. 그래서 GetPriority()가 CameraSystem(0)보다 작아야 같은 프레임에 반영된다.
    /// Edit 모드에서는 돌지 않는다. 에디터에서 편집 중인 카메라 위치를 덮어쓰면 안 되기 때문이다.
    /// </summary>
    class CameraRigSystem : public System
    {
    public:
        // input이 nullptr이면 OrbitControl은 입력 없이 현재 yaw/pitch/distance로 자리만 잡는다.
        explicit CameraRigSystem(const InputState* input);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "CameraRigSystem"; }
        int GetPriority() const override { return -10; }     // CameraSystem(0)보다 먼저
        bool CanRunInParallel() const override { return false; }
        bool RunsInEditMode() const override { return false; }

    private:
        // 대상 위치를 구한다. 대상이 없거나 Transform이 없으면 false와 함께 카메라당 한 번 경고한다.
        bool ResolveTarget(ECSRegistry& registry, EntityID cameraId, const char* componentName,
                           Entity target, Vec3& outPosition);

        const InputState* input_;
        std::unordered_set<EntityID> missingTargetWarned_;

        // 리그는 계산한 월드 위치/회전을 카메라의 Transform에 그대로 쓴다. 카메라에 부모가 있으면 그 값이
        // 부모 기준 로컬로 해석돼 엉뚱한 곳으로 간다(에러 없음). 그래서 부모가 있는 리그 카메라는 건너뛰고
        // 한 번 경고한다(CLAUDE.md 관례 3번).
        bool IsRootCamera(ECSRegistry& registry, EntityID cameraId, const char* componentName);
        std::unordered_set<EntityID> parentedCameraWarned_;
    };
}
