#pragma once

#include "IPlatform.h"
#include <Windows.h>
#include <optional>
#include <queue>
#include <unordered_map>
#include <mutex>

// Undefine Windows macros that conflict with our method names
#ifdef CreateWindow
#undef CreateWindow
#endif

namespace Engine
{
    class Win32Platform : public IPlatform
    {
    public:
        Win32Platform();
        ~Win32Platform() override;

        // IPlatform interface
        bool Initialize() override;
        void Shutdown() override;

        WindowHandle CreateWindow(const WindowDesc& desc) override;
        void DestroyWindow(WindowHandle handle) override;

        bool PollEvents() override;
        InputEvent GetNextInputEvent() override;

        void* GetNativeWindowHandle(WindowHandle handle) override;
        void SetExternalWindowHandle(void* handle) override;
        void PushInputEvent(const InputEvent& event) override;

        bool CreateGraphicsContext(WindowHandle handle) override;
        bool MakeGraphicsContextCurrent(WindowHandle handle) override;
        void PresentFrame(WindowHandle handle) override;

        void SetCursorLockRequested(bool requested) override;
        bool IsCursorLockRequested() const override { return cursorLockRequested; }
        bool IsCursorLockApplied() const override { return cursorLockApplied; }
        void SuspendCursorLock() override;

    private:
        struct WindowData
        {
            HWND hwnd;
            uint32_t width;
            uint32_t height;
            bool isValid;

            // OpenGL 렌더링 컨텍스트 (CreateGraphicsContext 호출 전에는 nullptr).
            HDC hdc = nullptr;
            HGLRC hglrc = nullptr;
        };

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleWindowLifecycleMessage(Win32Platform* self, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleKeyboardMessage(Win32Platform* self, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleMouseMessage(Win32Platform* self, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        KeyCode TranslateKeyCode(WPARAM wParam, LPARAM lParam);

        // 커서 잠금 적용/해제(IPlatform 주석). Apply는 임베드 모드이거나 창이 전경이 아니면 아무것도 하지 않는다.
        void ApplyCursorLock(HWND hwnd);
        void ReleaseCursorLock();

        HINSTANCE hInstance;
        std::unordered_map<uint32_t, WindowData> windows;
        std::unordered_map<HWND, uint32_t> hwndToWindowID; // 역방향 매핑 (Qt와의 충돌 회피)
        std::queue<InputEvent> eventQueue;
        std::mutex eventMutex;

        uint32_t nextWindowID;
        uint32_t nextGeneration;

        // Mouse tracking
        int32_t lastMouseX;
        int32_t lastMouseY;

        bool initialized;

        // SetExternalWindowHandle()로 호스트(Qt 에디터)의 HWND에 임베드된 경우 true.
        // 이 스레드의 메시지 루프 주인은 호스트이므로 PollEvents()가 메시지를 펌프하면 안 된다.
        bool hostOwnsMessageLoop = false;

        bool cursorLockRequested = false;
        bool cursorLockApplied = false;
        HWND cursorLockWindow = nullptr;     // 독립 창 모드의 게임 창(CreateWindow)
    };

} // namespace Engine
