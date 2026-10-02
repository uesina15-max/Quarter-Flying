#pragma once

#include "System.h"
#include "Components.h"

namespace Engine
{
    class Camera;
    class DebugLineBuffer;
    struct TransformComponent;

    // 카메라 하나의 기즈모 선분(시야 피라미드 8개 + 위쪽 표시 삼각형 3개)을 buffer에 추가한다.
    // length = 피라미드 깊이(월드 단위), aspect = 가로/세로. 순수 계산이라 GL 없이 테스트할 수 있다.
    void AppendCameraGizmoLines(DebugLineBuffer& buffer, const TransformComponent& transform,
                                const CameraComponent& camera, float aspect, float length);

    /// <summary>
    /// Scene 뷰에 카메라 엔티티마다 위치와 시야를 선으로 그린다(docs/INGAME_CAMERA_PLAN.md C7).
    /// 선은 DebugLineBuffer에 모으기만 하고, 그릴지는 Renderer가 정한다(에디터 카메라로 볼 때만).
    /// 종횡비는 게임 카메라(aspectSource)에서 읽는다. 게임 화면과 같은 비율의 피라미드가 된다.
    /// </summary>
    class CameraGizmoSystem : public System
    {
    public:
        CameraGizmoSystem(DebugLineBuffer* buffer, const Camera* aspectSource);

        void Update(ECSRegistry& registry, float deltaTime) override;

        const char* GetName() const override { return "CameraGizmoSystem"; }
        int GetPriority() const override { return 50; }      // 리그/카메라 계산 뒤(최종 위치로 그림)
        bool CanRunInParallel() const override { return false; }

        static constexpr float kGizmoLength = 1.5f;

    private:
        DebugLineBuffer* buffer_;
        const Camera* aspectSource_;
    };
}
