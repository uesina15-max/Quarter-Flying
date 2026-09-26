#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include "../core/EngineError.h"

namespace Engine
{
    // Forward declarations
    class Mesh;
    class Camera;
    class CommandList;

    // ============================================================================
    // Occlusion Query Type
    // ============================================================================
    enum class OcclusionQueryType : uint32_t
    {
        AnySamplesPassed = 0,      // True if any samples pass (binary)
        AnySamplesPassedConservative = 1,  // Less accurate but faster
        SamplesPassed = 2          // Returns actual sample count
    };

    // ============================================================================
    // Occlusion Query Handle
    // ============================================================================
    struct OcclusionQueryHandle
    {
        uint32_t queryId;
        bool isActive;
        // GL's glGetQueryObjectuiv(..., GL_QUERY_RESULT_AVAILABLE, GLuint*) writes into this
        // as a GLuint (0/1), not a bool -- kept as uint32_t so &resultAvailable is a valid
        // GLuint* without a reinterpret_cast. Still works fine in boolean contexts (if (handle->resultAvailable)).
        uint32_t resultAvailable;
        uint64_t result;
        // Query target this handle was opened with (engine-level enum, not the raw GLenum,
        // to keep this header free of a <GL/glew.h> dependency). EndOcclusionQuery() must
        // call glEndQuery() with the same target glBeginQuery() used, so this needs to be
        // remembered per-handle rather than recomputed from a caller-supplied parameter that
        // EndOcclusionQuery() doesn't receive.
        OcclusionQueryType queryType;

        OcclusionQueryHandle() : queryId(0), isActive(false), resultAvailable(0), result(0), queryType(OcclusionQueryType::AnySamplesPassed) {}
    };

    // ============================================================================
    // Bounding Volume Types
    // ============================================================================
    enum class BoundingVolumeType : uint32_t
    {
        Sphere = 0,
        AABB = 1,
        OBB = 2
    };

    // ============================================================================
    // Bounding Volume
    // ============================================================================
    struct BoundingVolume
    {
        BoundingVolumeType type;
        glm::vec3 center;
        glm::vec3 extents;  // For AABB: half-extents, for Sphere: radius
        
        BoundingVolume() : type(BoundingVolumeType::Sphere), center(0.0f), extents(1.0f) {}
        BoundingVolume(BoundingVolumeType t, const glm::vec3& c, const glm::vec3& e) 
            : type(t), center(c), extents(e) {}
    };

    // ============================================================================
    // Occlusion Culling Statistics
    // ============================================================================
    struct OcclusionStats
    {
        uint32_t totalQueries{0};
        uint32_t activeQueries{0};
        uint32_t cachedResults{0};
        uint32_t occludedObjects{0};
        uint32_t visibleObjects{0};
        uint32_t queryChained{0};
        float queryTimeMs{0.0f};
        float cacheHitRate{0.0f};
        
        void Reset()
        {
            totalQueries = 0;
            activeQueries = 0;
            cachedResults = 0;
            occludedObjects = 0;
            visibleObjects = 0;
            queryChained = 0;
            queryTimeMs = 0.0f;
            cacheHitRate = 0.0f;
        }
    };

    // ============================================================================
    // Occlusion Culling System
    // ============================================================================
    class OcclusionCullingSystem
    {
    public:
        OcclusionCullingSystem();
        ~OcclusionCullingSystem();

        // Initialization
        Result<void> Initialize();
        void Shutdown();

        // Query Management
        Result<void> BeginOcclusionQuery(uint32_t objectId, const BoundingVolume& volume);
        Result<void> EndOcclusionQuery(uint32_t objectId);
        bool IsObjectVisible(uint32_t objectId);
        bool IsQueryResultAvailable(uint32_t objectId);

        // Batch Processing
        void BeginFrame();
        void EndFrame();
        void ProcessPendingQueries();

        // Visibility Testing
        bool TestVisibility(uint32_t objectId, const BoundingVolume& volume, CommandList& cmdList);
        
        // Spatial Partitioning for Hierarchical Culling
        void SetEnableHierarchicalCulling(bool enable) { hierarchicalCulling = enable; }
        bool IsHierarchicalCullingEnabled() const { return hierarchicalCulling; }

        // Configuration
        void SetQueryType(OcclusionQueryType type) { queryType = type; }
        OcclusionQueryType GetQueryType() const { return queryType; }
        
        void SetMaxConcurrentQueries(uint32_t max) { maxConcurrentQueries = max; }
        uint32_t GetMaxConcurrentQueries() const { return maxConcurrentQueries; }
        
        void SetEnableQueryChaining(bool enable) { queryChaining = enable; }
        bool IsQueryChainingEnabled() const { return queryChaining; }

        // Statistics
        const OcclusionStats& GetStats() const { return stats; }
        void ResetStats() { stats.Reset(); }

    private:
        // Internal helpers
        OcclusionQueryHandle* FindQuery(uint32_t objectId);
        Result<void> CreateQuery(OcclusionQueryHandle& handle);
        void DestroyQuery(OcclusionQueryHandle& handle);
        void IssueQuery(OcclusionQueryHandle& handle, const BoundingVolume& volume, CommandList& cmdList);
        
        // Spatial queries
        bool ShouldTestObject(uint32_t objectId, const BoundingVolume& volume);
        
        // Render bounding volume for occlusion test
        void RenderBoundingVolume(const BoundingVolume& volume, CommandList& cmdList);

    private:
        std::unordered_map<uint32_t, OcclusionQueryHandle> queries;
        
        OcclusionStats stats;
        
        // Configuration
        OcclusionQueryType queryType;
        uint32_t maxConcurrentQueries;
        bool hierarchicalCulling;
        bool queryChaining;
        
        // Frame state
        bool frameInProgress;
        uint32_t currentFrameQueries;
        
        bool initialized;
    };

} // namespace Engine