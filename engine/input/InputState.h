#pragma once

#include "../platform/IPlatform.h"
#include <array>
#include <cstdint>

namespace Engine
{
    /// <summary>
    /// 한 프레임 동안의 입력 상태(키/마우스 버튼 눌림, 마우스 이동량, 휠). 게임 쪽 System이 읽는다.
    ///
    /// 이 클래스가 생기기 전: 뷰포트(Qt)가 입력 이벤트를 Engine::PushInputEvent로 보내고 있었지만,
    /// TickFrame은 WindowClose/WindowResize만 처리하고 나머지는 버렸다. 즉 게임 코드가 입력을 읽을
    /// 방법이 없었다(docs/INGAME_CAMERA_PLAN.md C6).
    ///
    /// 사용 순서(Engine::TickFrame): BeginFrame() -> 이번 프레임 이벤트마다 Apply() -> System들이 읽기.
    /// "이번 프레임에 얼마나 움직였나"(마우스 이동량, 휠)는 BeginFrame에서 0으로 돌아가고,
    /// "지금 눌려 있나"(키, 버튼)는 Up 이벤트가 올 때까지 유지된다.
    /// </summary>
    class InputState
    {
    public:
        void BeginFrame();
        void Apply(const InputEvent& event);

        bool IsKeyDown(KeyCode key) const;
        bool IsMouseButtonDown(MouseButton button) const;
        float GetMouseDeltaX() const { return mouseDeltaX_; }
        float GetMouseDeltaY() const { return mouseDeltaY_; }
        float GetWheelDelta() const { return wheelDelta_; }   // 휠 한 칸 = 1.0 (앞으로 굴리면 +)

        // 이번 프레임에 button을 누른 채로 움직인 양. 누름 -> 이동 -> 뗌이 한 프레임 안에 모두 들어와도
        // 그 사이의 이동을 드래그로 센다. "프레임 끝에 눌려 있나 + 전체 이동량"으로 판단하면, 빠르고 짧은
        // 드래그(프레임 끝에는 이미 뗀 상태)가 통째로 무시된다. 실제 에디터 검증에서 그렇게 무시됐다.
        float GetDragDeltaX(MouseButton button) const;
        float GetDragDeltaY(MouseButton button) const;

        // 포커스를 잃는 등 Up 이벤트를 못 받는 경우를 위해 눌림 상태를 전부 해제한다.
        void ReleaseAll();

    private:
        static constexpr size_t kKeyCount = 256;
        std::array<bool, kKeyCount> keys_{};
        std::array<bool, 3> mouseButtons_{};
        std::array<float, 3> dragDeltaX_{};
        std::array<float, 3> dragDeltaY_{};
        float mouseDeltaX_ = 0.0f;
        float mouseDeltaY_ = 0.0f;
        float wheelDelta_ = 0.0f;
        int32_t lastMouseX_ = 0;
        int32_t lastMouseY_ = 0;
        bool hasLastMouse_ = false;
    };
}
