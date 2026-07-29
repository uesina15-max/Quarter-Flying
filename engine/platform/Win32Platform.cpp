/*
 * Copyright 2026 Quarter Flying Game Engine Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Win32Platform.h"
#include "../core/logging/Logger.h"
#include "../core/assert/Assert.h"
#include <windowsx.h>

// Undefine Windows macros that conflict with our method names
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef DestroyWindow
#undef DestroyWindow
#endif

namespace Engine
{
    // Static pointer to access platform instance in WindowProc
    static Win32Platform* g_PlatformInstance = nullptr;

    Win32Platform::Win32Platform()
        : hInstance(nullptr)
        , nextWindowID(1)
        , nextGeneration(1)
        , lastMouseX(0)
        , lastMouseY(0)
        , initialized(false)
    {
        g_PlatformInstance = this;
    }

    Win32Platform::~Win32Platform()
    {
        if (initialized)
        {
            Shutdown();
        }
        g_PlatformInstance = nullptr;
    }

    bool Win32Platform::Initialize()
    {
        if (initialized)
        {
            Logger::Log(LogLevel::Warning, "Win32Platform already initialized");
            return true;
        }

        hInstance = GetModuleHandle(nullptr);
        if (!hInstance)
        {
            DWORD error = GetLastError();
            Logger::Log(LogLevel::Error, "Failed to get module handle (Error: {})", error);
            return false;
        }

        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"EngineWindowClass";

        if (!RegisterClassExW(&wc))
        {
            DWORD error = GetLastError();
            Logger::Log(LogLevel::Error, "Failed to register window class (Error: {})", error);
            return false;
        }

        initialized = true;
        Logger::Log(LogLevel::Info, "Win32Platform initialized successfully");
        return true;
    }

    void Win32Platform::Shutdown()
    {
        if (!initialized)
        {
            return;
        }

        // Destroy all windows
        for (auto& pair : windows)
        {
            if (pair.second.isValid && pair.second.hwnd)
            {
                ::DestroyWindow(pair.second.hwnd);
            }
        }
        windows.clear();

        // Unregister window class
        UnregisterClassW(L"EngineWindowClass", hInstance);

        initialized = false;
        Logger::Log(LogLevel::Info, "Win32Platform shutdown");
    }

    WindowHandle Win32Platform::CreateWindow(const WindowDesc& desc)
    {
        if (!initialized)
        {
            Logger::Log(LogLevel::Error, "Win32Platform not initialized");
            return WindowHandle();
        }

        // Convert title to wide string
        int titleLen = MultiByteToWideChar(CP_UTF8, 0, desc.title.c_str(), -1, nullptr, 0);
        if (titleLen == 0)
        {
            Logger::Log(LogLevel::Error, "Failed to calculate wide string length for window title");
            return WindowHandle();
        }
        
        wchar_t* wideTitle = new wchar_t[titleLen];
        if (!wideTitle)
        {
            Logger::Log(LogLevel::Error, "Failed to allocate memory for wide string");
            return WindowHandle();
        }
        
        int conversionResult = MultiByteToWideChar(CP_UTF8, 0, desc.title.c_str(), -1, wideTitle, titleLen);
        if (conversionResult == 0)
        {
            Logger::Log(LogLevel::Error, "Failed to convert window title to wide string");
            delete[] wideTitle;
            return WindowHandle();
        }

        // Calculate window size including borders
        DWORD style = WS_OVERLAPPEDWINDOW;
        RECT rect = { 0, 0, (LONG)desc.width, (LONG)desc.height };
        AdjustWindowRect(&rect, style, FALSE);

        int windowWidth = rect.right - rect.left;
        int windowHeight = rect.bottom - rect.top;

        // Create window
        HWND hwnd = CreateWindowExW(
            0,
            L"EngineWindowClass",
            wideTitle,
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowWidth, windowHeight,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );

        delete[] wideTitle;

        if (!hwnd)
        {
            DWORD error = GetLastError();
            Logger::Log(LogLevel::Error, "Failed to create Win32 window (Error: {})", error);
            delete[] wideTitle;
            return WindowHandle();
        }

        // Show window
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        // Store window data
        uint32_t windowID = nextWindowID++;
        WindowData windowData;
        windowData.hwnd = hwnd;
        windowData.width = desc.width;
        windowData.height = desc.height;
        windowData.isValid = true;

        windows[windowID] = windowData;

        // 역방향 매핑 추가 (Qt와의 충돌 회피)
        hwndToWindowID[hwnd] = windowID;

        WindowHandle handle(windowID, nextGeneration++);

        Logger::Log(LogLevel::Info, "Win32 window created");
        return handle;
    }

    void Win32Platform::DestroyWindow(WindowHandle handle)
    {
        auto it = windows.find(handle.id);
        if (it == windows.end() || !it->second.isValid)
        {
            Logger::Log(LogLevel::Warning, "Attempted to destroy invalid window");
            return;
        }

        if (it->second.hwnd)
        {
            // 역방향 매핑 제거
            hwndToWindowID.erase(it->second.hwnd);
            ::DestroyWindow(it->second.hwnd);
        }

        it->second.isValid = false;
        windows.erase(it);

        Logger::Log(LogLevel::Info, "Win32 window destroyed");
    }

    bool Win32Platform::PollEvents()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return true;
    }

    InputEvent Win32Platform::GetNextInputEvent()
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        if (eventQueue.empty())
        {
            return InputEvent();
        }

        InputEvent event = eventQueue.front();
        eventQueue.pop();
        return event;
    }

    void* Win32Platform::GetNativeWindowHandle(WindowHandle handle)
    {
        auto it = windows.find(handle.id);
        if (it == windows.end() || !it->second.isValid)
        {
            return nullptr;
        }

        return (void*)it->second.hwnd;
    }

    void Win32Platform::SetExternalWindowHandle(void* handle)
    {
        if (!initialized)
        {
            Logger::Log(LogLevel::Error, "Win32Platform not initialized before setting external handle");
            return;
        }

        HWND hwnd = static_cast<HWND>(handle);
        if (!hwnd)
        {
            Logger::Log(LogLevel::Error, "Invalid external window handle");
            return;
        }

        // Store external window data as ID 1
        uint32_t windowID = 1;
        WindowData windowData;
        windowData.hwnd = hwnd;

        RECT rect;
        GetClientRect(hwnd, &rect);
        windowData.width = rect.right - rect.left;
        windowData.height = rect.bottom - rect.top;
        windowData.isValid = true;

        windows[windowID] = windowData;

        // 역방향 매핑 추가 (Qt와의 충돌 회피)
        hwndToWindowID[hwnd] = windowID;

        Logger::Log(LogLevel::Info, "External Win32 window registered (ID: 1)");
    }

    LRESULT CALLBACK Win32Platform::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (!g_PlatformInstance)
        {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        if (auto res = HandleWindowLifecycleMessage(hwnd, msg, wParam, lParam))
        {
            return *res;
        }
        if (auto res = HandleKeyboardMessage(hwnd, msg, wParam, lParam))
        {
            return *res;
        }
        if (auto res = HandleMouseMessage(hwnd, msg, wParam, lParam))
        {
            return *res;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    std::optional<LRESULT> Win32Platform::HandleWindowLifecycleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        InputEvent event;
        switch (msg)
        {
        case WM_CLOSE:
            event.type = InputEventType::WindowClose;
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_SIZE:
            event.type = InputEventType::WindowResize;
            event.windowWidth = LOWORD(lParam);
            event.windowHeight = HIWORD(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return std::nullopt;
    }

    std::optional<LRESULT> Win32Platform::HandleKeyboardMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        InputEvent event;
        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            event.type = InputEventType::KeyDown;
            event.keyCode = g_PlatformInstance->TranslateKeyCode(wParam, lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            event.type = InputEventType::KeyUp;
            event.keyCode = g_PlatformInstance->TranslateKeyCode(wParam, lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;
        }
        return std::nullopt;
    }

    std::optional<LRESULT> Win32Platform::HandleMouseMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        InputEvent event;
        switch (msg)
        {
        case WM_MOUSEMOVE:
            event.type = InputEventType::MouseMove;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            event.mouseDeltaX = event.mouseX - g_PlatformInstance->lastMouseX;
            event.mouseDeltaY = event.mouseY - g_PlatformInstance->lastMouseY;
            g_PlatformInstance->lastMouseX = event.mouseX;
            g_PlatformInstance->lastMouseY = event.mouseY;
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_LBUTTONDOWN:
            event.type = InputEventType::MouseButtonDown;
            event.mouseButton = MouseButton::Left;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_LBUTTONUP:
            event.type = InputEventType::MouseButtonUp;
            event.mouseButton = MouseButton::Left;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_RBUTTONDOWN:
            event.type = InputEventType::MouseButtonDown;
            event.mouseButton = MouseButton::Right;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_RBUTTONUP:
            event.type = InputEventType::MouseButtonUp;
            event.mouseButton = MouseButton::Right;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_MBUTTONDOWN:
            event.type = InputEventType::MouseButtonDown;
            event.mouseButton = MouseButton::Middle;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_MBUTTONUP:
            event.type = InputEventType::MouseButtonUp;
            event.mouseButton = MouseButton::Middle;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;

        case WM_MOUSEWHEEL:
            event.type = InputEventType::MouseWheel;
            event.mouseWheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            event.mouseX = GET_X_LPARAM(lParam);
            event.mouseY = GET_Y_LPARAM(lParam);
            g_PlatformInstance->PushInputEvent(event);
            return 0;
        }
        return std::nullopt;
    }

    KeyCode Win32Platform::TranslateKeyCode(WPARAM wParam, LPARAM lParam)
    {
        // Handle alphabet and number keys
        if (wParam >= 'A' && wParam <= 'Z')
        {
            return static_cast<KeyCode>(wParam);
        }
        if (wParam >= '0' && wParam <= '9')
        {
            return static_cast<KeyCode>(wParam);
        }

        // Handle function keys
        if (wParam >= VK_F1 && wParam <= VK_F12)
        {
            return static_cast<KeyCode>(wParam);
        }

        // Handle special keys
        switch (wParam)
        {
        case VK_ESCAPE: return KeyCode::Escape;
        case VK_SPACE: return KeyCode::Space;
        case VK_RETURN: return KeyCode::Enter;
        case VK_TAB: return KeyCode::Tab;
        case VK_BACK: return KeyCode::Backspace;
        case VK_SHIFT: return KeyCode::Shift;
        case VK_CONTROL: return KeyCode::Control;
        case VK_MENU: return KeyCode::Alt;
        case VK_LEFT: return KeyCode::Left;
        case VK_UP: return KeyCode::Up;
        case VK_RIGHT: return KeyCode::Right;
        case VK_DOWN: return KeyCode::Down;
        default: return KeyCode::Unknown;
        }
    }

    void Win32Platform::PushInputEvent(const InputEvent& event)
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push(event);
    }

} // namespace Engine
