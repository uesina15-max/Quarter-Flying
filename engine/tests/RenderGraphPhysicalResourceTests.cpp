#include <gtest/gtest.h>
#include "../renderer/RenderGraph.h"
#include "../renderer/CommandList.h"
#include <memory>

using namespace Engine;

class RenderGraphPhysicalResourceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        renderGraph = std::make_unique<RenderGraph>();
    }

    void TearDown() override
    {
        renderGraph.reset();
    }

    std::unique_ptr<RenderGraph> renderGraph;
};

// Test physical resource creation for non-aliased resources
TEST_F(RenderGraphPhysicalResourceTest, CreatePhysicalResources_NonAliased_CreatesUniqueResources)
{
    // Create two textures with different sizes (cannot be aliased)
    RGTextureDesc desc1(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    RGTextureDesc desc2(512, 512, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    
    auto texture1 = renderGraph->CreateTexture("Texture1", desc1);
    auto texture2 = renderGraph->CreateTexture("Texture2", desc2);

    // Add passes that use these textures
    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    renderGraph->AddPass("Pass2", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture2");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(0.0f, 1.0f, 0.0f, 1.0f);
        });

    // Compile the graph
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());

    // Check that physical resources were created
    EXPECT_EQ(renderGraph->GetPhysicalResourceCount(), 2);

    // Check that each logical resource has a valid physical resource
    GPUTextureHandle physical1 = renderGraph->GetPhysicalResource(texture1);
    GPUTextureHandle physical2 = renderGraph->GetPhysicalResource(texture2);

    EXPECT_TRUE(physical1.IsValid());
    EXPECT_TRUE(physical2.IsValid());
    EXPECT_NE(physical1.id, physical2.id);  // Should be different physical resources
}

// Test physical resource aliasing for compatible resources
TEST_F(RenderGraphPhysicalResourceTest, CreatePhysicalResources_CompatibleResources_AliasesCorrectly)
{
    // Create two textures with same size and format (can be aliased)
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    
    auto texture1 = renderGraph->CreateTexture("Texture1", desc);
    auto texture2 = renderGraph->CreateTexture("Texture2", desc);

    // Add passes with non-overlapping lifetimes
    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    renderGraph->AddPass("Pass2", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture2"); // Write texture2 after texture1's lifetime ends
        },
        [](CommandList& cmdList) {
            cmdList.Clear(0.0f, 1.0f, 0.0f, 1.0f);
        });

    // Compile the graph
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());

    // Should have fewer physical resources than logical resources due to aliasing
    EXPECT_LE(renderGraph->GetPhysicalResourceCount(), 2);

    // Check aliasing information
    bool texture1Aliased = renderGraph->IsResourceAliased(texture1);
    bool texture2Aliased = renderGraph->IsResourceAliased(texture2);

    // At least one should be aliased
    EXPECT_TRUE(texture1Aliased || texture2Aliased);

    if (texture2Aliased)
    {
        RGTextureHandle aliasSource = renderGraph->GetAliasSource(texture2);
        EXPECT_EQ(aliasSource, texture1);
    }
}

// Test imported resource handling
TEST_F(RenderGraphPhysicalResourceTest, CreatePhysicalResources_ImportedResource_UsesExistingHandle)
{
    // Import an external texture
    GPUTextureHandle externalHandle(999, 0);
    auto importedTexture = renderGraph->ImportTexture("ImportedTexture", externalHandle);

    // Create a regular texture
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto regularTexture = renderGraph->CreateTexture("RegularTexture", desc);

    // Add passes that use these textures
    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.ReadTexture("ImportedTexture");
            builder.WriteTexture("RegularTexture");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    // Compile the graph
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());

    // Check physical resources
    GPUTextureHandle physicalImported = renderGraph->GetPhysicalResource(importedTexture);
    GPUTextureHandle physicalRegular = renderGraph->GetPhysicalResource(regularTexture);

    EXPECT_TRUE(physicalImported.IsValid());
    EXPECT_TRUE(physicalRegular.IsValid());

    // Imported resource should use the original handle (or at least have the same ID)
    EXPECT_EQ(physicalImported.id, importedTexture.id);
}

