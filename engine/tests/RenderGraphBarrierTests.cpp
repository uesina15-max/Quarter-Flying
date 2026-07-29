#include <gtest/gtest.h>
#include "../renderer/RenderGraph.h"
#include <memory>

using namespace Engine;

class RenderGraphBarrierTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        graph = std::make_unique<RenderGraph>();
    }

    void TearDown() override
    {
        graph.reset();
    }

    std::unique_ptr<RenderGraph> graph;
};

TEST_F(RenderGraphBarrierTest, SimpleReadAfterWrite)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture = graph->CreateTexture("TestTexture", desc);

    // Pass 1: Write to texture
    graph->AddPass("WritePass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Pass 2: Read from texture
    graph->AddPass("ReadPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Compile and verify barrier insertion
    graph->Compile();
    EXPECT_TRUE(graph->IsCompiled());
}

TEST_F(RenderGraphBarrierTest, WriteAfterRead)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture = graph->CreateTexture("TestTexture", desc);

    // Pass 1: Read from texture (imported or previous frame)
    graph->AddPass("ReadPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Pass 2: Write to texture
    graph->AddPass("WritePass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Compile and verify barrier insertion
    graph->Compile();
    EXPECT_TRUE(graph->IsCompiled());
}

TEST_F(RenderGraphBarrierTest, WriteAfterWrite)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture = graph->CreateTexture("TestTexture", desc);

    // Pass 1: Write to texture
    graph->AddPass("WritePass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Pass 2: Write to texture again
    graph->AddPass("WritePass2",
        [&](RGBuilder& builder) {
            builder.WriteTexture("TestTexture");
        },
        [](CommandList& cmd) {});

    // Compile and verify barrier insertion
    graph->Compile();
    EXPECT_TRUE(graph->IsCompiled());
}

TEST_F(RenderGraphBarrierTest, NoBarrierNeeded)
{
    // Create two separate textures
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture1 = graph->CreateTexture("Texture1", desc);
    auto texture2 = graph->CreateTexture("Texture2", desc);

    // Pass 1: Write to texture1
    graph->AddPass("WritePass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmd) {});

    // Pass 2: Write to texture2 (no dependency)
    graph->AddPass("WritePass2",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture2");
        },
        [](CommandList& cmd) {});

    // Compile - should succeed with no barriers needed between independent passes
    graph->Compile();
    EXPECT_TRUE(graph->IsCompiled());
}

TEST_F(RenderGraphBarrierTest, ComplexDependencyChain)
{
    // Create multiple textures
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto colorBuffer = graph->CreateTexture("ColorBuffer", desc);
    auto intermediate1 = graph->CreateTexture("Intermediate1", desc);
    auto intermediate2 = graph->CreateTexture("Intermediate2", desc);
    auto finalOutput = graph->CreateTexture("FinalOutput", desc);

    // Pass 1: Render to color buffer
    graph->AddPass("GeometryPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [](CommandList& cmd) {});

    // Pass 2: Post-process color buffer to intermediate1
    graph->AddPass("PostProcess1",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");
            builder.WriteTexture("Intermediate1");
        },
        [](CommandList& cmd) {});

    // Pass 3: Post-process intermediate1 to intermediate2
    graph->AddPass("PostProcess2",
        [&](RGBuilder& builder) {
            builder.ReadTexture("Intermediate1");
            builder.WriteTexture("Intermediate2");
        },
        [](CommandList& cmd) {});

    // Pass 4: Final pass - combine color buffer and intermediate2
    graph->AddPass("FinalPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");
            builder.ReadTexture("Intermediate2");
            builder.WriteTexture("FinalOutput");
        },
        [](CommandList& cmd) {});

    // Compile and verify
    graph->Compile();
    EXPECT_TRUE(graph->IsCompiled());
    EXPECT_EQ(graph->GetPassCount(), 4);
    EXPECT_EQ(graph->GetResourceCount(), 4);
}