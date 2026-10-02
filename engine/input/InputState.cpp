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
                keys_[index] = (event.type == InputEventType::KeyDown);
            }
            break;
        }
        case InputEventType::MouseButtonDown:
        case InputEventType::MouseButtonUp:
        {
            const size_t index = static_cast<size_t>(event.mouseButton);
            if (index < mouseButtons_.size())
            {
                mouseButtons_[index] = (event.type == InputEventType::MouseButtonDown);
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
    }
}
