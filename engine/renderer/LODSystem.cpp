#include "LODSystem.h"
#include "Mesh.h"
#include "Camera.h"
#include "../core/logging/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>

namespace Engine
{
    // ============================================================================
    // LODSystem Implementation
    // ============================================================================
    
    LODSystem::LODSystem()
        : lodBias(0.0f)
        , screenSpaceThreshold(50.0f)  // 50 pixels minimum for LOD0
        , initialized(false)
    {
    }
    
    LODSystem::~LODSystem()
    {
        Shutdown();
    }
    
    Result<void> LODSystem::Initialize()
    {
        if (initialized)
        {
            Logger::Log(LogLevel::Warning, "LODSystem::Initialize - Already initialized");
            return {};
        }
        
        Logger::Log(LogLevel::Info, "LODSystem::Initialize - Initializing LOD system");
        
        // Initialize default LOD transitions if needed
        // These can be overridden per-mesh via RegisterLODConfig
        
        initialized = true;
        Logger::Log(LogLevel::Info, "LODSystem::Initialize - LOD system initialized successfully");
        return {};
    }
    
    void LODSystem::Shutdown()
    {
        if (!initialized)
        {
            return;
        }
        
        Logger::Log(LogLevel::Info, "LODSystem::Shutdown - Shutting down LOD system");
        
        lodConfigs.clear();
        instances.clear();
        
        initialized = false;
        Logger::Log(LogLevel::Info, "LODSystem::Shutdown - Shutdown complete");
    }
    
