#pragma once
#include <cstdint>
#include <string>

namespace Engine {

// Global Engine Configuration Constants
constexpr uint32_t kDefaultWindowWidth = 1280;
constexpr uint32_t kDefaultWindowHeight = 720;

struct EngineConfig
{
    // Window configuration
    std::string windowTitle;
    uint32_t windowWidth;
    uint32_t windowHeight;
    bool windowFullscreen;

    // Frame allocator size (in bytes)
    size_t frameAllocatorSize;

    // Job system configuration
    uint32_t numWorkerThreads;

    // Logging configuration
    uint32_t logFrameInterval; // Log frame stats every N frames

    // 에셋 경로(RenderableComponent::meshPath 등)의 기준 디렉터리. 비어 있으면 현재 작업
    // 디렉터리 기준. 에디터는 engine/ 절대경로를 넣는다(scene.json/프리팹의 경로가
    // "assets/models/..." 형태로 engine/ 기준이기 때문).
    std::string assetRoot;

    // 디버그 그리드를 그릴지. 에디터에서는 켜고, 게임 실행 파일(Player)에서는 끈다.
    // 그리드는 뷰 카메라와 상관없이 그려지므로(Renderer::Render), 끄지 않으면 게임 화면에도 나온다
    // (docs/GAME_BUILD_PLAN.md §3.5).
    bool showDebugGrid;

    // 스크립트를 인스턴스화할 때마다 모듈을 다시 불러올지(ScriptSystem). 에디터에서 스크립트를
    // 고친 뒤 Play를 다시 누르면 반영되게 하는 기능이라, 게임 실행 파일에서는 끈다.
    bool scriptHotReload;

    // 독립 창(게임 실행 파일)에서 시작할 때 커서 잠금을 요청할지(game.json window.cursorLock, IPlatform 주석).
    // 스크립트가 quarterflying.input.set_cursor_locked로 나중에 바꿀 수 있다. 에디터 임베드 모드에서는 무시된다.
    bool cursorLock;

    EngineConfig()
        : windowTitle("Quarter Flying")
        , windowWidth(kDefaultWindowWidth)
        , windowHeight(kDefaultWindowHeight)
        , windowFullscreen(false)
        , frameAllocatorSize(16 * 1024 * 1024) // 16 MB
        , numWorkerThreads(0) // 0 = auto-detect
        , logFrameInterval(60) // Log every 60 frames
        , showDebugGrid(true)
        , scriptHotReload(true)
        , cursorLock(false)
    {}
};

} // namespace Engine
