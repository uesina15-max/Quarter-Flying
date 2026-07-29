#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include "RenderBatchPolicy.h"
#include "../core/EngineError.h"

namespace Engine
{
    // Forward declarations
    class Mesh;
    class Shader;

    // ============================================================================
    // Pass Type Enumeration
    // ============================================================================
    enum class PassType : uint32_t
    {
        ForwardOpaque = 0,
        ForwardTransparent = 1,
        DeferredGeometry = 2,
        DeferredLighting = 3,
        Shadow = 4,
        Custom = 5
    };

    // ============================================================================
    // Material Layout Enumeration
    // ============================================================================
    enum class MaterialLayout : uint32_t
    {
        PBR = 0,
        Phong = 1,
        Unlit = 2,
        Custom = 3
    };

    // ============================================================================
    // Enhanced Batch Key for Instance Rendering
    // ============================================================================
    struct InstancedBatchKey
    {
        uint64_t meshGuid{0};           // Unique mesh identifier
        uint64_t materialId{0};         // Unique material identifier
        uint32_t shaderId{0};           // Shader program ID
        PassType passType{PassType::ForwardOpaque};
        MaterialLayout materialLayout{MaterialLayout::PBR};
        PipelineFeature features{PipelineFeature::None};

        bool operator==(const InstancedBatchKey& other) const
        {
            return meshGuid == other.meshGuid &&
                   materialId == other.materialId &&
                   shaderId == other.shaderId &&
                   passType == other.passType &&
                   materialLayout == other.materialLayout &&
                   features == other.features;
        }

        // Hash function support
        size_t hash() const noexcept
        {
            size_t h1 = std::hash<uint64_t>{}(meshGuid);
            size_t h2 = std::hash<uint64_t>{}(materialId);
            size_t h3 = std::hash<uint32_t>{}(shaderId);
            size_t h4 = std::hash<uint32_t>{}(static_cast<uint32_t>(passType));
            size_t h5 = std::hash<uint32_t>{}(static_cast<uint32_t>(materialLayout));
            size_t h6 = std::hash<uint32_t>{}(static_cast<uint32_t>(features));
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
        }
    };

    // Hash specialization for std::unordered_map
    template <>
    struct std::hash<InstancedBatchKey>
    {
        size_t operator()(const InstancedBatchKey& key) const noexcept
        {
            return key.hash();
        }
    };

    // ============================================================================
    // Instance Data Structure (GPU-aligned)
    // ============================================================================
    struct InstanceData
    {
        glm::mat4 model;          // 64 bytes - Model transformation matrix
        glm::vec3 color;          // 12 bytes - Material color (for scalar/vector materials)
        float roughness;          // 4 bytes  - Roughness value
        float metallic;           // 4 bytes  - Metallic value
        uint32_t entityId;        // 4 bytes  - Entity ID for picking/editor
        uint32_t isSelected;      // 4 bytes  - Selection flag for highlighting
        
        // Total: 92 bytes (should be aligned to 16-byte boundary)
        // Padding to ensure 16-byte alignment
        float _padding[3];        // 12 bytes padding
        
        static_assert(sizeof(InstanceData) % 16 == 0, "InstanceData must be 16-byte aligned for GPU compatibility");
    };

    // ============================================================================
    // Batch Type Enumeration
    // ============================================================================
    enum class BatchType : uint32_t
    {
        Static = 0,    // Immutable data (map objects, etc.)
        Dynamic = 1    // Mutable data (moving objects, editor changes)
    };

    // ============================================================================
    // Instance Batch Container
    // ============================================================================
    struct InstanceBatch
    {
        InstancedBatchKey key;
        BatchType type;
        
        // GPU resources
        uint32_t instanceVBO{0};
        uint32_t instanceVAO{0};
        
        // CPU data
        std::vector<InstanceData> instanceData;
        std::vector<uint32_t> entityIds;  // InstanceID -> EntityID mapping for picking
        
