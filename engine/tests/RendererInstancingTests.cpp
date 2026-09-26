// Feature: game-engine-renderer-instancing
// Unit tests for Renderer Instancing Submission & MockCommandList recording

#include <gtest/gtest.h>
#include "renderer/Renderer.h"
#include "renderer/CommandList.h"
#include "platform/Win32Platform.h"   // Windows.h 포함 + CreateWindow 매크로 해제

using namespace Engine;

class RendererInstancingTest : public ::testing::Test
{
protected:
    Win32Platform platform;
    WindowHandle window;
    Renderer renderer;
    bool rendererInitialized = false;

    void SetUp() override
    {
        // 픽스처가 자기 GL 컨텍스트를 직접 만든다. 예전에는 컨텍스트 없이 renderer.Initialize()를
        // 불렀다. 그래서 단독 실행하면 3개 모두 SetUp에서 access violation이 났고, 전체 스위트에서는
        // 앞선 테스트가 남긴 GL 상태에 따라 결과가 달라졌다(Renderer::Initialize 주석 참고).
        ASSERT_TRUE(platform.Initialize()) << "Win32Platform 초기화 실패";
        WindowDesc desc;
        desc.title = "RendererInstancingTest";
        desc.width = 64;
        desc.height = 64;
        window = platform.CreateWindow(desc);
        if (!platform.CreateGraphicsContext(window))
        {
            GTEST_SKIP() << "OpenGL 컨텍스트를 만들 수 없는 환경(헤드리스 등) - 렌더러 테스트 건너뜀";
        }

        auto result = renderer.Initialize();
        ASSERT_TRUE(result.has_value()) << "Renderer::Initialize 실패: " << result.error().message;
        rendererInitialized = true;
    }

    void TearDown() override
    {
        if (rendererInitialized)
        {
            renderer.Shutdown();
        }
        platform.DestroyWindow(window);
        platform.Shutdown();
    }
};

// Renderer::Initialize의 전제조건 검사: 컨텍스트가 없으면 크래시 대신 에러를 돌려줘야 한다.
// (위 픽스처를 쓰지 않는다 - 일부러 컨텍스트 없이 부른다.)
TEST(RendererInitializeTest, WithoutCurrentGLContext_ReturnsErrorInsteadOfCrashing)
{
    wglMakeCurrent(nullptr, nullptr);  // 앞선 테스트가 남긴 current 컨텍스트가 있어도 떼어낸다

    Renderer renderer;
    auto result = renderer.Initialize();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::InvalidState);
}

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
