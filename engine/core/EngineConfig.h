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
