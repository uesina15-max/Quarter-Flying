#include <gtest/gtest.h>
#include "renderer/RenderGraph.h"
#include "renderer/CommandList.h"
#include "core/logging/Logger.h"
#include <vector>
#include <string>
#include <memory>

using namespace Engine;

class RenderGraphExecuteTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Logger::Initialize();
        graph = std::make_unique<RenderGraph>();
        cmdList = std::make_unique<MockCommandList>();
    }

    void TearDown() override
    {
        cmdList.reset();
        graph.reset();
        Logger::Shutdown();
    }

    std::unique_ptr<RenderGraph> graph;
    std::unique_ptr<MockCommandList> cmdList;
};

// Test basic execution - Requirement 10.1: Execute passes in order according to ExecutionPlan
TEST_F(RenderGraphExecuteTest, Execute_SinglePass_ExecutesAccordingToExecutionPlan)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    // Add a pass
    bool executeCalled = false;
    graph->AddPass("ClearPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            executeCalled = true;
            cmd.Clear(0.2f, 0.3f, 0.8f, 1.0f);
        }
    );

    // Compile to generate ExecutionPlan
    graph->Compile();
    ASSERT_TRUE(graph->IsCompiled());

    // Verify ExecutionPlan was created
    EXPECT_EQ(graph->GetPassCount(), 1u);

    // Execute according to ExecutionPlan - Requirement 10.1
    graph->Execute(*cmdList);

    // Verify execution occurred according to plan
    EXPECT_TRUE(executeCalled);
    
    // Verify CommandList was provided to pass - Requirement 10.2
    const auto& commands = cmdList->GetCommands();
    EXPECT_GT(commands.size(), 0u);
    EXPECT_TRUE(cmdList->HasCommand("BeginEvent(ClearPass)"));
    EXPECT_TRUE(cmdList->HasCommand("Clear(0.200000, 0.300000, 0.800000, 1.000000)"));
    EXPECT_TRUE(cmdList->HasCommand("EndEvent()"));
}

// Test multiple passes execute in correct order according to ExecutionPlan - Requirement 10.1
TEST_F(RenderGraphExecuteTest, Execute_MultiplePasses_ExecutesAccordingToExecutionPlan)
{
    // Create textures
    RGTextureDesc colorDesc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    RGTextureDesc depthDesc(1920, 1080, TextureFormat::Depth24Stencil8, TextureUsage::DepthStencil);
    
    graph->CreateTexture("ColorBuffer", colorDesc);
    graph->CreateTexture("DepthBuffer", depthDesc);

    std::vector<std::string> executionOrder;

    // Add passes with dependencies to test ExecutionPlan ordering
    graph->AddPass("ClearPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
            builder.WriteTexture("DepthBuffer");
        },
        [&](CommandList& cmd) {
            executionOrder.push_back("ClearPass");
            cmd.Clear(0.0f, 0.0f, 0.0f, 1.0f);
            cmd.ClearDepth(1.0f);
        }
    );

    graph->AddPass("GeometryPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");  // Creates dependency on ClearPass
            builder.WriteTexture("ColorBuffer");
            builder.WriteTexture("DepthBuffer");
        },
        [&](CommandList& cmd) {
            executionOrder.push_back("GeometryPass");
            cmd.DrawIndexed(36);
        }
    );

    graph->AddPass("PostProcessPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");  // Creates dependency on GeometryPass
        },
        [&](CommandList& cmd) {
            executionOrder.push_back("PostProcessPass");
            cmd.Draw(3);
        }
    );

    // Compile to generate ExecutionPlan with correct ordering
    graph->Compile();
    EXPECT_EQ(graph->GetPassCount(), 3u);

    // Execute according to ExecutionPlan - Requirement 10.1
    graph->Execute(*cmdList);

    // Verify execution order follows ExecutionPlan (dependency-resolved order)
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], "ClearPass");
    EXPECT_EQ(executionOrder[1], "GeometryPass");
    EXPECT_EQ(executionOrder[2], "PostProcessPass");

    // Verify all passes received CommandList - Requirement 10.2
    const auto& commands = cmdList->GetCommands();
    EXPECT_TRUE(cmdList->HasCommand("BeginEvent(ClearPass)"));
    EXPECT_TRUE(cmdList->HasCommand("BeginEvent(GeometryPass)"));
    EXPECT_TRUE(cmdList->HasCommand("BeginEvent(PostProcessPass)"));
    EXPECT_TRUE(cmdList->HasCommand("Clear(0.000000, 0.000000, 0.000000, 1.000000)"));
    EXPECT_TRUE(cmdList->HasCommand("DrawIndexed(36, 0, 0)"));
    EXPECT_TRUE(cmdList->HasCommand("Draw(3, 0)"));
}

// Test execution without compilation fails gracefully
TEST_F(RenderGraphExecuteTest, Execute_WithoutCompilation_FailsGracefully)
{
    // Add a pass but don't compile
    graph->AddPass("TestPass",
        [&](RGBuilder& builder) {
            // Empty setup
        },
        [](CommandList& cmd) {
            cmd.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        }
    );

    // Try to execute without compiling
    graph->Execute(*cmdList);

    // Should have no commands recorded
    EXPECT_EQ(cmdList->GetCommandCount(), 0u);
}

