// 입력 이벤트 라우팅(Engine::DispatchInputEvent) + 포커스 상실 처리(Engine::HandleFocusLost) 유닛 테스트.
// docs/USABILITY_IMPROVEMENT_PLAN.md E-2.
//
// 플랫폼 큐(Player 창)와 외부 큐(에디터 Qt)가 같은 DispatchInputEvent를 거치므로, 여기서 검사하는
// 동작은 두 경로에 똑같이 적용된다. Engine은 Initialize 없이 만들어 쓴다(GL·창 없음).

#include <gtest/gtest.h>
#include "../core/Engine.h"

using namespace Engine;

namespace
{
    InputEvent Ev(InputEventType type)
    {
        InputEvent e;
        e.type = type;
        return e;
    }

    InputEvent Key(InputEventType type, KeyCode key)
    {
        InputEvent e = Ev(type);
        e.keyCode = key;
        return e;
    }

    InputEvent Button(InputEventType type, MouseButton button)
    {
        InputEvent e = Ev(type);
        e.mouseButton = button;
        return e;
    }
}

TEST(InputRoutingTest, KeyAndButtonEvents_ReachInputState)
{
    class Engine engine;
    EXPECT_TRUE(engine.DispatchInputEventForTesting(Key(InputEventType::KeyDown, KeyCode::W)));
    EXPECT_TRUE(engine.DispatchInputEventForTesting(Button(InputEventType::MouseButtonDown, MouseButton::Right)));

    EXPECT_TRUE(engine.GetInputStateForTesting().IsKeyDown(KeyCode::W));
    EXPECT_TRUE(engine.GetInputStateForTesting().IsMouseButtonDown(MouseButton::Right));
}

TEST(InputRoutingTest, FocusLost_ReleasesHeldKeysAndMouseButtons)
{
    class Engine engine;
    engine.DispatchInputEventForTesting(Key(InputEventType::KeyDown, KeyCode::W));
    engine.DispatchInputEventForTesting(Key(InputEventType::KeyDown, KeyCode::Space));
    engine.DispatchInputEventForTesting(Button(InputEventType::MouseButtonDown, MouseButton::Left));
    engine.DispatchInputEventForTesting(Button(InputEventType::MouseButtonDown, MouseButton::Middle));

    // 포커스 상실은 프레임을 끝내지 않는다(WindowClose와 다르다).
    EXPECT_TRUE(engine.DispatchInputEventForTesting(Ev(InputEventType::FocusLost)));

    const InputState& input = engine.GetInputStateForTesting();
    EXPECT_FALSE(input.IsKeyDown(KeyCode::W));
    EXPECT_FALSE(input.IsKeyDown(KeyCode::Space));
    EXPECT_FALSE(input.IsMouseButtonDown(MouseButton::Left));
    EXPECT_FALSE(input.IsMouseButtonDown(MouseButton::Middle));
}

TEST(InputRoutingTest, KeysPressedAfterFocusLost_AreTrackedAgain)
{
    class Engine engine;
    engine.DispatchInputEventForTesting(Key(InputEventType::KeyDown, KeyCode::W));
    engine.DispatchInputEventForTesting(Ev(InputEventType::FocusLost));
    engine.DispatchInputEventForTesting(Key(InputEventType::KeyDown, KeyCode::A));

    const InputState& input = engine.GetInputStateForTesting();
    EXPECT_FALSE(input.IsKeyDown(KeyCode::W));
    EXPECT_TRUE(input.IsKeyDown(KeyCode::A));
}

TEST(InputRoutingTest, WindowClose_EndsFrameAndStopsEngine)
{
    class Engine engine;
    EXPECT_FALSE(engine.DispatchInputEventForTesting(Ev(InputEventType::WindowClose)));
    EXPECT_FALSE(engine.IsRunning());
}
