#include "AssetManager.h"
#include "AssetCache.h"
#include "AssetImporter.h"
#include "importers/TextureImporter.h"
#include "../core/logging/Logger.h"
#include <fstream>
#include <filesystem>

namespace Engine
{
    AssetManager::AssetManager()
        : initialized(false)
    {
        ENGINE_LOG_INFO("AssetManager created");
    }

    AssetManager::~AssetManager()
    {
        if (initialized)
        {
            Shutdown();
        }
    }

    Result<void> AssetManager::Initialize()
    {
        ENGINE_LOG_INFO("AssetManager initializing...");

        // Register default importers
        RegisterImporter(std::make_unique<TextureImporter>());
        // RegisterImporter(std::make_unique<FBXImporter>()); // Future implementation

        initialized = true;
        ENGINE_LOG_INFO("AssetManager initialized successfully");
        return {};
    }

    void AssetManager::Shutdown() noexcept
    {
        ENGINE_LOG_INFO("AssetManager shutting down...");

        // Unload all assets
        std::vector<AssetHandle> allAssets = registry.GetAllAssets();
        for (AssetHandle handle : allAssets)
        {
            UnloadAsset(handle);
        }

        // Clean up importers
        importers.clear();

        // Clean up cache
        assetCache.reset();

        registry.Clear();

        initialized = false;
        ENGINE_LOG_INFO("AssetManager shutdown complete");
    }

    void AssetManager::Tick(float deltaTime)
    {
        // Async loading progress check etc.
        // Future implementation
    }

    Result<AssetHandle> AssetManager::LoadAsset(const std::string& path)
    {
        if (!initialized)
        {
            return MakeError<AssetHandle>(
                EngineErrorCode::NotInitialized,
                "AssetManager not initialized",
                "AssetManager"
            );
        }

        // Path normalization
        std::filesystem::path fsPath(path);
        std::string normalizedPath = fsPath.lexically_normal().string();

        // Check if already loaded in registry
        auto existingHandle = registry.FindAssetByPath(normalizedPath);
        if (existingHandle.has_value())
        {
            // Increment reference count
            auto metadataResult = registry.GetMetadata(existingHandle.value());
            if (metadataResult.has_value())
            {
                AssetMetadata* metadata = metadataResult.value();
                metadata->referenceCount++;
                ENGINE_LOG_INFO("Asset loaded from cache: {} (RefCount: {})", 
                    normalizedPath, metadata->referenceCount);
                return existingHandle.value();
            }
        }

        // New load
        return LoadAssetInternal(normalizedPath);
    }

    std::future<Result<AssetHandle>> AssetManager::LoadAssetAsync(
        const std::string& path,
        std::function<void(Result<AssetHandle>)> callback
    )
    {
        // Async loading implementation (future)
        // Currently sync loading as placeholder
        auto result = LoadAsset(path);
        
        if (callback)
        {
            callback(result);
        }

        std::promise<Result<AssetHandle>> promise;
        promise.set_value(result);
        return promise.get_future();
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        std::unique_lock<std::shared_mutex> lock(dataMutex);

        auto metadataResult = registry.GetMetadata(handle);
        if (!metadataResult.has_value())
        {
            return;
        }

        AssetMetadata* metadata = metadataResult.value();
        metadata->referenceCount--;

        ENGINE_LOG_INFO("Asset unloaded: {} (RefCount: {})", 
            metadata->name, metadata->referenceCount);

        // Free data if reference count is 0
        if (metadata->referenceCount <= 0)
        {
            assetDataMap.erase(handle);
            metadata->isLoaded = false;
            
            ENGINE_LOG_INFO("Asset data freed: {}", metadata->name);
        }
    }

