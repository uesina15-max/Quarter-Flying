#include "OcclusionCulling.h"
#include "CommandList.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>
#include <algorithm>

namespace Engine
{
    // ============================================================================
    // OcclusionCullingSystem Implementation
    // ============================================================================
    
    OcclusionCullingSystem::OcclusionCullingSystem()
        : queryType(OcclusionQueryType::AnySamplesPassedConservative)
        , maxConcurrentQueries(64)  // Conservative default
        , hierarchicalCulling(true)
        , queryChaining(true)
        , frameInProgress(false)
        , currentFrameQueries(0)
        , initialized(false)
    {
    }
    
    OcclusionCullingSystem::~OcclusionCullingSystem()
    {
        Shutdown();
    }
    
    Result<void> OcclusionCullingSystem::Initialize()
    {
        if (initialized)
        {
            Logger::Log(LogLevel::Warning, "OcclusionCullingSystem::Initialize - Already initialized");
            return {};
        }
        
        Logger::Log(LogLevel::Info, "OcclusionCullingSystem::Initialize - Initializing occlusion culling system");
        
        // Check for occlusion query support
        if (!glewIsSupported("GL_ARB_occlusion_query2") && !glewIsSupported("GL_ARB_occlusion_query"))
        {
            Logger::Log(LogLevel::Warning, "OcclusionCullingSystem::Initialize - Occlusion queries not supported, system will be disabled");
            // We continue but queries will be effectively disabled
        }
        
        // NOTE: previously called glGetIntegerv(GL_MAX_SAMPLES_PASSED, &maxQueries) here to
        // cap maxConcurrentQueries, but GL_MAX_SAMPLES_PASSED is not a real GL query-able
        // limit (it doesn't exist as a glGetIntegerv pname) -- there is no standard GL
        // constant for "max concurrent occlusion queries"; that's a driver/memory limit, not
        // a queryable one. Removed rather than guessed at a replacement constant;
        // maxConcurrentQueries keeps whatever value the caller set via SetMaxConcurrentQueries()
        // (or its constructor default).
        Logger::Log(LogLevel::Info, "OcclusionCullingSystem::Initialize - Max concurrent queries: {}",
                    maxConcurrentQueries);

        initialized = true;
        Logger::Log(LogLevel::Info, "OcclusionCullingSystem::Initialize - Occlusion culling system initialized successfully");
        return {};
    }
    
    void OcclusionCullingSystem::Shutdown()
    {
        if (!initialized)
        {
            return;
        }
        
        Logger::Log(LogLevel::Info, "OcclusionCullingSystem::Shutdown - Shutting down occlusion culling system");
        
        // Clean up all queries
        for (auto& pair : queries)
        {
            DestroyQuery(pair.second);
        }
        queries.clear();
        
        initialized = false;
        Logger::Log(LogLevel::Info, "OcclusionCullingSystem::Shutdown - Shutdown complete");
    }
    
