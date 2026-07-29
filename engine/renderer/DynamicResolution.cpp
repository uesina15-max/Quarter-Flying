#include "DynamicResolution.h"
#include "../core/logging/Logger.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
    // ============================================================================
    // DynamicResolutionSystem Implementation
    // ============================================================================
    
    DynamicResolutionSystem::DynamicResolutionSystem()
        : mode(DynamicResolutionMode::Disabled)
        , initialized(false)
    {
    }
    
    DynamicResolutionSystem::~DynamicResolutionSystem()
    {
        Shutdown();
    }
    
    Result<void> DynamicResolutionSystem::Initialize(uint32_t nativeWidth, uint32_t nativeHeight)
    {
        if (initialized)
        {
            Logger::Log(LogLevel::Warning, "DynamicResolutionSystem::Initialize - Already initialized");
            return {};
        }
        
        Logger::Log(LogLevel::Info, "DynamicResolutionSystem::Initialize - Initializing dynamic resolution system");
        
        stats.nativeWidth = nativeWidth;
        stats.nativeHeight = nativeHeight;
        stats.resolutionWidth = nativeWidth;
        stats.resolutionHeight = nativeHeight;
        
        // Set default target to 60 FPS
        stats.targetFrameTimeMs = 16.67f;
        stats.minFrameTimeMs = 8.33f;   // 120 FPS
        stats.maxFrameTimeMs = 33.33f;  // 30 FPS
        
        initialized = true;
        Logger::Log(LogLevel::Info, "DynamicResolutionSystem::Initialize - Dynamic resolution system initialized successfully");
        return {};
    }
    
    void DynamicResolutionSystem::Shutdown()
    {
        if (!initialized)
        {
            return;
        }
        
        Logger::Log(LogLevel::Info, "DynamicResolutionSystem::Shutdown - Shutting down dynamic resolution system");
        
        // Reset to native resolution
        ApplyScale(1.0f);
        
        initialized = false;
        Logger::Log(LogLevel::Info, "DynamicResolutionSystem::Shutdown - Shutdown complete");
    }
    
    void DynamicResolutionSystem::SetManualScale(float scale)
    {
        if (!initialized)
        {
            return;
        }
        
        ClampScale(scale);
        ApplyScale(scale);
        stats.targetScale = scale;
        
        Logger::Log(LogLevel::Info, "DynamicResolutionSystem::SetManualScale - Set manual scale to {:.2f}", scale);
    }
    
    void DynamicResolutionSystem::Update(float frameTimeMs)
    {
        if (!initialized || mode == DynamicResolutionMode::Disabled)
        {
            return;
        }
        
        stats.frameTimeMs = frameTimeMs;
        
        switch (mode)
        {
            case DynamicResolutionMode::Automatic:
                UpdateAutomaticScale(frameTimeMs);
                break;
            case DynamicResolutionMode::Manual:
                // Manual mode doesn't auto-adjust
                break;
            case DynamicResolutionMode::Adaptive:
                UpdateAdaptiveScale(frameTimeMs);
                break;
            default:
                break;
        }
        
        // Smoothly transition to target scale
        if (stats.currentScale != stats.targetScale)
        {
            float diff = stats.targetScale - stats.currentScale;
            
            // Apply gradual change
            float change = std::copysign(std::min(std::abs(diff), config.scaleStep), diff);
            float newScale = stats.currentScale + change;
            
            ClampScale(newScale);
            ApplyScale(newScale);
            
            // Update statistics
            if (newScale > stats.currentScale)
            {
                stats.scaleUpCount++;
            }
            else if (newScale < stats.currentScale)
            {
                stats.scaleDownCount++;
            }
            
            stats.currentScale = newScale;
        }
        else
        {
            stats.stableFrames++;
        }
    }
    
    void DynamicResolutionSystem::UpdateAutomaticScale(float frameTimeMs)
    {
        // Automatic mode: adjust based on frame time
        if (ShouldScaleDown(frameTimeMs))
        {
            stats.targetScale = std::max(config.minScale, stats.targetScale - config.scaleStep);
            stats.stableFrames = 0;
        }
        else if (ShouldScaleUp(frameTimeMs))
        {
            stats.targetScale = std::min(config.maxScale, stats.targetScale + config.scaleStep);
            stats.stableFrames = 0;
        }
    }
    
    void DynamicResolutionSystem::UpdateAdaptiveScale(float frameTimeMs)
    {
        // Adaptive mode: more sophisticated adjustments
        // Consider content complexity and motion
        float frameTimeRatio = frameTimeMs / stats.targetFrameTimeMs;
        
        if (frameTimeRatio > 1.2f)  // Running 20% slower than target
        {
            // Aggressive scale down
            stats.targetScale = std::max(config.minScale, stats.targetScale - config.scaleStep * 2.0f);
            stats.stableFrames = 0;
        }
        else if (frameTimeRatio > 1.1f)  // Running 10% slower than target
        {
            // Normal scale down
            stats.targetScale = std::max(config.minScale, stats.targetScale - config.scaleStep);
            stats.stableFrames = 0;
        }
        else if (frameTimeRatio < 0.8f && stats.stableFrames > config.stableFramesThreshold)
        {
            // Running faster than target, scale up
            stats.targetScale = std::min(config.maxScale, stats.targetScale + config.scaleStep);
            stats.stableFrames = 0;
        }
    }
    
    void DynamicResolutionSystem::ApplyScale(float scale)
    {
        stats.resolutionWidth = static_cast<uint32_t>(stats.nativeWidth * scale);
        stats.resolutionHeight = static_cast<uint32_t>(stats.nativeHeight * scale);
        
        // Ensure minimum resolution
        stats.resolutionWidth = std::max(320u, stats.resolutionWidth);
        stats.resolutionHeight = std::max(240u, stats.resolutionHeight);
        
        Logger::Log(LogLevel::Trace, "DynamicResolutionSystem::ApplyScale - Applied scale {:.2f}, resolution: {}x{}", 
                    scale, stats.resolutionWidth, stats.resolutionHeight);
    }
    
    bool DynamicResolutionSystem::ShouldScaleUp(float frameTimeMs) const
    {
        // Scale up if we're consistently running faster than target
        if (frameTimeMs < stats.targetFrameTimeMs - config.frameTimeTolerance)
        {
            // Only scale up after stable frames threshold
            return stats.stableFrames >= config.stableFramesThreshold;
        }
        return false;
    }
    
    bool DynamicResolutionSystem::ShouldScaleDown(float frameTimeMs) const
    {
        // Scale down if we're running slower than target
        if (frameTimeMs > stats.targetFrameTimeMs + config.frameTimeTolerance)
        {
            // Immediate scale down for performance
            return true;
        }
        return false;
    }
    
    void DynamicResolutionSystem::ClampScale(float& scale) const
    {
        scale = std::max(config.minScale, std::min(config.maxScale, scale));
    }
    
    void DynamicResolutionSystem::UpdateResolutionFromScale()
    {
        ApplyScale(stats.currentScale);
    }
    
} // namespace Engine