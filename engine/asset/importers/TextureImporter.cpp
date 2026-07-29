#include "TextureImporter.h"
#include "../../core/logging/Logger.h"
#include <filesystem>
#include <algorithm>
#include <cstring>

// stb_image 구현 (단일 헤더 라이브러리)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Engine
{
    TextureImporter::TextureImporter()
    {
        ENGINE_LOG_INFO("TextureImporter created");
    }

    bool TextureImporter::CanImport(const std::string& extension) const
    {
        // 소문자로 변환하여 비교
        std::string ext = extension;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // 지원하는 텍스처 형식
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
               ext == ".tga" || ext == ".bmp" || ext == ".psd" ||
               ext == ".gif" || ext == ".hdr" || ext == ".pic" ||
               ext == ".pnm" || ext == ".webp";
    }

    Result<ImportedAssetData> TextureImporter::Import(
        const std::string& sourcePath,
        const ImportOptions& options
    )
    {
        ENGINE_LOG_INFO("Importing texture: {}", sourcePath);

        // 텍스처 데이터 로드
        auto textureDataResult = LoadTextureData(sourcePath, options);
        if (!textureDataResult.has_value())
        {
            return std::unexpected(textureDataResult.error());
        }

        TextureData textureData = textureDataResult.value();

        // ImportedAssetData 생성
        ImportedAssetData importedData;
        importedData.type = AssetType::Texture;

        // 메타데이터에 텍스처 정보 저장
        importedData.metadata["width"] = textureData.width;
        importedData.metadata["height"] = textureData.height;
        importedData.metadata["channels"] = textureData.channels;
        importedData.metadata["format"] = static_cast<uint32_t>(DetermineTextureFormat(textureData.channels));

        // 원본 데이터 저장
        importedData.rawData = std::move(textureData.pixels);

        // 런타임 데이터 (추후 Texture 클래스로 대체)
        // 현재는 rawData만 사용
        importedData.runtimeData = nullptr;

        ENGINE_LOG_INFO("Texture imported successfully: {}x{} ({} channels)",
            textureData.width, textureData.height, textureData.channels);

        return importedData;
    }

    Result<AssetMetadata> TextureImporter::ExtractMetadata(const std::string& sourcePath)
    {
        AssetMetadata metadata;
        metadata.type = AssetType::Texture;
        metadata.uuid = UUID::Generate();

        // 파일 이름 추출
        std::filesystem::path fsPath(sourcePath);
        metadata.name = fsPath.stem().string();
        metadata.sourcePath = sourcePath;

        // 기본 메타데이터 설정
        metadata.importOptions = ImportOptions{};
        metadata.sourceHash = 0; // 추후 계산
        metadata.importTime = 0;
        metadata.referenceCount = 0;
        metadata.isLoaded = false;

        ENGINE_LOG_INFO("Texture metadata extracted: {}", metadata.name);

        return metadata;
    }

    Result<TextureData> TextureImporter::LoadTextureData(
        const std::string& path,
        const ImportOptions& options
    )
    {
        TextureData data;
        data.width = 0;
        data.height = 0;
        data.channels = 0;

        // stb_image 설정
        stbi_set_flip_vertically_on_load(options.flipUVs);

        // 텍스처 로드
        int width, height, channels;
        unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!pixels)
        {
            return MakeError<TextureData>(
                EngineErrorCode::AssetImportFailed,
                "Failed to load texture: " + path + " (" + std::string(stbi_failure_reason()) + ")",
                "TextureImporter"
            );
        }

        data.width = width;
        data.height = height;
        data.channels = channels;

        // 픽셀 데이터 복사
        size_t pixelCount = width * height * channels;
        data.pixels.resize(pixelCount);
        std::memcpy(data.pixels.data(), pixels, pixelCount);

        // stb_image 메모리 해제
        stbi_image_free(pixels);

        // 최대 텍스처 크기 제한
        if (options.maxTextureSize > 0 && 
            (width > static_cast<int>(options.maxTextureSize) || 
             height > static_cast<int>(options.maxTextureSize)))
        {
            // 리사이징 로직은 추후 구현
            ENGINE_LOG_WARN("Texture exceeds max size ({}x{}), resize not yet implemented", 
                width, height);
        }

        return data;
    }

    TextureFormat TextureImporter::DetermineTextureFormat(int channels)
    {
        switch (channels)
        {
            case 1: return TextureFormat::RGBA8;  // Grayscale (RGBA로 확장)
            case 2: return TextureFormat::RGBA8;  // Grayscale + Alpha (RGBA로 확장)
            case 3: return TextureFormat::RGBA8;  // RGB (RGBA로 확장)
            case 4: return TextureFormat::RGBA8;  // RGBA
            default: return TextureFormat::RGBA8;
        }
    }

} // namespace Engine
