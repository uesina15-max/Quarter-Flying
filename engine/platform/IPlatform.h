#pragma once

#include "../core/Types.h"
#include "../core/EngineConfig.h"
#include <cstdint>
#include <string>

namespace Engine
{
    // ========================================
    // Window Types
    // ========================================

    struct WindowDesc
    {
        std::string title;
        uint32_t width;
        uint32_t height;
        bool fullscreen;

        WindowDesc()
            : title("Engine Window")
            , width(kDefaultWindowWidth)
            , height(kDefaultWindowHeight)
            , fullscreen(false)
        {}

        WindowDesc(const std::string& title, uint32_t width, uint32_t height, bool fullscreen = false)
            : title(title)
            , width(width)
            , height(height)
            , fullscreen(fullscreen)
        {}
    };

    // ========================================
    // Input Types
    // ========================================

    enum class InputEventType : uint8_t
    {
        None,
        KeyDown,
        KeyUp,
        MouseMove,
        MouseButtonDown,
        MouseButtonUp,
        MouseWheel,
        WindowClose,
        WindowResize,
        // 창(Player) 또는 뷰포트(에디터 Qt)가 키보드 포커스를 잃었다. Up 이벤트가 오지 않으므로
        // 눌림 상태를 엔진이 직접 풀어야 한다(Engine::HandleFocusLost). 새 값은 항상 끝에 추가한다 -
        // 바인딩(EngineBindings.cpp)과 저장된 값의 번호가 바뀌지 않게.
        FocusLost
    };

    enum class KeyCode : uint16_t
    {
        Unknown = 0,
        
        // Alphabet keys
        A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G',
        H = 'H', I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N',
        O = 'O', P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U',
        V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',
        
        // Number keys
        Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
        Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',
        
        // Function keys
        F1 = 0x70, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        
        // Special keys
        Escape = 0x1B,
        Space = 0x20,
        Enter = 0x0D,
        Tab = 0x09,
        Backspace = 0x08,
        Shift = 0x10,
        Control = 0x11,
        Alt = 0x12,
        
        // Arrow keys
        Left = 0x25,
        Up = 0x26,
        Right = 0x27,
        Down = 0x28
    };

    enum class MouseButton : uint8_t
    {
        Left = 0,
        Right = 1,
        Middle = 2
    };

    struct InputEvent
    {
        InputEventType type;
        
        // Keyboard data
        KeyCode keyCode;
        
        // Mouse data
        MouseButton mouseButton;
        int32_t mouseX;
        int32_t mouseY;
        int32_t mouseDeltaX;
        int32_t mouseDeltaY;
        float mouseWheelDelta;
        
        // Window data
        uint32_t windowWidth;
        uint32_t windowHeight;

        InputEvent()
            : type(InputEventType::None)
            , keyCode(KeyCode::Unknown)
            , mouseButton(MouseButton::Left)
            , mouseX(0)
            , mouseY(0)
            , mouseDeltaX(0)
            , mouseDeltaY(0)
            , mouseWheelDelta(0.0f)
            , windowWidth(0)
            , windowHeight(0)
        {}
    };

    // ========================================
    // Platform Interface
    // ========================================

    class IPlatform
    {
    public:
        virtual ~IPlatform() = default;

        // Initialization and shutdown
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        // Window management
        virtual WindowHandle CreateWindow(const WindowDesc& desc) = 0;
        virtual void DestroyWindow(WindowHandle handle) = 0;

        // Event handling
        virtual bool PollEvents() = 0;
        virtual InputEvent GetNextInputEvent() = 0;
        virtual void PushInputEvent(const InputEvent& event) = 0;

        // Platform-specific access
        virtual void* GetNativeWindowHandle(WindowHandle handle) = 0;
        virtual void SetExternalWindowHandle(void* handle) = 0;

        // Graphics context (OpenGL 등 실제 GPU 렌더링 표면을 창에 바인딩).
        // Renderer 서브시스템이 Initialize()되기 전, 즉 GL 함수 포인터가 필요한
        // 어떤 코드보다도 먼저 호출되어야 한다.
        virtual bool CreateGraphicsContext(WindowHandle handle) = 0;

        // 이 창의 GL 컨텍스트를 호출 스레드의 current 컨텍스트로 만든다. 이미 current면 아무 것도
        // 하지 않는다. 컨텍스트가 없거나 전환에 실패하면 false를 반환한다. 이때 호출자는 이번 프레임에
        // GL 호출을 하면 안 된다.
        // 매 프레임 GL 호출 전에 불러야 한다(Engine::TickFrame). 한 프로세스에 Engine이 여러 개
        // 있으면(Scene 뷰포트 + Play 뷰포트) current 컨텍스트는 스레드 단위 상태라 마지막으로
        // 컨텍스트를 만든 쪽 것으로 바뀌어 있기 때문이다.
        virtual bool MakeGraphicsContextCurrent(WindowHandle handle) = 0;

        // 이번 프레임에 그린 백버퍼를 화면에 표시(더블버퍼 스왑)한다.
        // 매 프레임 렌더링 이후(Renderer::LateTick 이후) 호출되어야 한다.
        virtual void PresentFrame(WindowHandle handle) = 0;

        // 커서 잠금(docs/USABILITY_IMPROVEMENT_PLAN.md G3): 커서를 창 안에 가두고 숨긴다. "요청"과 "실제 적용"을
        // 나눈다 - 게임(game.json, 스크립트)은 요청만 하고, 적용은 창이 포커스를 가진 동안만 한다. 포커스를 잃으면
        // Engine::HandleFocusLost가 SuspendCursorLock으로 적용만 풀고(요청은 유지), 창 안을 다시 클릭하면 요청이 켜져
        // 있을 때만 다시 건다. 에디터 임베드 모드에서는 Qt가 창을 소유하므로 요청을 받기만 하고 적용하지 않는다.
        // 한계: 커서를 가두기만 한다(가운데로 되돌리지 않음) - 창 가장자리에서는 마우스 이동량이 멈춘다.
        // 기본 구현은 아무것도 하지 않는다(테스트용 가짜 플랫폼, 커서 개념이 없는 플랫폼).
        virtual void SetCursorLockRequested(bool requested) { (void)requested; }
        virtual bool IsCursorLockRequested() const { return false; }
        virtual bool IsCursorLockApplied() const { return false; }
        virtual void SuspendCursorLock() {}
    };

} // namespace Engine