    Result<void> LODSystem::RegisterLODConfig(const LODConfig& config)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "LOD system not initialized", "LODSystem");
        }
        
        if (!config.IsValid())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, "Invalid LOD configuration", "LODSystem");
        }
        
        lodConfigs[config.meshGuid] = config;
        
        Logger::Log(LogLevel::Info, "LODSystem::RegisterLODConfig - Registered LOD config for mesh GUID: {}", 
                    config.meshGuid);
        
        return {};
    }
    
    Result<void> LODSystem::UnregisterLODConfig(uint64_t meshGuid)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "LOD system not initialized", "LODSystem");
        }
        
        auto it = lodConfigs.find(meshGuid);
        if (it == lodConfigs.end())
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "LOD config not found", "LODSystem");
        }
        
        lodConfigs.erase(it);
        
        Logger::Log(LogLevel::Info, "LODSystem::UnregisterLODConfig - Unregistered LOD config for mesh GUID: {}", 
                    meshGuid);
        
        return {};
    }
    
    const LODConfig* LODSystem::GetLODConfig(uint64_t meshGuid) const
    {
        auto it = lodConfigs.find(meshGuid);
        return (it != lodConfigs.end()) ? &it->second : nullptr;
    }
    
    Result<void> LODSystem::AddInstance(uint32_t entityId, uint64_t meshGuid, const glm::vec3& position)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "LOD system not initialized", "LODSystem");
        }
        
        // Check if LOD config exists for this mesh
        if (lodConfigs.find(meshGuid) == lodConfigs.end())
        {
            Logger::Log(LogLevel::Warning, "LODSystem::AddInstance - No LOD config for mesh GUID: {}, using default LOD0", 
                        meshGuid);
        }
        
        LODInstance instance;
        instance.entityId = entityId;
        instance.meshGuid = meshGuid;
        instance.position = position;
        instance.currentLOD = LODLevel::LOD0;
        instance.previousLOD = LODLevel::LOD0;
        
        instances[entityId] = instance;
        
        Logger::Log(LogLevel::Trace, "LODSystem::AddInstance - Added instance for entity ID: {}, mesh GUID: {}", 
                    entityId, meshGuid);
        
        return {};
    }
    
    Result<void> LODSystem::RemoveInstance(uint32_t entityId)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "LOD system not initialized", "LODSystem");
        }
        
        auto it = instances.find(entityId);
        if (it == instances.end())
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "Instance not found", "LODSystem");
        }
        
        instances.erase(it);
        
        Logger::Log(LogLevel::Trace, "LODSystem::RemoveInstance - Removed instance for entity ID: {}", 
                    entityId);
        
        return {};
    }
    
    Result<void> LODSystem::UpdateInstancePosition(uint32_t entityId, const glm::vec3& position)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "LOD system not initialized", "LODSystem");
        }
        
        LODInstance* instance = FindInstance(entityId);
        if (!instance)
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "Instance not found", "LODSystem");
        }
        
        instance->position = position;
        
        return {};
    }
    
    void LODSystem::UpdateLODs(const Camera& camera)
    {
        if (!initialized)
        {
            return;
        }
        
        // Reset statistics
        stats.Reset();
        stats.totalInstances = instances.size();
        
        // Update LOD for each instance
        for (auto& pair : instances)
        {
            UpdateLODInstance(pair.second, camera);
        }
        
        Logger::Log(LogLevel::Trace, "LODSystem::UpdateLODs - Updated {} instances, LOD0: {}, LOD1: {}, LOD2: {}, LOD3: {}", 
                    stats.totalInstances, stats.lod0Count, stats.lod1Count, stats.lod2Count, stats.lod3Count);
    }
    
    void LODSystem::ForceLODUpdate(uint32_t entityId, LODLevel lod)
    {
        LODInstance* instance = FindInstance(entityId);
        if (!instance)
        {
            Logger::Log(LogLevel::Warning, "LODSystem::ForceLODUpdate - Instance not found for entity ID: {}", 
                        entityId);
            return;
        }
        
        instance->previousLOD = instance->currentLOD;
        instance->currentLOD = lod;
        stats.lodTransitions++;
        
        Logger::Log(LogLevel::Trace, "LODSystem::ForceLODUpdate - Forced LOD {} for entity ID: {}", 
                    static_cast<uint32_t>(lod), entityId);
    }
    
    std::shared_ptr<Mesh> LODSystem::GetLODMesh(uint64_t meshGuid, LODLevel lod) const
    {
        const LODConfig* config = GetLODConfig(meshGuid);
        if (!config)
        {
            Logger::Log(LogLevel::Warning, "LODSystem::GetLODMesh - No LOD config for mesh GUID: {}", 
                        meshGuid);
            return nullptr;
        }
        
        uint32_t lodIndex = static_cast<uint32_t>(lod);
        if (lodIndex >= config->lodMeshes.size())
        {
            Logger::Log(LogLevel::Warning, "LODSystem::GetLODMesh - Invalid LOD level {} for mesh GUID: {}", 
                        lodIndex, meshGuid);
            return nullptr;
        }
        
        return config->lodMeshes[lodIndex];
    }
    
    LODLevel LODSystem::GetCurrentLOD(uint32_t entityId) const
    {
        const LODInstance* instance = FindInstance(entityId);
        return instance ? instance->currentLOD : LODLevel::LOD0;
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    LODLevel LODSystem::CalculateLOD(const glm::vec3& position, const Camera& camera, const LODConfig& config)
    {
        float distance = CalculateDistance(position, camera);
        
        // Apply LOD bias
        distance += lodBias * distance;
        
        // Find appropriate LOD level based on distance
        for (size_t i = 0; i < config.transitions.size(); ++i)
        {
            const LODTransition& transition = config.transitions[i];
            
            if (distance >= transition.nearDistance && distance < transition.farDistance)
            {
                return static_cast<LODLevel>(i);
            }
        }
        
        // If beyond all transitions, use lowest LOD
        if (!config.transitions.empty())
        {
            return static_cast<LODLevel>(config.transitions.size() - 1);
        }
        
        return LODLevel::LOD0;
    }
    
    float LODSystem::CalculateScreenSpaceSize(const glm::vec3& position, const Camera& camera, float boundingRadius)
    {
        // Calculate screen space size based on distance and projection
        float distance = CalculateDistance(position, camera);
        if (distance < 0.1f) return 10000.0f; // Very close, max size
        
        // Approximate screen space size (this is a simplified calculation)
        // For accurate results, you'd need the actual projection matrix and viewport
        float screenSize = (boundingRadius / distance) * 1000.0f;
        
        return screenSize;
    }
    
    float LODSystem::CalculateDistance(const glm::vec3& position, const Camera& camera)
    {
        glm::vec3 cameraPos = camera.getPosition();
        return glm::length(position - cameraPos);
    }
    
    void LODSystem::UpdateLODInstance(LODInstance& instance, const Camera& camera)
    {
        LODConfig* config = FindLODConfig(instance.meshGuid);
        
        if (!config)
        {
            // No config, keep at LOD0
            instance.currentLOD = LODLevel::LOD0;
            stats.lod0Count++;
            return;
        }
        
        LODLevel newLOD = CalculateLOD(instance.position, camera, *config);
        
        // Apply hysteresis to prevent flickering
        if (newLOD != instance.currentLOD)
        {
            uint32_t currentIdx = static_cast<uint32_t>(instance.currentLOD);
            uint32_t newIdx = static_cast<uint32_t>(newLOD);
            
            if (newIdx < config->transitions.size())
            {
                const LODTransition& transition = config->transitions[newIdx];
                float distance = CalculateDistance(instance.position, camera);
                
                // Check hysteresis
                if (newLOD > instance.currentLOD)
                {
                    // Moving to lower detail (farther)
                    if (distance < transition.farDistance - transition.hysteresis)
                    {
                        newLOD = instance.currentLOD; // Keep current LOD
                    }
                }
                else
                {
                    // Moving to higher detail (closer)
                    if (distance > transition.nearDistance + transition.hysteresis)
                    {
                        newLOD = instance.currentLOD; // Keep current LOD
                    }
                }
            }
        }
        
        // Update LOD if changed
        if (newLOD != instance.currentLOD)
        {
            instance.previousLOD = instance.currentLOD;
            instance.currentLOD = newLOD;
            stats.lodTransitions++;
        }
        
        // Update statistics
        switch (instance.currentLOD)
        {
            case LODLevel::LOD0: stats.lod0Count++; break;
            case LODLevel::LOD1: stats.lod1Count++; break;
            case LODLevel::LOD2: stats.lod2Count++; break;
            case LODLevel::LOD3: stats.lod3Count++; break;
        }
    }
    
    LODConfig* LODSystem::FindLODConfig(uint64_t meshGuid)
    {
        auto it = lodConfigs.find(meshGuid);
        return (it != lodConfigs.end()) ? &it->second : nullptr;
    }
    
    LODInstance* LODSystem::FindInstance(uint32_t entityId)
    {
        auto it = instances.find(entityId);
        return (it != instances.end()) ? &it->second : nullptr;
    }

    const LODInstance* LODSystem::FindInstance(uint32_t entityId) const
    {
        auto it = instances.find(entityId);
        return (it != instances.end()) ? &it->second : nullptr;
    }

} // namespace Engine