// Test empty graph execution
TEST_F(RenderGraphExecuteTest, Execute_EmptyGraph_HandlesGracefully)
{
    // Compile and execute empty graph
    graph->Compile();
    graph->Execute(*cmdList);

    // Should have no commands recorded
    EXPECT_EQ(cmdList->GetCommandCount(), 0u);
}

// Test pass execution with barriers
TEST_F(RenderGraphExecuteTest, Execute_WithBarriers_InsertsBarriersCorrectly)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("SharedTexture", desc);

    // Add passes that create read-after-write dependency
    graph->AddPass("WritePass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("SharedTexture");
        },
        [&](CommandList& cmd) {
            cmd.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        }
    );

    graph->AddPass("ReadPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("SharedTexture");
        },
        [&](CommandList& cmd) {
            cmd.Draw(3);
        }
    );

    // Compile and execute
    graph->Compile();
    graph->Execute(*cmdList);

    // Verify barriers were inserted
    const auto& commands = cmdList->GetCommands();
    bool foundBarrier = false;
    for (const auto& cmd : commands)
    {
        if (cmd.find("ResourceBarrier") != std::string::npos)
        {
            foundBarrier = true;
            break;
        }
    }
    EXPECT_TRUE(foundBarrier);
}

// Test pass execution provides correct CommandList interface - Requirement 10.2
TEST_F(RenderGraphExecuteTest, Execute_PassReceivesCommandList_CanUseAllMethods)
{
    // Create textures
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    // Add pass that uses various CommandList methods
    graph->AddPass("ComplexPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            // Test that all CommandList methods are available - Requirement 10.2
            cmd.Clear(0.5f, 0.5f, 0.5f, 1.0f);
            cmd.ClearDepth(1.0f);
            cmd.SetRenderTarget(GPUTextureHandle(1, 0));
            cmd.SetDepthStencil(GPUTextureHandle(2, 0));
            cmd.SetTexture(0, GPUTextureHandle(3, 0));
            cmd.DrawIndexed(100, 10, 5);
            cmd.Draw(50, 20);
        }
    );

    // Compile and execute
    graph->Compile();
    graph->Execute(*cmdList);

    // Verify all CommandList methods were called - confirms Requirement 10.2
    const auto& commands = cmdList->GetCommands();
    EXPECT_TRUE(cmdList->HasCommand("Clear(0.500000, 0.500000, 0.500000, 1.000000)"));
    EXPECT_TRUE(cmdList->HasCommand("ClearDepth(1.000000)"));
    EXPECT_TRUE(cmdList->HasCommand("SetRenderTarget(1)"));
    EXPECT_TRUE(cmdList->HasCommand("SetDepthStencil(2)"));
    EXPECT_TRUE(cmdList->HasCommand("SetTexture(0, 3)"));
    EXPECT_TRUE(cmdList->HasCommand("DrawIndexed(100, 10, 5)"));
    EXPECT_TRUE(cmdList->HasCommand("Draw(50, 20)"));
}

// Test ExecutionPlan integrity validation - Requirement 10.1
TEST_F(RenderGraphExecuteTest, Execute_ValidatesExecutionPlanIntegrity)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    // Add passes
    graph->AddPass("Pass1",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            cmd.Clear(1.0f, 0.0f, 0.0f, 1.0f);
        }
    );

    graph->AddPass("Pass2",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            cmd.Draw(3);
        }
    );

    // Compile to create ExecutionPlan
    graph->Compile();
    ASSERT_TRUE(graph->IsCompiled());

    // Execute should follow ExecutionPlan order - Requirement 10.1
    graph->Execute(*cmdList);

    // Verify passes executed in ExecutionPlan order
    const auto& commands = cmdList->GetCommands();
    
    // Find the order of BeginEvent calls to verify execution order
    std::vector<std::string> passOrder;
    for (const auto& cmd : commands)
    {
        if (cmd.find("BeginEvent(Pass") == 0)
        {
            if (cmd == "BeginEvent(Pass1)")
                passOrder.push_back("Pass1");
            else if (cmd == "BeginEvent(Pass2)")
                passOrder.push_back("Pass2");
        }
    }

    // Pass1 should execute before Pass2 due to dependency
    ASSERT_EQ(passOrder.size(), 2u);
    EXPECT_EQ(passOrder[0], "Pass1");
    EXPECT_EQ(passOrder[1], "Pass2");
}

// Test exception handling during pass execution
TEST_F(RenderGraphExecuteTest, Execute_PassThrowsException_HandlesGracefully)
{
    // Create a texture
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);
    graph->CreateTexture("ColorBuffer", desc);

    // Add pass that throws exception
    graph->AddPass("ThrowingPass",
        [&](RGBuilder& builder) {
            builder.WriteTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            throw std::runtime_error("Test exception");
        }
    );

    // Add another pass to verify execution continues
    bool secondPassExecuted = false;
    graph->AddPass("NormalPass",
        [&](RGBuilder& builder) {
            builder.ReadTexture("ColorBuffer");
        },
        [&](CommandList& cmd) {
            secondPassExecuted = true;
            cmd.Clear(0.0f, 1.0f, 0.0f, 1.0f);
        }
    );

    // Compile and execute - should not crash
    graph->Compile();
    graph->Execute(*cmdList);

    // Second pass should still execute despite first pass throwing
    EXPECT_TRUE(secondPassExecuted);
    EXPECT_TRUE(cmdList->HasCommand("Clear(0.000000, 1.000000, 0.000000, 1.000000)"));
}