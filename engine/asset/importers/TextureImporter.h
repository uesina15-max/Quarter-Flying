#pragma once

#include "../AssetImporter.h"
#include "../AssetHandle.h"
#include "../../core/EngineError.h"
#include "../../core/Types.h"
#include <string>
#include <vector>

namespace Engine
{
    // ========================================
    // Texture Data Structure
    // ========================================

    struct TextureData
    {
        int width;
        int height;
        int channels;
        std::vector<uint8_t> pixels;
    };

    // ========================================
    // Texture Importer
    // ========================================

    class TextureImporter : public AssetImporter
    {
    public:
        TextureImporter();
        ~TextureImporter() override = default;

        // AssetImporter interface implementation
        bool CanImport(const std::string& extension) const override;
        Result<ImportedAssetData> Import(
            const std::string& sourcePath,
            const ImportOptions& options
        ) override;
        Result<AssetMetadata> ExtractMetadata(const std::string& sourcePath) override;
        std::string GetName() const override { return "TextureImporter"; }

    private:
        // Texture load using stb_image
        Result<TextureData> LoadTextureData(
            const std::string& path,
            const ImportOptions& options
        );

        // Texture format determination
        TextureFormat DetermineTextureFormat(int channels);
    };

} // namespace Engine
