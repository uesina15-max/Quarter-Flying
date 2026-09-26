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

    // ============================================================================
    // LOD Level Definition
    // ============================================================================
    enum class LODLevel : uint32_t
    {
        LOD0 = 0,  // Highest detail (closest)
        LOD1 = 1,  // Medium detail
        LOD2 = 2,  // Low detail
        LOD3 = 3,  // Lowest detail (farthest)
        Count = 4
    };

    // ============================================================================
    // LOD Transition Configuration
    // ============================================================================
    struct LODTransition
    {
        float nearDistance;    // Distance to switch to higher detail
        float farDistance;     // Distance to switch to lower detail
        float hysteresis;      // Hysteresis to prevent flickering
        
        LODTransition() : nearDistance(0.0f), farDistance(0.0f), hysteresis(0.1f) {}
        LODTransition(float near, float far, float hyst = 0.1f) 
            : nearDistance(near), farDistance(far), hysteresis(hyst) {}
    };

    // ============================================================================
    // LOD Configuration for a Mesh
    // ============================================================================
    struct LODConfig
    {
        uint64_t meshGuid;
        std::vector<LODTransition> transitions;
        std::vector<std::shared_ptr<Mesh>> lodMeshes;
        
        LODConfig() : meshGuid(0) {}
        LODConfig(uint64_t guid) : meshGuid(guid) {}
        
        bool IsValid() const 
        { 
            return meshGuid != 0 && 
                   !transitions.empty() && 
                   transitions.size() == lodMeshes.size() &&
                   lodMeshes.size() == static_cast<size_t>(LODLevel::Count);
        }
    };

    // ============================================================================
    // LOD Instance Data
    // ============================================================================
    struct LODInstance
    {
        uint32_t entityId;
        uint64_t meshGuid;
        LODLevel currentLOD;
        LODLevel previousLOD;
        glm::vec3 position;
        float screenSpaceSize;
        
        LODInstance() : entityId(0), meshGuid(0), currentLOD(LODLevel::LOD0), 
                       previousLOD(LODLevel::LOD0), screenSpaceSize(0.0f) {}
    };

    // ============================================================================
    // LOD System Statistics
    // ============================================================================
    struct LODStats
    {
        uint32_t totalInstances{0};
        uint32_t lod0Count{0};
        uint32_t lod1Count{0};
        uint32_t lod2Count{0};
        uint32_t lod3Count{0};
        uint32_t lodTransitions{0};
        float lodUpdateTimeMs{0.0f};
        
        void Reset()
        {
            totalInstances = 0;
            lod0Count = 0;
            lod1Count = 0;
            lod2Count = 0;
            lod3Count = 0;
            lodTransitions = 0;
            lodUpdateTimeMs = 0.0f;
        }
        
        float GetAverageLOD() const
        {
            if (totalInstances == 0) return 0.0f;
            return (lod0Count * 0.0f + lod1Count * 1.0f + lod2Count * 2.0f + lod3Count * 3.0f) / totalInstances;
        }
    };

    // ============================================================================
    // LOD System
    // ============================================================================
    class LODSystem
    {
    public:
        LODSystem();
        ~LODSystem();

        // Initialization
        Result<void> Initialize();
        void Shutdown();

        // LOD Configuration
        Result<void> RegisterLODConfig(const LODConfig& config);
        Result<void> UnregisterLODConfig(uint64_t meshGuid);
        const LODConfig* GetLODConfig(uint64_t meshGuid) const;

        // Instance Management
        Result<void> AddInstance(uint32_t entityId, uint64_t meshGuid, const glm::vec3& position);
        Result<void> RemoveInstance(uint32_t entityId);
        Result<void> UpdateInstancePosition(uint32_t entityId, const glm::vec3& position);

        // LOD Update
        void UpdateLODs(const Camera& camera);
        void ForceLODUpdate(uint32_t entityId, LODLevel lod);

        // Mesh Access
        std::shared_ptr<Mesh> GetLODMesh(uint64_t meshGuid, LODLevel lod) const;
        LODLevel GetCurrentLOD(uint32_t entityId) const;

        // Statistics
        const LODStats& GetStats() const { return stats; }
        void ResetStats() { stats.Reset(); }

        // Configuration
        void SetGlobalLODBias(float bias) { lodBias = bias; }
        float GetGlobalLODBias() const { return lodBias; }
        
        void SetScreenSpaceThreshold(float threshold) { screenSpaceThreshold = threshold; }
        float GetScreenSpaceThreshold() const { return screenSpaceThreshold; }

    private:
        // Internal helpers
        LODLevel CalculateLOD(const glm::vec3& position, const Camera& camera, const LODConfig& config);
        float CalculateScreenSpaceSize(const glm::vec3& position, const Camera& camera, float boundingRadius);
        float CalculateDistance(const glm::vec3& position, const Camera& camera);
        
        void UpdateLODInstance(LODInstance& instance, const Camera& camera);
        LODConfig* FindLODConfig(uint64_t meshGuid);
        LODInstance* FindInstance(uint32_t entityId);
        const LODInstance* FindInstance(uint32_t entityId) const;

    private:
        std::unordered_map<uint64_t, LODConfig> lodConfigs;
        std::unordered_map<uint32_t, LODInstance> instances;
        
        LODStats stats;
        
        // Global LOD settings
        float lodBias;              // Bias for LOD selection (0.0 = default, >0 = higher detail, <0 = lower detail)
        float screenSpaceThreshold; // Minimum screen space size for LOD0
        
        bool initialized;
    };

} // namespace Engine