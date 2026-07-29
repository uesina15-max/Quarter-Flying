#pragma once

#include "AssetHandle.h"
#include "../core/EngineError.h"
#include <string>
#include <memory>
#include <vector>

namespace Engine
{
    // ========================================
    // Forward Declarations
    // ========================================

    struct ImportedAssetData;

    // ========================================
    // Asset Importer Interface
    // ========================================

    class AssetImporter
    {
    public:
        virtual ~AssetImporter() = default;

        // 지원 파일 확장자 확인
        virtual bool CanImport(const std::string& extension) const = 0;

        // 임포트 실행 (소스 → 런타임 데이터)
        virtual Result<ImportedAssetData> Import(
            const std::string& sourcePath,
            const ImportOptions& options
        ) = 0;

        // 메타데이터 추출 (소스 파일에서)
        virtual Result<AssetMetadata> ExtractMetadata(
            const std::string& sourcePath
        ) = 0;

        // 임포터 이름 (디버깅용)
        virtual std::string GetName() const = 0;
    };

} // namespace Engine
