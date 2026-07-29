#include <gtest/gtest.h>
#include "renderer/RenderGraph.h"
#include "core/logging/Logger.h"

using namespace Engine;
#include "renderer/CommandList.h"
#include <memory>


class RenderGraphBasicTest : public ::testing::Test
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

// Test CreateTexture functionality
TEST_F(RenderGraphBasicTest, CreateTexture_ValidDesc_ReturnsValidHandle)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto handle = graph->CreateTexture("ColorBuffer", desc);
    
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 1u);
    EXPECT_TRUE(graph->HasResource("ColorBuffer"));
    EXPECT_EQ(graph->GetResourceHandle("ColorBuffer"), handle);
}

TEST_F(RenderGraphBasicTest, CreateTexture_DuplicateName_ReturnsExistingHandle)
{
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto handle1 = graph->CreateTexture("ColorBuffer", desc);
    auto handle2 = graph->CreateTexture("ColorBuffer", desc);
    
    EXPECT_EQ(handle1, handle2);
    EXPECT_EQ(graph->GetResourceCount(), 1u);
}

TEST_F(RenderGraphBasicTest, CreateTexture_InvalidDimensions_ReturnsZero)
{
    RGTextureDesc desc(0, 0, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto handle = graph->CreateTexture("InvalidTexture", desc);
    
    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 0u);
}

// Test ImportTexture functionality
TEST_F(RenderGraphBasicTest, ImportTexture_ValidHandle_ReturnsValidHandle)
{
    GPUTextureHandle mockHandle(12345, 0);  // Create valid handle
    auto handle = graph->ImportTexture("BackBuffer", mockHandle);
    
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 1u);
    EXPECT_TRUE(graph->HasResource("BackBuffer"));
    
    const auto& resource = graph->GetResource(handle);
    EXPECT_TRUE(resource.imported);
    EXPECT_EQ(resource.name, "BackBuffer");
}

TEST_F(RenderGraphBasicTest, ImportTexture_InvalidHandle_ReturnsZero)
{
    GPUTextureHandle invalidHandle;  // Default constructor creates invalid handle
    auto handle = graph->ImportTexture("InvalidImport", invalidHandle);
    
    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(graph->GetResourceCount(), 0u);
}

// Test AddPass functionality
TEST_F(RenderGraphBasicTest, AddPass_ValidPass_AddsSuccessfully)
{
    // First create a texture to use in the pass
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);
    
    bool setupCalled = false;
    bool executeCalled = false;
    
    graph->AddPass("ClearPass",
        [&](RGBuilder& builder) {
            setupCalled = true;
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmdList) {
            executeCalled = true;
        }
    );
    
    EXPECT_EQ(graph->GetPassCount(), 1u);
    EXPECT_TRUE(setupCalled);
    EXPECT_FALSE(executeCalled); // Execute should not be called during AddPass
    
    const auto& pass = graph->GetPass(0);
    EXPECT_EQ(pass.name, "ClearPass");
    EXPECT_EQ(pass.writes.size(), 1u);
    EXPECT_EQ(pass.reads.size(), 0u);
}

TEST_F(RenderGraphBasicTest, AddPass_EmptyName_DoesNotAdd)
{
    graph->AddPass("",
        [](RGBuilder& builder) {},
        [](CommandList& cmdList) {}
    );
    
    EXPECT_EQ(graph->GetPassCount(), 0u);
}

TEST_F(RenderGraphBasicTest, AddPass_NullSetup_DoesNotAdd)
{
    graph->AddPass("TestPass",
        nullptr,
        [](CommandList& cmdList) {}
    );
    
    EXPECT_EQ(graph->GetPassCount(), 0u);
}

TEST_F(RenderGraphBasicTest, AddPass_NullExecute_DoesNotAdd)
{
    graph->AddPass("TestPass",
        [](RGBuilder& builder) {},
        nullptr
    );
    
    EXPECT_EQ(graph->GetPassCount(), 0u);
}

// Test pass read/write relationships
TEST_F(RenderGraphBasicTest, AddPass_RecordsReadWriteRelationships)
{
    // Create textures
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);
    graph->ImportTexture("BackBuffer", GPUTextureHandle{12345, 1});
    
    // Add pass that reads from ColorBuffer and writes to BackBuffer
    graph->AddPass("CopyPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");
            builder.WriteTexture("BackBuffer");
        },
        [](CommandList& cmdList) {}
    );
    
    EXPECT_EQ(graph->GetPassCount(), 1u);
    
    const auto& pass = graph->GetPass(0);
    EXPECT_EQ(pass.reads.size(), 1u);
    EXPECT_EQ(pass.writes.size(), 1u);
    
    // Verify the handles match
    auto colorHandle = graph->GetResourceHandle("ColorBuffer");
    auto backHandle = graph->GetResourceHandle("BackBuffer");
    
    EXPECT_EQ(pass.reads[0], colorHandle);
    EXPECT_EQ(pass.writes[0], backHandle);
}

// Test Reset functionality
TEST_F(RenderGraphBasicTest, Reset_ClearsAllData)
{
    // Add some data
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);
    graph->ImportTexture("BackBuffer", GPUTextureHandle{12345, 1});
    
    graph->AddPass("TestPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [](CommandList& cmdList) {}
    );
    
    EXPECT_GT(graph->GetPassCount(), 0u);
    EXPECT_GT(graph->GetResourceCount(), 0u);
    EXPECT_TRUE(graph->HasResource("ColorBuffer"));
    
    // Reset
    graph->Reset();
    
    EXPECT_EQ(graph->GetPassCount(), 0u);
    EXPECT_EQ(graph->GetResourceCount(), 0u);
    EXPECT_FALSE(graph->HasResource("ColorBuffer"));
    EXPECT_FALSE(graph->HasResource("BackBuffer"));
}

// Test GetResource with invalid handle
TEST_F(RenderGraphBasicTest, GetResource_InvalidHandle_ReturnsInvalidTexture)
{
    const auto& resource = graph->GetResource(RGTextureHandle(999, 0));
    
    EXPECT_FALSE(resource.id.IsValid());
    EXPECT_EQ(resource.name, "INVALID");
}
