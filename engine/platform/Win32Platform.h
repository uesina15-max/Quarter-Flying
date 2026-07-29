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

    private:
        struct WindowData
        {
            HWND hwnd;
            uint32_t width;
            uint32_t height;
            bool isValid;
        };

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleWindowLifecycleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleKeyboardMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static std::optional<LRESULT> HandleMouseMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        KeyCode TranslateKeyCode(WPARAM wParam, LPARAM lParam);

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
    };

} // namespace Engine