    Result<void> OcclusionCullingSystem::BeginOcclusionQuery(uint32_t objectId, const BoundingVolume& volume)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Occlusion culling system not initialized", "OcclusionCullingSystem");
        }
        
        if (!frameInProgress)
        {
            return MakeUnexpected(EngineErrorCode::InvalidState, "No frame in progress", "OcclusionCullingSystem");
        }
        
        // Check if we've hit the concurrent query limit
        if (currentFrameQueries >= maxConcurrentQueries)
        {
            Logger::Log(LogLevel::Trace, "OcclusionCullingSystem::BeginOcclusionQuery - Max concurrent queries reached, skipping object {}", 
                        objectId);
            stats.queryChained++;
            return {};
        }
        
        // Find or create query handle
        OcclusionQueryHandle* handle = FindQuery(objectId);
        if (!handle)
        {
            OcclusionQueryHandle newHandle;
            auto result = CreateQuery(newHandle);
            if (!result)
            {
                return result;
            }
            
            queries[objectId] = std::move(newHandle);
            handle = &queries[objectId];
        }
        
        // Reset query state
        handle->isActive = true;
        handle->resultAvailable = 0;
        handle->result = 0;
        handle->queryType = queryType;  // remembered so EndOcclusionQuery() can match glEndQuery()'s target

        // Issue the occlusion query
        GLenum glQueryType = GL_ANY_SAMPLES_PASSED;
        if (queryType == OcclusionQueryType::AnySamplesPassedConservative)
        {
            glQueryType = GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
        }
        else if (queryType == OcclusionQueryType::SamplesPassed)
        {
            glQueryType = GL_SAMPLES_PASSED;
        }

        glBeginQuery(glQueryType, handle->queryId);
        
        currentFrameQueries++;
        stats.totalQueries++;
        stats.activeQueries++;
        
        Logger::Log(LogLevel::Trace, "OcclusionCullingSystem::BeginOcclusionQuery - Started query for object {}", 
                    objectId);
        
        return {};
    }
    
    Result<void> OcclusionCullingSystem::EndOcclusionQuery(uint32_t objectId)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Occlusion culling system not initialized", "OcclusionCullingSystem");
        }
        
        OcclusionQueryHandle* handle = FindQuery(objectId);
        if (!handle || !handle->isActive)
        {
            Logger::Log(LogLevel::Warning, "OcclusionCullingSystem::EndOcclusionQuery - No active query for object {}", 
                        objectId);
            return {};
        }
        
        // glEndQuery() must be called with the same target glBeginQuery() used for this query.
        GLenum glQueryType = GL_ANY_SAMPLES_PASSED;
        if (handle->queryType == OcclusionQueryType::AnySamplesPassedConservative)
        {
            glQueryType = GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
        }
        else if (handle->queryType == OcclusionQueryType::SamplesPassed)
        {
            glQueryType = GL_SAMPLES_PASSED;
        }
        glEndQuery(glQueryType);
        handle->isActive = false;
        stats.activeQueries--;
        
        Logger::Log(LogLevel::Trace, "OcclusionCullingSystem::EndOcclusionQuery - Ended query for object {}", 
                    objectId);
        
        return {};
    }
    
    bool OcclusionCullingSystem::IsObjectVisible(uint32_t objectId)
    {
        if (!initialized)
        {
            return true; // Fallback to visible if not initialized
        }
        
        OcclusionQueryHandle* handle = FindQuery(objectId);
        if (!handle)
        {
            return true; // No query = visible
        }
        
        if (handle->isActive)
        {
            return true; // Query still in progress, assume visible
        }
        
        if (!handle->resultAvailable)
        {
            // Try to get the result
            glGetQueryObjectuiv(handle->queryId, GL_QUERY_RESULT_AVAILABLE, &handle->resultAvailable);
            
            if (handle->resultAvailable)
            {
                glGetQueryObjectui64v(handle->queryId, GL_QUERY_RESULT, &handle->result);
                
                if (handle->result > 0)
                {
                    stats.visibleObjects++;
                }
                else
                {
                    stats.occludedObjects++;
                }
            }
            else
            {
                return true; // Result not available yet, assume visible
            }
        }
        
        return handle->result > 0;
    }
    
    bool OcclusionCullingSystem::IsQueryResultAvailable(uint32_t objectId)
    {
        if (!initialized)
        {
            return true;
        }
        
        OcclusionQueryHandle* handle = FindQuery(objectId);
        if (!handle)
        {
            return true;
        }
        
        if (handle->isActive)
        {
            return false;
        }
        
        if (!handle->resultAvailable)
        {
            glGetQueryObjectuiv(handle->queryId, GL_QUERY_RESULT_AVAILABLE, &handle->resultAvailable);
        }
        
        return handle->resultAvailable;
    }
    
    void OcclusionCullingSystem::BeginFrame()
    {
        if (!initialized)
        {
            return;
        }
        
        frameInProgress = true;
        currentFrameQueries = 0;
        
        Logger::Log(LogLevel::Trace, "OcclusionCullingSystem::BeginFrame - Starting new frame");
    }
    
    void OcclusionCullingSystem::EndFrame()
    {
        if (!initialized || !frameInProgress)
        {
            return;
        }
        
        frameInProgress = false;
        
        // Calculate cache hit rate
        if (stats.totalQueries > 0)
        {
            stats.cacheHitRate = static_cast<float>(stats.cachedResults) / stats.totalQueries;
        }
        
        Logger::Log(LogLevel::Trace, "OcclusionCullingSystem::EndFrame - Frame complete, total queries: {}, occluded: {}", 
                    stats.totalQueries, stats.occludedObjects);
    }
    
    void OcclusionCullingSystem::ProcessPendingQueries()
    {
        if (!initialized)
        {
            return;
        }
        
        // Check for available results on pending queries
        for (auto& pair : queries)
        {
            OcclusionQueryHandle& handle = pair.second;
            
            if (!handle.isActive && !handle.resultAvailable)
            {
                glGetQueryObjectuiv(handle.queryId, GL_QUERY_RESULT_AVAILABLE, &handle.resultAvailable);
                
                if (handle.resultAvailable)
                {
                    glGetQueryObjectui64v(handle.queryId, GL_QUERY_RESULT, &handle.result);
                    
                    if (handle.result > 0)
                    {
                        stats.visibleObjects++;
                    }
                    else
                    {
                        stats.occludedObjects++;
                    }
                }
            }
        }
    }
    
    bool OcclusionCullingSystem::TestVisibility(uint32_t objectId, const BoundingVolume& volume, CommandList& cmdList)
    {
        if (!initialized)
        {
            return true;
        }
        
        // Check if we should skip this object (hierarchical culling)
        if (!ShouldTestObject(objectId, volume))
        {
            stats.cachedResults++;
            return true; // Assume visible for cached results
        }
        
        // Begin occlusion query
        auto result = BeginOcclusionQuery(objectId, volume);
        if (!result)
        {
            return true; // Fallback to visible if query fails
        }
        
        // Render bounding volume (simplified representation)
        RenderBoundingVolume(volume, cmdList);
        
        // End occlusion query
        EndOcclusionQuery(objectId);
        
        // For now, return true (result will be available next frame)
        // In a production system, you'd use previous frame's results
        return true;
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    OcclusionQueryHandle* OcclusionCullingSystem::FindQuery(uint32_t objectId)
    {
        auto it = queries.find(objectId);
        return (it != queries.end()) ? &it->second : nullptr;
    }
    
    Result<void> OcclusionCullingSystem::CreateQuery(OcclusionQueryHandle& handle)
    {
        glGenQueries(1, &handle.queryId);
        
        if (handle.queryId == 0)
        {
            return MakeUnexpected(EngineErrorCode::GPUResourceCreationFailed, 
                                  "Failed to create occlusion query", 
                                  "OcclusionCullingSystem");
        }
        
        handle.isActive = false;
        handle.resultAvailable = false;
        handle.result = 0;
        
        return {};
    }
    
    void OcclusionCullingSystem::DestroyQuery(OcclusionQueryHandle& handle)
    {
        if (handle.queryId != 0)
        {
            glDeleteQueries(1, &handle.queryId);
            handle.queryId = 0;
        }
    }
    
    void OcclusionCullingSystem::IssueQuery(OcclusionQueryHandle& handle, const BoundingVolume& volume, CommandList& cmdList)
    {
        // This is handled in BeginOcclusionQuery/EndOcclusionQuery
        // This method is for future extension with more complex query issuing
    }
    
    bool OcclusionCullingSystem::ShouldTestObject(uint32_t objectId, const BoundingVolume& volume)
    {
        // Hierarchical culling: skip testing if object is definitely visible
        // This is a simplified version - a full implementation would use spatial partitioning
        
        if (!hierarchicalCulling)
        {
            return true;
        }
        
        // For now, always test (can be enhanced with spatial hierarchies)
        return true;
    }
    
    void OcclusionCullingSystem::RenderBoundingVolume(const BoundingVolume& volume, CommandList& cmdList)
    {
        // Simplified bounding volume rendering for occlusion test
        // In a production system, this would render the actual bounding geometry
        
        switch (volume.type)
        {
            case BoundingVolumeType::Sphere:
            {
                // Render a simplified sphere representation
                // This would use a pre-allocated low-poly sphere mesh
                break;
            }
            case BoundingVolumeType::AABB:
            {
                // Render a simplified box representation
                // This would use a pre-allocated unit cube mesh scaled to extents
                break;
            }
            case BoundingVolumeType::OBB:
            {
                // Render oriented box representation
                break;
            }
        }
        
        // Note: Actual implementation would use CommandList methods to draw the geometry
        // For now, this is a placeholder for the rendering logic
    }
    
} // namespace Engine