        // State tracking
        bool isDirty{true};
        bool isVisible{true};
        bool markedForDeletion{false};
        uint32_t visibleCount{0};
        
        // Mesh reference (shared_ptr for dedup)
        std::shared_ptr<Mesh> mesh;
        
        InstanceBatch() = default;
        ~InstanceBatch();
        
        // Prevent copying
        InstanceBatch(const InstanceBatch&) = delete;
        InstanceBatch& operator=(const InstanceBatch&) = delete;
        
        // Allow moving
        InstanceBatch(InstanceBatch&& other) noexcept;
        InstanceBatch& operator=(InstanceBatch&& other) noexcept;
    };

    // ============================================================================
    // Renderer Statistics
    // ============================================================================
    struct RendererStats
    {
        uint32_t drawCalls{0};
        uint32_t totalInstances{0};
        uint32_t visibleInstances{0};
        uint32_t staticBatches{0};
        uint32_t dynamicBatches{0};
        uint32_t culledInstances{0};
        uint32_t frustumCulled{0};
        uint32_t distanceCulled{0};
        uint32_t occlusionCulled{0};
        float cullingTimeMs{0.0f};
        
        void Reset()
        {
            drawCalls = 0;
            totalInstances = 0;
            visibleInstances = 0;
            staticBatches = 0;
            dynamicBatches = 0;
            culledInstances = 0;
            frustumCulled = 0;
            distanceCulled = 0;
            occlusionCulled = 0;
            cullingTimeMs = 0.0f;
        }
    };

    // ============================================================================
    // Instanced Batch Manager
    // ============================================================================
    class InstancedBatchManager
    {
    public:
        InstancedBatchManager();
        ~InstancedBatchManager();

        // Initialization
        Result<void> Initialize();
        void Shutdown();

        // Batch management
        Result<void> CreateBatch(const InstancedBatchKey& key, BatchType type, std::shared_ptr<Mesh> mesh);
        Result<void> AddInstance(const InstancedBatchKey& key, const InstanceData& data, uint32_t entityId);
        Result<void> UpdateInstance(const InstancedBatchKey& key, uint32_t instanceIndex, const InstanceData& data);
        Result<void> RemoveBatch(const InstancedBatchKey& key);
        
        // Visibility and culling
        void SetBatchVisibility(const InstancedBatchKey& key, bool visible);
        void UpdateVisibility(const class Frustum& frustum);
        
        // Rendering
        void RenderBatches(PassType passType, Shader* shader);
        void RenderBatch(const InstancedBatchKey& key, Shader* shader);
        
        // Lifecycle management
        void UpdateDynamicBatches();  // Upload dirty dynamic data to GPU
        void CleanupStaleBatches();
        void SortBatchesForRendering();  // Sort batches for optimal rendering order
        
        // Statistics
        const RendererStats& GetStats() const { return stats; }
        void ResetStats() { stats.Reset(); }
        
        // Validation
        static bool ValidateOpenGLCapabilities();
        static uint32_t GetMaxVertexAttributes();

    private:
        // Internal helpers
        InstanceBatch* FindBatch(const InstancedBatchKey& key);
        const InstanceBatch* FindBatch(const InstancedBatchKey& key) const;
        
        // Sorted batch cache for rendering
        std::vector<std::pair<InstancedBatchKey, InstanceBatch*>> sortedBatches;
        bool batchesNeedSorting{true};
        
        Result<void> UploadInstanceBuffer(InstanceBatch& batch);
        Result<void> SetupInstanceVAO(InstanceBatch& batch);
        void ReleaseBatchResources(InstanceBatch& batch);
        
        // Sorting for render state optimization
        void SortBatchesForRendering();
        
    private:
        std::unordered_map<InstancedBatchKey, std::unique_ptr<InstanceBatch>> batches;
        RendererStats stats;
        
        bool initialized;
        uint32_t maxVertexAttribs;
    };

} // namespace Engine
