// RenderGraphUnitTests.cpp
// Task 11.4: RenderGraph 단위 테스트
// Validates: Requirements 8.1, 8.2

#include <gtest/gtest.h>
#include "renderer/RenderGraph.h"
#include "core/logging/Logger.h"
#include <memory>

using namespace Engine;

// ============================================================================
// Test Fixture
// ============================================================================

class RenderGraphUnitTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Logger::Initialize();
        graph = std::make_unique<RenderGraph>();
    }

    void TearDown() override
    {
        graph.reset();
        Logger::Shutdown();
    }

    std::unique_ptr<RenderGraph> graph;
};

// ============================================================================
// Requirements 8.1: Pass 추가 및 읽기/쓰기 리소스 선언 API
// ============================================================================

// 단일 Pass 추가 테스트: AddPass가 Pass를 올바르게 저장하는지 확인
TEST_F(RenderGraphUnitTest, AddPass_SinglePass_StoredCorrectly)
{
    // Arrange
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    bool setupCalled = false;

    // Act
    graph->AddPass("GeometryPass",
        [&](RGBuilder& builder) {
            setupCalled = true;
            builder.WriteTexture("ColorBuffer");
        },
        [](CommandList& cmdList) {}
    );

    // Assert
    EXPECT_EQ(graph->GetPassCount(), 1u);
    EXPECT_TRUE(setupCalled);  // setup lambda must be called during AddPass

    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.name, "GeometryPass");
    EXPECT_EQ(pass.passIndex, 0u);
}

// 단일 Pass 추가 테스트: execute lambda는 AddPass 중에 호출되지 않아야 함
TEST_F(RenderGraphUnitTest, AddPass_SinglePass_ExecuteNotCalledDuringSetup)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    bool executeCalled = false;

    graph->AddPass("TestPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmdList) {
            executeCalled = true;
        }
    );

    EXPECT_FALSE(executeCalled);  // execute must NOT be called during AddPass
}

// 단일 Pass 추가 테스트: passIndex가 올바르게 설정되는지 확인
TEST_F(RenderGraphUnitTest, AddPass_MultiplePassesInOrder_PassIndexIsCorrect)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("Tex0", desc);
    graph->CreateTexture("Tex1", desc);
    graph->CreateTexture("Tex2", desc);

    graph->AddPass("Pass0",
        [&](RGBuilder& builder) { builder.WriteTexture("Tex0"); },
        [](CommandList&) {}
    );
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) { builder.WriteTexture("Tex1"); },
        [](CommandList&) {}
    );
    graph->AddPass("Pass2",
        [&](RGBuilder& builder) { builder.WriteTexture("Tex2"); },
        [](CommandList&) {}
    );

    EXPECT_EQ(graph->GetPassCount(), 3u);
    EXPECT_EQ(graph->GetPass(0).passIndex, 0u);
    EXPECT_EQ(graph->GetPass(1).passIndex, 1u);
    EXPECT_EQ(graph->GetPass(2).passIndex, 2u);
}

// Pass 읽기/쓰기 관계 기록 테스트: reads/writes가 올바르게 추적되는지 확인
TEST_F(RenderGraphUnitTest, AddPass_ReadWriteRelationships_RecordedCorrectly)
{
    // Arrange: 두 개의 텍스처 생성
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("InputTex", desc);
    graph->CreateTexture("OutputTex", desc);

    // Act: InputTex를 읽고 OutputTex에 쓰는 Pass 추가
    graph->AddPass("ProcessPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("InputTex");
            builder.WriteTexture("OutputTex");
        },
        [](CommandList&) {}
    );

    // Assert
    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.reads.size(), 1u);
    EXPECT_EQ(pass.writes.size(), 1u);

    RGTextureHandle inputHandle = graph->GetResourceHandle("InputTex");
    RGTextureHandle outputHandle = graph->GetResourceHandle("OutputTex");

    EXPECT_EQ(pass.reads[0], inputHandle);
    EXPECT_EQ(pass.writes[0], outputHandle);
}

// Pass 읽기/쓰기 관계 기록 테스트: 여러 reads/writes
TEST_F(RenderGraphUnitTest, AddPass_MultipleReadsAndWrites_AllRecorded)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("Albedo", desc);
    graph->CreateTexture("Normal", desc);
    graph->CreateTexture("Depth", desc);
    graph->CreateTexture("LightBuffer", desc);

    graph->AddPass("LightingPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("Albedo");
            builder.ReadTexture("Normal");
            builder.ReadTexture("Depth");
            builder.WriteTexture("LightBuffer");
        },
        [](CommandList&) {}
    );

    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.reads.size(), 3u);
    EXPECT_EQ(pass.writes.size(), 1u);
}

// ============================================================================
// Requirements 8.2: 논리적 리소스(RGTexture) 정의 기능
// ============================================================================

// 리소스 생성 테스트: CreateTexture가 유효한 핸들을 반환하는지 확인
TEST_F(RenderGraphUnitTest, CreateTexture_ValidDesc_ReturnsValidHandle)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    RGTextureHandle handle = graph->CreateTexture("GBuffer", desc);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 1u);
    EXPECT_TRUE(graph->HasResource("GBuffer"));
}

