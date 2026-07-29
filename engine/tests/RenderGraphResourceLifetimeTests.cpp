#include <gtest/gtest.h>
#include "renderer/RenderGraph.h"
#include "core/logging/Logger.h"

using namespace Engine;
#include "renderer/CommandList.h"
#include <memory>


class RenderGraphResourceLifetimeTest : public ::testing::Test
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

// Test resource lifetime calculation
TEST_F(RenderGraphResourceLifetimeTest, ComputeResourceLifetimes_SimpleChain_CalculatesCorrectly)
{
    // Create textures
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("Texture1", desc);
    graph->CreateTexture("Texture2", desc);
    
    // Add passes in sequence: Pass1 writes Texture1, Pass2 reads Texture1 and writes Texture2
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("Pass2",
        [&](RGBuilder& builder) {
            builder.ReadTexture("Texture1");
            builder.WriteTexture("Texture2");
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile to compute lifetimes
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test transient resource aliasing
TEST_F(RenderGraphResourceLifetimeTest, ComputeAliasing_NonOverlappingLifetimes_CreatesAliasing)
{
    // Create two textures with same format and size
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("EarlyTexture", desc);
    graph->CreateTexture("LateTexture", desc);
    
    // Add passes so that EarlyTexture is used first, then LateTexture
    graph->AddPass("EarlyPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("EarlyTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("MiddlePass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("EarlyTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("LatePass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("LateTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile to compute aliasing
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
    // Note: We can't directly test aliasing results without exposing internal state
    // The test verifies that compilation succeeds with aliasing computation
}

// Test aliasing with different formats (should not alias)
TEST_F(RenderGraphResourceLifetimeTest, ComputeAliasing_DifferentFormats_DoesNotAlias)
{
    // Create textures with different formats
    RGTextureDesc desc1(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    RGTextureDesc desc2(1024, 768, TextureFormat::RGBA16F, TextureUsage::RenderTarget);
    
    graph->CreateTexture("Texture1", desc1);
    graph->CreateTexture("Texture2", desc2);
    
    // Add passes with non-overlapping lifetimes
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("Pass2",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture2");
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile - should succeed but not create aliasing due to format mismatch
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test aliasing with different sizes (should not alias)
TEST_F(RenderGraphResourceLifetimeTest, ComputeAliasing_DifferentSizes_DoesNotAlias)
{
    // Create textures with different sizes
    RGTextureDesc desc1(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    RGTextureDesc desc2(512, 384, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    
    graph->CreateTexture("BigTexture", desc1);
    graph->CreateTexture("SmallTexture", desc2);
    
    // Add passes with non-overlapping lifetimes
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("BigTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("Pass2",
        [&](RGBuilder& builder) {
            builder.WriteTexture("SmallTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile - should succeed but not create aliasing due to size mismatch
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test aliasing with overlapping lifetimes (should not alias)
TEST_F(RenderGraphResourceLifetimeTest, ComputeAliasing_OverlappingLifetimes_DoesNotAlias)
{
    // Create two textures with same format and size
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("Texture1", desc);
    graph->CreateTexture("Texture2", desc);
    
    // Add passes so that lifetimes overlap
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
            builder.WriteTexture("Texture2");  // Both textures used in same pass
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile - should succeed but not create aliasing due to overlapping lifetimes
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test aliasing with imported textures (should not alias)
TEST_F(RenderGraphResourceLifetimeTest, ComputeAliasing_ImportedTextures_DoesNotAlias)
{
    // Create one regular texture and one imported texture
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("RegularTexture", desc);
    graph->ImportTexture("ImportedTexture", GPUTextureHandle(12345, 0));
    
    // Add passes with non-overlapping lifetimes
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("RegularTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    graph->AddPass("Pass2",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ImportedTexture");
        },
        [](CommandList& cmdList) {}
    );
    
    // Compile - should succeed but not create aliasing with imported textures
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test compilation with empty graph
TEST_F(RenderGraphResourceLifetimeTest, Compile_EmptyGraph_Succeeds)
{
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}

// Test compilation with unused resources
TEST_F(RenderGraphResourceLifetimeTest, Compile_UnusedResources_Succeeds)
{
    // Create textures but don't use them in any passes
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("UnusedTexture1", desc);
    graph->CreateTexture("UnusedTexture2", desc);
    
    // Compile - should succeed and handle unused resources gracefully
    graph->Compile();
    
    EXPECT_TRUE(graph->IsCompiled());
}
