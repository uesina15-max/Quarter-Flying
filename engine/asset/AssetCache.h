#pragma once

#include "../core/UUID.h"
#include "../core/EngineError.h"
#include "AssetImporter.h"

namespace Engine
{
    // ========================================
    // Asset Cache Interface
    // ========================================

    class AssetCache
    {
    public:
        virtual ~AssetCache() = default;

        virtual Result<ImportedAssetData> LoadFromCache(const UUID& uuid) = 0;
        virtual void SaveToCache(const UUID& uuid, const ImportedAssetData& data) = 0;
    };
} // namespace Engine
