// Feature: game-engine-renderer-instancing
// Unit tests for Renderer Instancing Submission & MockCommandList recording

#include <GL/glew.h>   // gl.h보다 먼저 와야 한다
#include <gtest/gtest.h>
#include <vector>
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

namespace
{
    // 테스트용 최소 셰이더. 화면 결과는 보지 않고 "유효한 프로그램으로 draw가 발행되는가"만 본다.
    const char* kTestVS = "#version 330 core\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    const char* kTestFS = "#version 330 core\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n";

    // 인덱스 버퍼(EBO)를 가진 VAO를 만들어 바인딩한 채로 돌려준다.
    struct IndexedVao
    {
        GLuint vao = 0, ebo = 0;
        explicit IndexedVao(uint32_t indexCount)
        {
            std::vector<uint32_t> indices(indexCount, 0u);
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        }
        ~IndexedVao()
        {
            glBindVertexArray(0);
            glDeleteBuffers(1, &ebo);
            glDeleteVertexArrays(1, &vao);
        }
    };
}

TEST_F(RendererInstancingTest, SubmitInstancedBatchSucceedsInFrame)
{
    uint32_t shader = renderer.CreateShaderProgram(kTestVS, kTestFS);
    ASSERT_NE(shader, 0u);
    renderer.BeginFrame();

    RenderBatchKey key{100, 200, shader, PipelineFeature::Blend};
    auto res = renderer.SubmitInstancedBatch(key, 36, 100, 0, 0);
    EXPECT_TRUE(res.has_value());

    renderer.EndFrame();
}

TEST_F(RendererInstancingTest, SubmitIndexedInstancedBatchSucceeds)
{
    // 예전 버전은 가짜 shaderId(10)와 EBO 없는 상태로 실제 glDrawElements*를 호출해서
    // access violation으로 죽었다(OpenGLCommandList::DrawIndexedInstanced 주석 참고).
    uint32_t shader = renderer.CreateShaderProgram(kTestVS, kTestFS);
    ASSERT_NE(shader, 0u);
    renderer.BeginFrame();          // Reset()이 VAO를 0으로 되돌리므로 VAO는 그 뒤에 바인딩한다
    IndexedVao vao(120);

    RenderBatchKey key{101, 201, shader, PipelineFeature::DepthTest};
    auto res = renderer.SubmitIndexedInstancedBatch(key, 120, 50);
    EXPECT_TRUE(res.has_value()) << (res.has_value() ? "" : res.error().message);

    renderer.EndFrame();
}

TEST_F(RendererInstancingTest, SubmitWithUnknownShader_ReturnsErrorWithoutDrawing)
{
    renderer.BeginFrame();

    RenderBatchKey key{100, 200, 9999, PipelineFeature::None};
    auto res = renderer.SubmitInstancedBatch(key, 36, 10);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, EngineErrorCode::InvalidParameter);

    auto indexed = renderer.SubmitIndexedInstancedBatch(key, 120, 50);
    ASSERT_FALSE(indexed.has_value());
    EXPECT_EQ(indexed.error().code, EngineErrorCode::InvalidParameter);

    renderer.EndFrame();
}

TEST_F(RendererInstancingTest, SubmitIndexedWithoutIndexBuffer_ReturnsErrorInsteadOfCrashing)
{
    uint32_t shader = renderer.CreateShaderProgram(kTestVS, kTestFS);
    ASSERT_NE(shader, 0u);
    renderer.BeginFrame();
    GLuint emptyVao = 0;               // EBO가 없는 VAO (BeginFrame 뒤에 바인딩)
    glGenVertexArrays(1, &emptyVao);
    glBindVertexArray(emptyVao);

    RenderBatchKey key{101, 201, shader, PipelineFeature::None};
    auto res = renderer.SubmitIndexedInstancedBatch(key, 120, 50);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, EngineErrorCode::InvalidState);

    renderer.EndFrame();
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &emptyVao);
}

// ── frustum 컬링 후 GPU 버퍼 내용 (InstancedBatchManager::UpdateVisibility 주석의 버그) ─────
// 컬링되면 "보이는 인스턴스"가 버퍼 앞쪽에 있어야 한다. 예전에는 개수만 줄이고 버퍼는 원래 순서
// 그대로여서, 처음 추가된(보이지 않는) 인스턴스가 그려졌다.
#include "renderer/InstancedBatchManager.h"
#include "renderer/Frustum.h"
#include "renderer/Mesh.h"
#include "renderer/Vertex.h"
#include <glm/gtc/matrix_transform.hpp>

TEST_F(RendererInstancingTest, FrustumCulling_UploadsOnlyVisibleInstances_InFront)
{
    InstancedBatchManager* mgr = renderer.GetInstancedBatchManager();
    ASSERT_NE(mgr, nullptr);

    auto mesh = std::make_shared<Mesh>();
    mesh->create({ {0,0,0, 0,0,1, 0,0}, {1,0,0, 0,0,1, 1,0}, {0,1,0, 0,0,1, 0,1} }, { 0u, 1u, 2u });

    InstancedBatchKey key;
    key.meshGuid = 424242;
    ASSERT_TRUE(mgr->CreateBatch(key, BatchType::Dynamic, mesh).has_value());

    auto instanceAt = [](float x) {
        InstanceData d{};
        d.model = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, 0.0f));
        return d;
    };
    // 먼저 추가된 두 개는 화면 밖(x = -500, -400), 마지막 하나만 화면 안(x = 0)
    ASSERT_TRUE(mgr->AddInstance(key, instanceAt(-500.0f), 1).has_value());
    ASSERT_TRUE(mgr->AddInstance(key, instanceAt(-400.0f), 2).has_value());
    ASSERT_TRUE(mgr->AddInstance(key, instanceAt(0.0f), 3).has_value());

    // 카메라: (0,0,10)에서 원점을 본다
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    Frustum frustum;
    frustum.Update(proj * view);

    mgr->UpdateVisibility(frustum);
    mgr->UpdateDynamicBatches();   // 업로드

    const InstanceBatch* batch = mgr->GetBatch(key);
    ASSERT_NE(batch, nullptr);
    ASSERT_EQ(batch->visibleCount, 1u);

    InstanceData gpuFirst{};
    glBindBuffer(GL_ARRAY_BUFFER, batch->instanceVBO);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(InstanceData), &gpuFirst);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    EXPECT_FLOAT_EQ(gpuFirst.model[3][0], 0.0f)
        << "GPU 버퍼 첫 인스턴스가 보이는 인스턴스(x=0)가 아니다 - drawInstanced(visibleCount)가 엉뚱한 것을 그린다";

    ASSERT_TRUE(mgr->RemoveBatch(key).has_value());
}