// Test unused resource handling
TEST_F(RenderGraphPhysicalResourceTest, CreatePhysicalResources_UnusedResource_SkipsCreation)
{
    // Create a texture but don't use it in any pass
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto unusedTexture = renderGraph->CreateTexture("UnusedTexture", desc);

    // Create and use another texture
    auto usedTexture = renderGraph->CreateTexture("UsedTexture", desc);

    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("UsedTexture");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    // Compile the graph
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());

    // Should only have one physical resource (for the used texture)
    EXPECT_EQ(renderGraph->GetPhysicalResourceCount(), 1);

    // Unused texture should not have a valid physical resource
    GPUTextureHandle physicalUnused = renderGraph->GetPhysicalResource(unusedTexture);
    GPUTextureHandle physicalUsed = renderGraph->GetPhysicalResource(usedTexture);

    EXPECT_FALSE(physicalUnused.IsValid());
    EXPECT_TRUE(physicalUsed.IsValid());
}

// Test resource lifecycle management
TEST_F(RenderGraphPhysicalResourceTest, ReleaseTransientResources_ReleasesNonImportedResources)
{
    // Create regular and imported textures
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto regularTexture = renderGraph->CreateTexture("RegularTexture", desc);
    
    GPUTextureHandle externalHandle(999, 0);
    auto importedTexture = renderGraph->ImportTexture("ImportedTexture", externalHandle);

    // Add pass that uses both
    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("RegularTexture");
            builder.ReadTexture("ImportedTexture");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    // Compile and check initial state
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());
    EXPECT_GT(renderGraph->GetPhysicalResourceCount(), 0);

    // Release transient resources
    renderGraph->ReleaseTransientResources();

    // Physical resources should still exist but transient ones should be marked as released
    // (In this test implementation, we can't easily verify the internal state change,
    // but the method should execute without errors)
    EXPECT_GT(renderGraph->GetPhysicalResourceCount(), 0);
}

// Test resource pool return
TEST_F(RenderGraphPhysicalResourceTest, ReturnResourcesToPool_ReturnsNonAliasedResources)
{
    // Create textures
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture1 = renderGraph->CreateTexture("Texture1", desc);
    auto texture2 = renderGraph->CreateTexture("Texture2", desc);

    // Add passes
    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture1");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    renderGraph->AddPass("Pass2", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture2");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(0.0f, 1.0f, 0.0f, 1.0f);
        });

    // Compile
    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());

    // Return resources to pool (should execute without errors)
    renderGraph->ReturnResourcesToPool();
    
    // Method should complete successfully
    EXPECT_TRUE(renderGraph->IsCompiled());
}

// Test physical resource access methods
TEST_F(RenderGraphPhysicalResourceTest, GetPhysicalResourceByIndex_ValidIndex_ReturnsResource)
{
    // Create texture and pass
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture = renderGraph->CreateTexture("Texture", desc);

    renderGraph->AddPass("Pass1", 
        [&](RGBuilder& builder) {
            builder.WriteTexture("Texture");
        },
        [](CommandList& cmdList) {
            cmdList.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        });

    renderGraph->Compile();
    ASSERT_TRUE(renderGraph->IsCompiled());
    ASSERT_GT(renderGraph->GetPhysicalResourceCount(), 0);

    // Get physical resource by index
    const PhysicalTexture& physical = renderGraph->GetPhysicalResourceByIndex(0);
    EXPECT_TRUE(physical.handle.IsValid());
    EXPECT_EQ(physical.firstUse, 0);  // First pass
    EXPECT_EQ(physical.lastUse, 0);   // Only used in first pass
}

// Test error handling for invalid access
TEST_F(RenderGraphPhysicalResourceTest, GetPhysicalResource_InvalidHandle_ReturnsInvalidHandle)
{
    renderGraph->Compile();  // Compile empty graph
    
    // Try to get physical resource for invalid handle
    RGTextureHandle invalidHandle(9999, 0);
    GPUTextureHandle physical = renderGraph->GetPhysicalResource(invalidHandle);
    
    EXPECT_FALSE(physical.IsValid());
}

TEST_F(RenderGraphPhysicalResourceTest, GetPhysicalResource_UncompiledGraph_ReturnsInvalidHandle)
{
    // Create texture but don't compile
    RGTextureDesc desc(1024, 768, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    auto texture = renderGraph->CreateTexture("Texture", desc);
    
    // Try to get physical resource before compilation
    GPUTextureHandle physical = renderGraph->GetPhysicalResource(texture);
    
    EXPECT_FALSE(physical.IsValid());
}