    Result<AssetHandle> AssetManager::ImportAsset(
        const std::string& sourcePath,
        const ImportOptions& options
    )
    {
        if (!initialized)
        {
            return MakeError<AssetHandle>(
                EngineErrorCode::NotInitialized,
                "AssetManager not initialized",
                "AssetManager"
            );
        }

        // File existence check
        if (!std::filesystem::exists(sourcePath))
        {
            return MakeError<AssetHandle>(
                EngineErrorCode::FileNotFound,
                "Source file not found: " + sourcePath,
                "AssetManager"
            );
        }

        // Extract extension
        std::filesystem::path fsPath(sourcePath);
        std::string extension = fsPath.extension().string();

        // Find appropriate importer
        AssetImporter* importer = FindImporter(extension);
        if (!importer)
        {
            return MakeError<AssetHandle>(
                EngineErrorCode::UnsupportedAssetType,
                "No importer found for extension: " + extension,
                "AssetManager"
            );
        }

        std::lock_guard<std::mutex> lock(importMutex);

        // Calculate source hash
        uint64_t sourceHash = CalculateSourceHash(sourcePath);

        // Extract metadata
        auto metadataResult = importer->ExtractMetadata(sourcePath);
        if (!metadataResult.has_value())
        {
            return std::unexpected(metadataResult.error());
        }

        AssetMetadata metadata = metadataResult.value();
        metadata.sourcePath = sourcePath;
        metadata.importOptions = options;
        metadata.sourceHash = sourceHash;
        metadata.importTime = std::chrono::system_clock::now().time_since_epoch().count();

        // Execute import
        auto importResult = importer->Import(sourcePath, options);
        if (!importResult.has_value())
        {
            return std::unexpected(importResult.error());
        }

        ImportedAssetData importedData = importResult.value();

        // Save to cache
        SaveToCache(metadata.uuid, importedData);

        // Register in registry
        auto registerResult = registry.RegisterAsset(metadata);
        if (!registerResult.has_value())
        {
            return std::unexpected(registerResult.error());
        }

        // Store asset data
        AssetHandle handle = registry.FindAssetByUUID(metadata.uuid).value();
        
        {
            std::unique_lock<std::shared_mutex> dataLock(dataMutex);
            assetDataMap[handle] = importedData.runtimeData;
            metadata.isLoaded = true;
            metadata.referenceCount = 1;
        }

        ENGINE_LOG_INFO("Asset imported successfully: {} (UUID: {})", 
            metadata.name, metadata.uuid.ToString());

        return handle;
    }

    Result<AssetHandle> AssetManager::ReimportAsset(AssetHandle handle)
    {
        auto metadataResult = registry.GetMetadata(handle);
        if (!metadataResult.has_value())
        {
            return std::unexpected(metadataResult.error());
        }

        const AssetMetadata* metadata = metadataResult.value();

        // Unload existing data
        UnloadAsset(handle);

        // Invalidate cache
        if (assetCache)
        {
            // assetCache->InvalidateCache(metadata->uuid);
        }

        // Reimport
        return ImportAsset(metadata->sourcePath, metadata->importOptions);
    }

    Result<const AssetMetadata*> AssetManager::GetMetadata(AssetHandle handle) const
    {
        return registry.GetMetadata(handle);
    }

    Result<void> AssetManager::SetMetadata(AssetHandle handle, const AssetMetadata& metadata)
    {
        return registry.UpdateMetadata(handle, metadata);
    }

    void AssetManager::RegisterImporter(std::unique_ptr<AssetImporter> importer)
    {
        if (importer)
        {
            importers.push_back(std::move(importer));
            ENGINE_LOG_INFO("Asset importer registered");
        }
    }

    void AssetManager::SetAssetCache(std::unique_ptr<AssetCache> cache)
    {
        assetCache = std::move(cache);
        ENGINE_LOG_INFO("Asset cache set");
    }

    Result<AssetHandle> AssetManager::LoadAssetInternal(const std::string& path)
    {
        // File existence check
        if (!std::filesystem::exists(path))
        {
            return MakeError<AssetHandle>(
                EngineErrorCode::FileNotFound,
                "Asset file not found: " + path,
                "AssetManager"
            );
        }

        // Auto import attempt
        return ImportAsset(path, ImportOptions{});
    }

    Result<ImportedAssetData> AssetManager::LoadFromCache(const UUID& uuid)
    {
        if (!assetCache)
        {
            return MakeError<ImportedAssetData>(
            EngineErrorCode::CacheNotAvailable,
            "Asset cache not available",
            "AssetManager"
        );
        }

        // return assetCache->LoadFromCache(uuid);
        
        return MakeError<ImportedAssetData>(
            EngineErrorCode::NotImplemented,
            "Cache loading not yet implemented",
            "AssetManager"
        );
    }

    void AssetManager::SaveToCache(const UUID& uuid, const ImportedAssetData& data)
    {
        if (!assetCache)
        {
            return;
        }

        // assetCache->SaveToCache(uuid, data);
    }

    AssetImporter* AssetManager::FindImporter(const std::string& extension)
    {
        for (auto& importer : importers)
        {
            if (importer->CanImport(extension))
            {
                return importer.get();
            }
        }
        return nullptr;
    }

    uint64_t AssetManager::CalculateSourceHash(const std::string& path) const
    {
        // Simple file hash calculation (modification time + file size)
        auto ftime = std::filesystem::last_write_time(path);
        uint64_t size = std::filesystem::file_size(path);
        
        // Convert modification time to timestamp
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        uint64_t timestamp = static_cast<uint64_t>(sctp.time_since_epoch().count());

        // Simple hash combination
        return timestamp ^ (size << 16);
    }

} // namespace Engine
