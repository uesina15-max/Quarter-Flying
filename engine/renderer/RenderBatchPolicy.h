#pragma once

#include <cstdint>
#include <functional>

namespace Engine
{
    // ============================================================================
    // Pipeline Features & State Flags
    // ============================================================================
    enum class PipelineFeature : uint32_t
    {
        None        = 0,
        Blend       = 1 << 0,
        DepthTest   = 1 << 1,
        DepthWrite  = 1 << 2,
        AlphaTest   = 1 << 3,
        Wireframe   = 1 << 4,
        Culling     = 1 << 5
    };

    inline PipelineFeature operator|(PipelineFeature a, PipelineFeature b)
    {
        return static_cast<PipelineFeature>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline PipelineFeature operator&(PipelineFeature a, PipelineFeature b)
    {
        return static_cast<PipelineFeature>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    // ============================================================================
    // Render Batch Key (Immutable Batching Policy Object)
    // ============================================================================
    struct RenderBatchKey
    {
        uint64_t meshGuid{0};
        uint64_t materialId{0};
        uint32_t shaderId{0};
        PipelineFeature features{PipelineFeature::None};

        bool operator==(const RenderBatchKey& other) const
        {
            return meshGuid == other.meshGuid &&
                   materialId == other.materialId &&
                   shaderId == other.shaderId &&
                   features == other.features;
        }
    };
} // namespace Engine

namespace std
{
    template <>
    struct hash<Engine::RenderBatchKey>
    {
        size_t operator()(const Engine::RenderBatchKey& key) const noexcept
        {
            size_t h1 = std::hash<uint64_t>{}(key.meshGuid);
            size_t h2 = std::hash<uint64_t>{}(key.materialId);
            size_t h3 = std::hash<uint32_t>{}(key.shaderId);
            size_t h4 = std::hash<uint32_t>{}(static_cast<uint32_t>(key.features));
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}
