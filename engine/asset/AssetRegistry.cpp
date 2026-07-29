#include "AssetRegistry.h"
#include "../core/logging/Logger.h"

namespace Engine
{
    AssetRegistry::AssetRegistry()
        : nextId(1)
    {
        ENGINE_LOG_INFO("AssetRegistry initialized");
    }

    AssetRegistry::~AssetRegistry()
    {
        Clear();
        ENGINE_LOG_INFO("AssetRegistry destroyed");
    }

    Result<void> AssetRegistry::RegisterAsset(const AssetMetadata& metadata)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        // 중복 경로 체크
        if (!metadata.sourcePath.empty())
        {
            auto it = pathIndex.find(metadata.sourcePath);
            if (it != pathIndex.end())
            {
                return EngineError::MakeError(
                    EngineErrorCode::AssetAlreadyRegistered,
                    "Asset already registered with path: " + metadata.sourcePath,
                    "AssetRegistry"
                );
            }
        }

        // 중복 UUID 체크
        auto uuidIt = uuidIndex.find(metadata.uuid);
        if (uuidIt != uuidIndex.end())
        {
            return EngineError::MakeError(
                EngineErrorCode::AssetAlreadyRegistered,
                "Asset already registered with UUID",
                "AssetRegistry"
            );
        }

        // 핸들 생성
        AssetHandle handle = CreateHandle();

        // 메타데이터 저장
        metadataMap[handle] = metadata;

        // 인덱스 등록
        if (!metadata.sourcePath.empty())
        {
            pathIndex[metadata.sourcePath] = handle;
        }

        uuidIndex[metadata.uuid] = handle;
        typeIndex[metadata.type].push_back(handle);

        ENGINE_LOG_INFO("Asset registered: {} (UUID: {}, Type: {})",
            metadata.name, metadata.uuid.ToString(), static_cast<uint32_t>(metadata.type));

