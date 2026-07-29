#pragma once

#include "../core/Types.h"
#include "../core/UUID.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Engine
{
    // ========================================
    // Asset Handle Type
    // ========================================

    struct AssetHandleTag {};
    using AssetHandle = Handle<AssetHandleTag>;

    // ========================================
    // Asset Type Enumeration
    // ========================================

    enum class AssetType : uint32_t
    {
        Unknown = 0,
        Mesh,           // 3D mesh (FBX, glTF)
        Material,       // Material
        Texture,        // Texture (PNG, JPG, TGA)
        Shader,         // Shader (HLSL, GLSL)
        Animation,      // Animation clip
        Audio,          // Audio (WAV, OGG)
        Scene,          // Scene file
        Prefab,         // Prefab
        Script,         // Script
    };

    // ========================================
    // Asset Import Options
    // ========================================

    struct ImportOptions
    {
        bool generateNormals = true;
        bool generateTangents = true;
        bool calculateBoundingBox = true;
        bool flipUVs = false;
        float scale = 1.0f;
        
        // Texture related
        bool generateMipmaps = true;
        bool compress = true;
        uint32_t maxTextureSize = 4096;
        
        // Other
        bool importAnimations = true;
        bool importMaterials = true;
    };

    // ========================================
    // Asset Metadata
    // ========================================

    struct AssetMetadata
    {
        UUID uuid;                      // Asset unique identifier
        AssetType type;                 // Asset type
        std::string sourcePath;         // Source file path
        std::string cachedPath;         // Cache file path
        std::string name;               // Asset name
        
        // Import info
        ImportOptions importOptions;    // Import options
        uint64_t sourceHash;            // Source file hash (change detection)
        uint64_t importTime;            // Import time (Unix timestamp)
        
        // Reference info
        std::vector<UUID> dependencies; // Dependent asset UUID list
        std::vector<UUID> dependents;   // Assets referencing this asset UUID list
        
        // Runtime info
        uint32_t referenceCount;       // Reference count
        bool isLoaded;                  // Loading complete
        
        // Custom data (JSON serialization)
        nlohmann::json customData;
        
        AssetMetadata()
            : type(AssetType::Unknown)
            , sourceHash(0)
            , importTime(0)
            , referenceCount(0)
            , isLoaded(false)
        {}
    };

    // ========================================
    // Imported Asset Data
    // ========================================

    struct ImportedAssetData
    {
        AssetType type;
        std::vector<uint8_t> rawData;  // Imported raw data
        nlohmann::json metadata;       // Additional metadata
        
        // Type-specific data pointer (runtime casting)
        std::shared_ptr<void> runtimeData;
    };

} // namespace Engine
