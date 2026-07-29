#pragma once

#include <cstdint>
#include <memory>
#include "../core/EngineError.h"

namespace Engine
{
    // ============================================================================
    // Dynamic Resolution Scaling Mode
    // ============================================================================
    enum class DynamicResolutionMode : uint32_t
    {
        Disabled = 0,
        Automatic = 1,    // Automatically adjust based on frame time
        Manual = 2,       // Manually set scale factor
        Adaptive = 3      // Adaptive based on content complexity
    };

    // ============================================================================
    // Dynamic Resolution Statistics
    // ============================================================================
    struct DynamicResolutionStats
    {
        float currentScale{1.0f};        // Current resolution scale (0.5 = half resolution)
        float targetScale{1.0f};        // Target resolution scale
        uint32_t resolutionWidth{1920};  // Current render width
        uint32_t resolutionHeight{1080}; // Current render height
        uint32_t nativeWidth{1920};      // Native display width
        uint32_t nativeHeight{1080};     // Native display height
        
        float frameTimeMs{16.67f};       // Current frame time (ms)
        float targetFrameTimeMs{16.67f}; // Target frame time (60 FPS)
        float minFrameTimeMs{8.33f};    // Minimum frame time (120 FPS)
        float maxFrameTimeMs{33.33f};   // Maximum frame time (30 FPS)
        
        uint32_t scaleUpCount{0};       // Number of times scaled up
        uint32_t scaleDownCount{0};     // Number of times scaled down
        uint32_t stableFrames{0};       // Frames at current scale
        
        void Reset()
        {
            currentScale = 1.0f;
            targetScale = 1.0f;
            resolutionWidth = nativeWidth;
            resolutionHeight = nativeHeight;
            frameTimeMs = 16.67f;
            scaleUpCount = 0;
            scaleDownCount = 0;
            stableFrames = 0;
        }
    };

    // ============================================================================
    // Dynamic Resolution Configuration
    // ============================================================================
    struct DynamicResolutionConfig
    {
        float minScale{0.5f};           // Minimum resolution scale (50%)
        float maxScale{1.0f};           // Maximum resolution scale (100%)
        float scaleStep{0.05f};          // Scale adjustment step (5%)
        uint32_t stableFramesThreshold{30}; // Frames before considering scale change
        float frameTimeTolerance{1.0f}; // Tolerance for frame time (ms)
        
        DynamicResolutionConfig() = default;
    };

    // ============================================================================
    // Dynamic Resolution System
    // ============================================================================
    class DynamicResolutionSystem
    {
    public:
        DynamicResolutionSystem();
        ~DynamicResolutionSystem();

        // Initialization
        Result<void> Initialize(uint32_t nativeWidth, uint32_t nativeHeight);
        void Shutdown();

        // Configuration
        void SetMode(DynamicResolutionMode mode) { this->mode = mode; }
        DynamicResolutionMode GetMode() const { return mode; }
        
        void SetConfig(const DynamicResolutionConfig& config) { this->config = config; }
        const DynamicResolutionConfig& GetConfig() const { return config; }
        
        void SetTargetFrameTime(float frameTimeMs) { stats.targetFrameTimeMs = frameTimeMs; }
        float GetTargetFrameTime() const { return stats.targetFrameTimeMs; }

        // Resolution Control
        void SetManualScale(float scale);
        void Update(float frameTimeMs);
        
        // Resolution Access
        float GetCurrentScale() const { return stats.currentScale; }
        uint32_t GetRenderWidth() const { return stats.resolutionWidth; }
        uint32_t GetRenderHeight() const { return stats.resolutionHeight; }
        
        // Statistics
        const DynamicResolutionStats& GetStats() const { return stats; }
        void ResetStats() { stats.Reset(); }

        // Utility
        bool ShouldRenderAtNativeResolution() const { return stats.currentScale >= 0.95f; }
        float GetResolutionRatio() const { return static_cast<float>(stats.resolutionWidth) / stats.nativeWidth; }

    private:
        // Internal helpers
        void UpdateAutomaticScale(float frameTimeMs);
        void UpdateAdaptiveScale(float frameTimeMs);
        void ApplyScale(float scale);
        bool ShouldScaleUp(float frameTimeMs) const;
        bool ShouldScaleDown(float frameTimeMs) const;
        
        void ClampScale(float& scale) const;
        void UpdateResolutionFromScale();

    private:
        DynamicResolutionMode mode;
        DynamicResolutionConfig config;
        DynamicResolutionStats stats;
        
        bool initialized;
    };

} // namespace Engine