// RenderGraph 배리어 상태 추적(src -> dst)과 병렬 실행 경로의 배리어 시점 테스트.
// docs/REVIEW_BASED_IMPROVEMENT_PLAN.md P2-1, P2-2.
// 기존 RenderGraphBarrierTests는 "컴파일된다"만 확인해서 배리어 내용이 틀려도 잡지 못했다.

#include <gtest/gtest.h>
#include "../renderer/RenderGraph.h"
#include "../renderer/CommandList.h"
#include "../job/JobSystem.h"
#include <algorithm>

using namespace Engine;

namespace
{
    void AddWritePass(RenderGraph& g, const char* name, const char* tex)
    {
        g.AddPass(name, [tex](RGBuilder& b) { b.WriteTexture(tex); }, [](CommandList&) {});
    }

    void AddReadPass(RenderGraph& g, const char* name, const char* tex)
    {
        g.AddPass(name, [tex](RGBuilder& b) { b.ReadTexture(tex); }, [](CommandList&) {});
    }
}

TEST(RenderGraphBarrierStateTest, WriteThenRead_TransitionsUndefinedToRenderTargetToShaderResource)
{
    RenderGraph g;
    g.CreateTexture("Color", RGTextureDesc(64, 64, TextureFormat::RGBA8, TextureUsage::RenderTarget));
    AddWritePass(g, "Write", "Color");
    AddReadPass(g, "Read", "Color");
    g.Compile();
    ASSERT_TRUE(g.IsCompiled());

    const auto& b = g.GetBarriers();
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0].passIndex, 0u);
    EXPECT_EQ(b[0].srcState, ResourceState::Undefined);
    EXPECT_EQ(b[0].dstState, ResourceState::RenderTarget);
    EXPECT_EQ(b[1].passIndex, 1u);
    EXPECT_EQ(b[1].srcState, ResourceState::RenderTarget);
    EXPECT_EQ(b[1].dstState, ResourceState::ShaderResource);
}

TEST(RenderGraphBarrierStateTest, ConsecutiveReads_NoRedundantBarrier)
{
    RenderGraph g;
    g.CreateTexture("Color", RGTextureDesc(64, 64, TextureFormat::RGBA8, TextureUsage::RenderTarget));
    AddWritePass(g, "Write", "Color");
    AddReadPass(g, "ReadA", "Color");
    AddReadPass(g, "ReadB", "Color");
    g.Compile();
    ASSERT_TRUE(g.IsCompiled());

    const auto& b = g.GetBarriers();
    // Write(Undefined->RT), ReadA(RT->SRV). ReadB는 이미 SRV라 배리어 없음.
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(std::count_if(b.begin(), b.end(), [](const Barrier& x) { return x.passIndex == 2u; }), 0);
}

TEST(RenderGraphBarrierStateTest, DepthTexture_UsesDepthStates)
{
    RenderGraph g;
    g.CreateTexture("Depth", RGTextureDesc(64, 64, TextureFormat::RGBA8, TextureUsage::DepthStencil));
    AddWritePass(g, "Write", "Depth");
    AddReadPass(g, "Read", "Depth");
    g.Compile();
    ASSERT_TRUE(g.IsCompiled());

    const auto& b = g.GetBarriers();
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0].dstState, ResourceState::DepthWrite);
    EXPECT_EQ(b[1].srcState, ResourceState::DepthWrite);
    EXPECT_EQ(b[1].dstState, ResourceState::DepthRead);
}

TEST(RenderGraphBarrierStateTest, SequentialExecute_RecordsBarrierBeforeItsPass_WithStates)
{
    RenderGraph g;
    g.CreateTexture("Color", RGTextureDesc(64, 64, TextureFormat::RGBA8, TextureUsage::RenderTarget));
    AddWritePass(g, "Write", "Color");
    AddReadPass(g, "Read", "Color");
    g.Compile();
    MockCommandList cmd;
    g.Execute(cmd);

    const auto& c = cmd.GetCommands();
    auto find = [&](const std::string& s) {
        return std::find_if(c.begin(), c.end(), [&](const std::string& x) { return x.find(s) != std::string::npos; }) - c.begin();
    };
    const auto barrierRtToSrv = find("->" + std::to_string(static_cast<int>(ResourceState::ShaderResource)) + ")");
    const auto readEvent = find("BeginEvent(Read)");
    ASSERT_LT(barrierRtToSrv, static_cast<long long>(c.size()));
    ASSERT_LT(readEvent, static_cast<long long>(c.size()));
    EXPECT_LT(barrierRtToSrv, readEvent);
}

TEST(RenderGraphBarrierStateTest, ParallelExecute_RecordsBarrierBeforeItsPass_NotAfterAllPasses)
{
    // 예전에는 병렬 경로가 모든 패스 잡이 끝난 뒤 배리어를 한꺼번에 기록해서 배리어가 패스 뒤에 찍혔다.
    RenderGraph g;
    g.CreateTexture("Color", RGTextureDesc(64, 64, TextureFormat::RGBA8, TextureUsage::RenderTarget));
    AddWritePass(g, "Write", "Color");
    AddReadPass(g, "Read", "Color");    // Write에 의존하므로 두 잡은 순서대로 돈다(같은 cmd 동시 접근 없음)
    g.Compile();

    JobSystem jobs;
    jobs.Initialize(2);
    MockCommandList cmd;
    g.ExecuteParallel(cmd, &jobs);
    jobs.Shutdown();

    const auto& c = cmd.GetCommands();
    auto find = [&](const std::string& s) {
        return std::find_if(c.begin(), c.end(), [&](const std::string& x) { return x.find(s) != std::string::npos; }) - c.begin();
    };
    const auto barrierRtToSrv = find("->" + std::to_string(static_cast<int>(ResourceState::ShaderResource)) + ")");
    const auto readEvent = find("BeginEvent(Read)");
    ASSERT_LT(barrierRtToSrv, static_cast<long long>(c.size())) << "RT->SRV 배리어가 기록되지 않았다";
    ASSERT_LT(readEvent, static_cast<long long>(c.size()));
    EXPECT_LT(barrierRtToSrv, readEvent);
}
