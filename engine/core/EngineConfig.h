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

    EngineConfig()
        : windowTitle("Quarter Flying")
        , windowWidth(kDefaultWindowWidth)
        , windowHeight(kDefaultWindowHeight)
        , windowFullscreen(false)
        , frameAllocatorSize(16 * 1024 * 1024) // 16 MB
        , numWorkerThreads(0) // 0 = auto-detect
        , logFrameInterval(60) // Log every 60 frames
    {}
};

} // namespace Engine
