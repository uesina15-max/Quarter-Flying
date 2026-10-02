#include <gtest/gtest.h>
#include "../asset/AssetManager.h"
#include "../asset/importers/TextureImporter.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Engine;

// asset/ 모듈을 실제 파일로 검증한다(개선안 P1-2 "유지하고 연결"). 이 모듈은 빌드만 되고 어디에도
// 연결되지 않아서 (1) 임포터가 픽셀을 버리고 (2) GetAsset<T> 정의가 없고 (3) 레지스트리 참조 카운트가
// 0으로 남거나 uint32_t 아래로 넘치는 버그가 전부 숨어 있었다. 아래 테스트가 각각을 막는다.

namespace
{
    // 2x2 24비트 BMP. 윗줄 = 빨강, 초록 / 아랫줄 = 파랑, 흰색. BMP는 아랫줄부터 저장하고 행을 4바이트로 맞춘다.
    std::string WriteTestBmp(const std::string& name)
    {
        const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
        const uint8_t rowBottom[8] = { 255, 0, 0,   255, 255, 255,   0, 0 };  // BGR: 파랑, 흰색 + 패딩
        const uint8_t rowTop[8]    = { 0, 0, 255,   0, 255, 0,       0, 0 };  // BGR: 빨강, 초록 + 패딩
        const uint32_t pixelBytes = 16;
        const uint32_t fileSize = 14 + 40 + pixelBytes;

        std::vector<uint8_t> f;
        auto u16 = [&f](uint16_t v) { f.push_back(static_cast<uint8_t>(v & 0xFF)); f.push_back(static_cast<uint8_t>(v >> 8)); };
        auto u32 = [&f](uint32_t v) { for (int i = 0; i < 4; ++i) f.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF)); };
        f.push_back('B'); f.push_back('M');
        u32(fileSize); u32(0); u32(14 + 40);
        u32(40); u32(2); u32(2); u16(1); u16(24); u32(0); u32(pixelBytes); u32(2835); u32(2835); u32(0); u32(0);
        f.insert(f.end(), rowBottom, rowBottom + 8);
        f.insert(f.end(), rowTop, rowTop + 8);

        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
        return path.string();
    }
}

TEST(AssetManagerTextureTest, LoadAsset_ReturnsPixelsInFileOrder)
{
    const std::string path = WriteTestBmp("qf_asset_test_pixels.bmp");
    AssetManager assets;
    ASSERT_TRUE(assets.Initialize().has_value());

    auto handle = assets.LoadAsset(path);
    ASSERT_TRUE(handle.has_value()) << handle.error().message;

    auto data = assets.GetAsset<TextureData>(handle.value());
    ASSERT_TRUE(data.has_value()) << data.error().message;   // 예전: runtimeData가 nullptr라 여기서 실패
    const TextureData& tex = *data.value();
    EXPECT_EQ(tex.width, 2);
    EXPECT_EQ(tex.height, 2);
    ASSERT_EQ(tex.channels, 3);
    ASSERT_EQ(tex.pixels.size(), 12u);
    // 윗줄 먼저(Texture2D::Create가 기대하는 순서)
    EXPECT_EQ(tex.pixels[0], 255); EXPECT_EQ(tex.pixels[1], 0);   EXPECT_EQ(tex.pixels[2], 0);    // 빨강
    EXPECT_EQ(tex.pixels[3], 0);   EXPECT_EQ(tex.pixels[4], 255); EXPECT_EQ(tex.pixels[5], 0);    // 초록
    EXPECT_EQ(tex.pixels[6], 0);   EXPECT_EQ(tex.pixels[7], 0);   EXPECT_EQ(tex.pixels[8], 255);  // 파랑

    assets.Shutdown();
    std::filesystem::remove(path);
}

TEST(AssetManagerTextureTest, SamePathTwice_ReturnsSameHandle_AndDataSurvivesOneUnload)
{
    const std::string path = WriteTestBmp("qf_asset_test_refcount.bmp");
    AssetManager assets;
    ASSERT_TRUE(assets.Initialize().has_value());

    auto h1 = assets.LoadAsset(path);
    auto h2 = assets.LoadAsset(path);
    ASSERT_TRUE(h1.has_value());
    ASSERT_TRUE(h2.has_value());
    EXPECT_EQ(h1.value(), h2.value());

    auto meta = assets.GetMetadata(h1.value());
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta.value()->referenceCount, 2u);   // 예전: 레지스트리 쪽이 0에서 시작해서 1이었다
    EXPECT_TRUE(meta.value()->isLoaded);

    // 두 사용자 중 하나만 놓아도 데이터는 남아 있어야 한다
    assets.UnloadAsset(h1.value());
    EXPECT_TRUE(assets.GetAsset<TextureData>(h1.value()).has_value());

    assets.UnloadAsset(h1.value());
    EXPECT_FALSE(assets.GetAsset<TextureData>(h1.value()).has_value());

    // 한 번 더 놓아도 카운트가 uint32_t 아래로 넘치지 않는다
    assets.UnloadAsset(h1.value());
    EXPECT_EQ(assets.GetMetadata(h1.value()).value()->referenceCount, 0u);

    assets.Shutdown();
    std::filesystem::remove(path);
}

TEST(AssetManagerTextureTest, LoadAfterFullUnload_ReloadsData)
{
    const std::string path = WriteTestBmp("qf_asset_test_reload.bmp");
    AssetManager assets;
    ASSERT_TRUE(assets.Initialize().has_value());

    auto h1 = assets.LoadAsset(path);
    ASSERT_TRUE(h1.has_value());
    assets.UnloadAsset(h1.value());
    ASSERT_FALSE(assets.GetAsset<TextureData>(h1.value()).has_value());

    auto h2 = assets.LoadAsset(path);
    ASSERT_TRUE(h2.has_value()) << h2.error().message;
    EXPECT_EQ(h1.value(), h2.value());
    auto data = assets.GetAsset<TextureData>(h2.value());   // 예전: 핸들은 받는데 데이터가 없었다
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data.value()->pixels.size(), 12u);

    assets.Shutdown();
    std::filesystem::remove(path);
}

TEST(AssetManagerTextureTest, MissingFile_IsError_NotCrash)
{
    AssetManager assets;
    ASSERT_TRUE(assets.Initialize().has_value());
    auto handle = assets.LoadAsset((std::filesystem::temp_directory_path() / "qf_no_such_texture.png").string());
    EXPECT_FALSE(handle.has_value());
    assets.Shutdown();
}