        return {};
    }

    void AssetRegistry::UnregisterAsset(AssetHandle handle)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        auto it = metadataMap.find(handle);
        if (it == metadataMap.end())
        {
            return;
        }

        const AssetMetadata& metadata = it->second;

        // 인덱스에서 제거
        if (!metadata.sourcePath.empty())
        {
            pathIndex.erase(metadata.sourcePath);
        }

        uuidIndex.erase(metadata.uuid);

        auto typeIt = typeIndex.find(metadata.type);
        if (typeIt != typeIndex.end())
        {
            auto& handles = typeIt->second;
            handles.erase(std::remove(handles.begin(), handles.end(), handle), handles.end());
            
            if (handles.empty())
            {
                typeIndex.erase(typeIt);
            }
        }

        // 메타데이터 제거
        metadataMap.erase(it);

        ENGINE_LOG_INFO("Asset unregistered: {}", metadata.name);
    }

    Result<AssetHandle> AssetRegistry::FindAssetByPath(const std::string& path) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        auto it = pathIndex.find(path);
        if (it != pathIndex.end())
        {
            return it->second;
        }

        return EngineError::MakeError(
            EngineErrorCode::AssetNotFound,
            "Asset not found with path: " + path,
            "AssetRegistry"
        );
    }

    Result<AssetHandle> AssetRegistry::FindAssetByUUID(const UUID& uuid) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        auto it = uuidIndex.find(uuid);
        if (it != uuidIndex.end())
        {
            return it->second;
        }

        return EngineError::MakeError(
            EngineErrorCode::AssetNotFound,
            "Asset not found with UUID",
            "AssetRegistry"
        );
    }

    std::vector<AssetHandle> AssetRegistry::FindAssetsByType(AssetType type) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        auto it = typeIndex.find(type);
        if (it != typeIndex.end())
        {
            return it->second;
        }

        return {};
    }

    Result<const AssetMetadata*> AssetRegistry::GetMetadata(AssetHandle handle) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        auto it = metadataMap.find(handle);
        if (it != metadataMap.end())
        {
            return &it->second;
        }

        return EngineError::MakeError(
            EngineErrorCode::AssetNotFound,
            "Asset metadata not found",
            "AssetRegistry"
        );
    }

    Result<AssetMetadata*> AssetRegistry::GetMetadata(AssetHandle handle)
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        auto it = metadataMap.find(handle);
        if (it != metadataMap.end())
        {
            return &it->second;
        }

        return EngineError::MakeError(
            EngineErrorCode::AssetNotFound,
            "Asset metadata not found",
            "AssetRegistry"
        );
    }

    Result<void> AssetRegistry::UpdateMetadata(AssetHandle handle, const AssetMetadata& metadata)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        auto it = metadataMap.find(handle);
        if (it == metadataMap.end())
        {
            return EngineError::MakeError(
                EngineErrorCode::AssetNotFound,
                "Asset not found for metadata update",
                "AssetRegistry"
            );
        }

        // 경로가 변경된 경우 인덱스 업데이트
        if (it->second.sourcePath != metadata.sourcePath)
        {
            if (!it->second.sourcePath.empty())
            {
                pathIndex.erase(it->second.sourcePath);
            }

            if (!metadata.sourcePath.empty())
            {
                auto pathIt = pathIndex.find(metadata.sourcePath);
                if (pathIt != pathIndex.end() && pathIt->second != handle)
                {
                    return EngineError::MakeError(
                        EngineErrorCode::AssetAlreadyRegistered,
                        "Target path already registered to another asset",
                        "AssetRegistry"
                    );
                }

                pathIndex[metadata.sourcePath] = handle;
            }
        }

        // UUID가 변경된 경우 인덱스 업데이트
        if (it->second.uuid != metadata.uuid)
        {
            uuidIndex.erase(it->second.uuid);

            auto uuidIt = uuidIndex.find(metadata.uuid);
            if (uuidIt != uuidIndex.end() && uuidIt->second != handle)
            {
                return EngineError::MakeError(
                    EngineErrorCode::AssetAlreadyRegistered,
                    "Target UUID already registered to another asset",
                    "AssetRegistry"
                );
            }

            uuidIndex[metadata.uuid] = handle;
        }

        // 타입이 변경된 경우 인덱스 업데이트
        if (it->second.type != metadata.type)
        {
            auto oldTypeIt = typeIndex.find(it->second.type);
            if (oldTypeIt != typeIndex.end())
            {
                auto& handles = oldTypeIt->second;
                handles.erase(std::remove(handles.begin(), handles.end(), handle), handles.end());
                
                if (handles.empty())
                {
                    typeIndex.erase(oldTypeIt);
                }
            }

            typeIndex[metadata.type].push_back(handle);
        }

        // 메타데이터 업데이트
        it->second = metadata;

        ENGINE_LOG_INFO("Asset metadata updated: {}", metadata.name);

        return {};
    }

    std::vector<AssetHandle> AssetRegistry::GetAllAssets() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        std::vector<AssetHandle> handles;
        handles.reserve(metadataMap.size());

        for (const auto& pair : metadataMap)
        {
            handles.push_back(pair.first);
        }

        return handles;
    }

    size_t AssetRegistry::GetAssetCount() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return metadataMap.size();
    }

    void AssetRegistry::Clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);

        size_t count = metadataMap.size();
        metadataMap.clear();
        pathIndex.clear();
        uuidIndex.clear();
        typeIndex.clear();
        nextId = 1;

        ENGINE_LOG_INFO("AssetRegistry cleared ({} assets removed)", count);
    }

    AssetHandle AssetRegistry::CreateHandle()
    {
        return AssetHandle(nextId++, 0);
    }

    bool AssetRegistry::IsValidHandle(AssetHandle handle) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return metadataMap.find(handle) != metadataMap.end();
    }

} // namespace Engine
