#include "InputState.h"

namespace Engine
{
    void InputState::BeginFrame()
    {
        mouseDeltaX_ = 0.0f;
        mouseDeltaY_ = 0.0f;
        wheelDelta_ = 0.0f;
        dragDeltaX_.fill(0.0f);
        dragDeltaY_.fill(0.0f);
        keysPressed_.fill(false);
        keysReleased_.fill(false);
        mouseButtonsPressed_.fill(false);
        mouseButtonsReleased_.fill(false);
    }

    void InputState::Apply(const InputEvent& event)
    {
        switch (event.type)
        {
        case InputEventType::KeyDown:
        case InputEventType::KeyUp:
        {
            const size_t index = static_cast<size_t>(event.keyCode);
            if (event.keyCode != KeyCode::Unknown && index < kKeyCount)
            {
                const bool down = (event.type == InputEventType::KeyDown);
                if (down && !keys_[index]) keysPressed_[index] = true;
                if (!down && keys_[index]) keysReleased_[index] = true;
                keys_[index] = down;
            }
            break;
        }
        case InputEventType::MouseButtonDown:
        case InputEventType::MouseButtonUp:
        {
            const size_t index = static_cast<size_t>(event.mouseButton);
            if (index < mouseButtons_.size())
            {
                const bool down = (event.type == InputEventType::MouseButtonDown);
                if (down && !mouseButtons_[index]) mouseButtonsPressed_[index] = true;
                if (!down && mouseButtons_[index]) mouseButtonsReleased_[index] = true;
                mouseButtons_[index] = down;
            }
            // 누른/뗀 위치를 이동량의 기준점으로 삼는다. 증상: 에디터에서 수평으로만 우클릭 드래그했는데
            // pitch까지 크게 바뀌었다. 원인: 기준점이 "마지막 MouseMove 위치"(마우스 트래킹으로 들어온
            // 실제 커서 위치일 수 있음)라서, 첫 드래그 이동량에 그 위치에서 누른 위치까지의 점프가 섞였다.
            lastMouseX_ = event.mouseX;
            lastMouseY_ = event.mouseY;
            hasLastMouse_ = true;
            break;
        }
        case InputEventType::MouseMove:
            // Qt 뷰포트는 절대 좌표만 보낸다. 이동량은 직전 좌표와의 차이로 누적한다(한 프레임에 여러 번 올 수 있음).
            if (hasLastMouse_)
            {
                const float dx = static_cast<float>(event.mouseX - lastMouseX_);
                const float dy = static_cast<float>(event.mouseY - lastMouseY_);
                mouseDeltaX_ += dx;
                mouseDeltaY_ += dy;
                for (size_t b = 0; b < mouseButtons_.size(); ++b)
                {
                    if (mouseButtons_[b])
                    {
                        dragDeltaX_[b] += dx;
                        dragDeltaY_[b] += dy;
                    }
                }
            }
            lastMouseX_ = event.mouseX;
            lastMouseY_ = event.mouseY;
            hasLastMouse_ = true;
            break;
        case InputEventType::MouseWheel:
            wheelDelta_ += event.mouseWheelDelta;
            break;
        default:
            break;
        }
    }

    bool InputState::IsKeyDown(KeyCode key) const
    {
        const size_t index = static_cast<size_t>(key);
        return key != KeyCode::Unknown && index < kKeyCount && keys_[index];
    }

    bool InputState::IsMouseButtonDown(MouseButton button) const
    {
        const size_t index = static_cast<size_t>(button);
        return index < mouseButtons_.size() && mouseButtons_[index];
    }

    bool InputState::WasKeyPressed(KeyCode key) const
    {
        const size_t index = static_cast<size_t>(key);
        return key != KeyCode::Unknown && index < kKeyCount && keysPressed_[index];
    }

    bool InputState::WasKeyReleased(KeyCode key) const
    {
        const size_t index = static_cast<size_t>(key);
        return key != KeyCode::Unknown && index < kKeyCount && keysReleased_[index];
    }

    bool InputState::WasMouseButtonPressed(MouseButton button) const
    {
        const size_t index = static_cast<size_t>(button);
        return index < mouseButtonsPressed_.size() && mouseButtonsPressed_[index];
    }

    bool InputState::WasMouseButtonReleased(MouseButton button) const
    {
        const size_t index = static_cast<size_t>(button);
        return index < mouseButtonsReleased_.size() && mouseButtonsReleased_[index];
    }

    float InputState::GetDragDeltaX(MouseButton button) const
    {
        const size_t index = static_cast<size_t>(button);
        return index < dragDeltaX_.size() ? dragDeltaX_[index] : 0.0f;
    }

    float InputState::GetDragDeltaY(MouseButton button) const
    {
        const size_t index = static_cast<size_t>(button);
        return index < dragDeltaY_.size() ? dragDeltaY_[index] : 0.0f;
    }

    void InputState::ReleaseAll()
    {
        keys_.fill(false);
        mouseButtons_.fill(false);
        hasLastMouse_ = false;
        // 이번 프레임에 쌓인 엣지도 지운다. 지우지 않으면, 같은 프레임 안에 키를 누르고(KeyDown) 바로 포커스를
        // 잃었을 때 "누른 엣지는 있는데 지금은 안 눌림"이 되어 InputActions가 이를 "한 프레임 안에 눌렀다 뗌"으로
        // 읽고 WasReleased를 만든다 - 포커스 상실이 "뗀 순간" 동작을 발동시키는 것이다
        // (InputActionsTest.EngineFocusLost_ResetsActionsToo가 잡았다). 포커스를 잃은 프레임의 입력은 버린다.
        keysPressed_.fill(false);
        keysReleased_.fill(false);
        mouseButtonsPressed_.fill(false);
        mouseButtonsReleased_.fill(false);
    }
}
