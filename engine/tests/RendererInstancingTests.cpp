// Feature: game-engine-renderer-instancing
// Unit tests for Renderer Instancing Submission & MockCommandList recording

#include <gtest/gtest.h>
#include "renderer/Renderer.h"
#include "renderer/CommandList.h"

using namespace Engine;

class RendererInstancingTest : public ::testing::Test
{
protected:
    Renderer renderer;

    void SetUp() override
    {
        renderer.Initialize();
    }

    void TearDown() override
    {
        renderer.Shutdown();
    }
};

TEST_F(RendererInstancingTest, SubmitInstancedBatchFailsOutsideFrame)
{
    RenderBatchKey key{100, 200, 1, PipelineFeature::None};
    auto res = renderer.SubmitInstancedBatch(key, 36, 10);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, EngineErrorCode::InvalidState);
}

TEST_F(RendererInstancingTest, SubmitInstancedBatchSucceedsInFrame)
{
    renderer.BeginFrame();

    RenderBatchKey key{100, 200, 5, PipelineFeature::Blend};
    auto res = renderer.SubmitInstancedBatch(key, 36, 100, 0, 0);
    EXPECT_TRUE(res.has_value());

    renderer.EndFrame();
}

TEST_F(RendererInstancingTest, SubmitIndexedInstancedBatchSucceeds)
{
    renderer.BeginFrame();

    RenderBatchKey key{101, 201, 10, PipelineFeature::DepthTest};
    auto res = renderer.SubmitIndexedInstancedBatch(key, 120, 50);
    EXPECT_TRUE(res.has_value());

    renderer.EndFrame();
}