// 리소스 생성 테스트: 생성된 리소스의 속성이 올바른지 확인
TEST_F(RenderGraphUnitTest, CreateTexture_ValidDesc_ResourcePropertiesCorrect)
{
    RGTextureDesc desc(512, 256, TextureFormat::RGBA16F, TextureUsage::ShaderResource);

    RGTextureHandle handle = graph->CreateTexture("HDRBuffer", desc);

    const RGTexture& resource = graph->GetResource(handle);
    EXPECT_EQ(resource.name, "HDRBuffer");
    EXPECT_EQ(resource.desc.width, 512u);
    EXPECT_EQ(resource.desc.height, 256u);
    EXPECT_EQ(resource.desc.format, TextureFormat::RGBA16F);
    EXPECT_EQ(resource.desc.usage, TextureUsage::ShaderResource);
    EXPECT_FALSE(resource.imported);  // 직접 생성한 리소스는 imported가 false
}

// 리소스 생성 테스트: 여러 리소스 생성 시 각각 고유한 핸들을 가지는지 확인
TEST_F(RenderGraphUnitTest, CreateTexture_MultipleTextures_UniqueHandles)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    RGTextureHandle h1 = graph->CreateTexture("Tex1", desc);
    RGTextureHandle h2 = graph->CreateTexture("Tex2", desc);
    RGTextureHandle h3 = graph->CreateTexture("Tex3", desc);

    EXPECT_NE(h1, h2);
    EXPECT_NE(h2, h3);
    EXPECT_NE(h1, h3);
    EXPECT_EQ(graph->GetResourceCount(), 3u);
}

// 리소스 생성 테스트: 잘못된 크기(0x0)로 생성 시 무효 핸들 반환
TEST_F(RenderGraphUnitTest, CreateTexture_ZeroDimensions_ReturnsInvalidHandle)
{
    RGTextureDesc invalidDesc(0, 0, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    RGTextureHandle handle = graph->CreateTexture("InvalidTex", invalidDesc);

    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 0u);
}

// import 테스트: ImportTexture가 유효한 핸들을 반환하는지 확인
TEST_F(RenderGraphUnitTest, ImportTexture_ValidGPUHandle_ReturnsValidHandle)
{
    GPUTextureHandle gpuHandle(42, 0);  // 유효한 GPU 핸들

    RGTextureHandle handle = graph->ImportTexture("BackBuffer", gpuHandle);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 1u);
    EXPECT_TRUE(graph->HasResource("BackBuffer"));
}

// import 테스트: ImportTexture로 가져온 리소스가 imported=true로 표시되는지 확인
TEST_F(RenderGraphUnitTest, ImportTexture_ValidGPUHandle_MarkedAsImported)
{
    GPUTextureHandle gpuHandle(100, 0);

    RGTextureHandle handle = graph->ImportTexture("ExternalTexture", gpuHandle);

    const RGTexture& resource = graph->GetResource(handle);
    EXPECT_TRUE(resource.imported);  // 외부에서 가져온 리소스는 imported가 true
    EXPECT_EQ(resource.name, "ExternalTexture");
}

// import 테스트: 무효한 GPU 핸들로 import 시 무효 핸들 반환
TEST_F(RenderGraphUnitTest, ImportTexture_InvalidGPUHandle_ReturnsInvalidHandle)
{
    GPUTextureHandle invalidGPUHandle;  // 기본 생성자 = 무효 핸들 (id=0)

    RGTextureHandle handle = graph->ImportTexture("InvalidImport", invalidGPUHandle);

    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 0u);
}

// import 테스트: CreateTexture와 ImportTexture 혼합 사용 시 모두 올바르게 저장
TEST_F(RenderGraphUnitTest, CreateAndImportTexture_Mixed_AllStoredCorrectly)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    GPUTextureHandle gpuHandle(999, 0);

    RGTextureHandle created = graph->CreateTexture("CreatedTex", desc);
    RGTextureHandle imported = graph->ImportTexture("ImportedTex", gpuHandle);

    EXPECT_TRUE(created.IsValid());
    EXPECT_TRUE(imported.IsValid());
    EXPECT_NE(created, imported);
    EXPECT_EQ(graph->GetResourceCount(), 2u);

    EXPECT_FALSE(graph->GetResource(created).imported);
    EXPECT_TRUE(graph->GetResource(imported).imported);
}

// ============================================================================
// Requirements 8.3: Pass 추가 시 읽기/쓰기 관계 기록
// ============================================================================

// RGBuilder::CreateTexture가 Pass의 writes에 자동으로 추가되는지 확인
TEST_F(RenderGraphUnitTest, RGBuilder_CreateTexture_AutoAddedToWrites)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    graph->AddPass("CreatePass",
        [&](RGBuilder& builder) {
            builder.CreateTexture("NewTex", desc);
        },
        [](CommandList&) {}
    );

    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.writes.size(), 1u);
    EXPECT_TRUE(graph->HasResource("NewTex"));

    RGTextureHandle handle = graph->GetResourceHandle("NewTex");
    EXPECT_EQ(pass.writes[0], handle);
}

// Pass가 읽기만 하는 경우 writes는 비어있어야 함
TEST_F(RenderGraphUnitTest, AddPass_ReadOnly_WritesEmpty)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::ShaderResource);
    graph->CreateTexture("ShadowMap", desc);

    graph->AddPass("ShadowReadPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ShadowMap");
        },
        [](CommandList&) {}
    );

    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.reads.size(), 1u);
    EXPECT_EQ(pass.writes.size(), 0u);
}

// Pass가 쓰기만 하는 경우 reads는 비어있어야 함
TEST_F(RenderGraphUnitTest, AddPass_WriteOnly_ReadsEmpty)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("OutputTex", desc);

    graph->AddPass("WriteOnlyPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("OutputTex");
        },
        [](CommandList&) {}
    );

    const RGPass& pass = graph->GetPass(0);
    EXPECT_EQ(pass.reads.size(), 0u);
    EXPECT_EQ(pass.writes.size(), 1u);
}
