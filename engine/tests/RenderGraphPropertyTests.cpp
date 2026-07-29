// Feature: game-engine-core-systems, Property 24: RenderGraph Pass 관계 기록
// 모든 Pass에 대해, 추가 시 선언된 읽기/쓰기 리소스 관계가 정확하게 기록되어야 합니다.
// 검증: 요구사항 8.3

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "../renderer/RenderGraph.h"
#include <vector>
#include <string>
#include <algorithm>

using namespace Engine;

/**
 * Property 24: RenderGraph Pass 관계 기록
 * **Validates: Requirements 8.3**
 *
 * 모든 Pass에 대해, 추가 시 선언된 읽기/쓰기 리소스 관계가 정확하게 기록되어야 합니다.
 * WHEN Pass가 추가될 때, THE RenderGraph SHALL Pass의 읽기/쓰기 관계를 기록해야 합니다.
 */
RC_GTEST_PROP(RenderGraphPassRelationshipProperty, ReadWriteRelationshipsRecorded,
              ())
{
    // Constrain: 1~8개의 패스, 각 설정은 0~3 (0=읽기, 1=쓰기, 2=읽기+쓰기, 3=쓰기만)
    const int generatedPassCount = *rc::gen::inRange(1, 9);
    std::vector<int> passConfigs;
    passConfigs.reserve(generatedPassCount);
    for (int i = 0; i < generatedPassCount; ++i)
    {
        passConfigs.push_back(*rc::gen::inRange(0, 4));
    }

    RenderGraph graph;
    RGTextureDesc desc(1920, 1080, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    // 각 패스마다 독립적인 리소스를 생성 (사이클 방지)
    const size_t numPasses = passConfigs.size();
    std::vector<std::string> resourceNames;
    for (size_t i = 0; i < numPasses; ++i)
    {
        std::string name = "Tex" + std::to_string(i);
        resourceNames.push_back(name);
        graph.CreateTexture(name, desc);
    }

    // 각 패스에서 선언한 읽기/쓰기 리소스를 추적
    struct PassExpectation
    {
        std::vector<std::string> expectedReads;
        std::vector<std::string> expectedWrites;
    };
    std::vector<PassExpectation> expectations(numPasses);

    for (size_t i = 0; i < numPasses; ++i)
    {
        int config = passConfigs[i];
        std::string passName = "Pass" + std::to_string(i);
        std::string texName = resourceNames[i];

        // config에 따라 읽기/쓰기 선언 결정
        bool doRead  = (config == 0 || config == 2);
        bool doWrite = (config == 1 || config == 2 || config == 3);

        if (doRead)  expectations[i].expectedReads.push_back(texName);
        if (doWrite) expectations[i].expectedWrites.push_back(texName);

        graph.AddPass(passName,
            [doRead, doWrite, texName](RGBuilder& builder) {
                if (doRead)  builder.ReadTexture(texName);
                if (doWrite) builder.WriteTexture(texName);
            },
            [](CommandList&) {});
    }

    // 패스 수가 정확히 기록되었는지 확인
    RC_ASSERT(graph.GetPassCount() == numPasses);

    // 각 패스의 읽기/쓰기 관계가 정확히 기록되었는지 검증
    for (size_t i = 0; i < numPasses; ++i)
    {
        const RGPass& pass = graph.GetPass(i);
        const PassExpectation& exp = expectations[i];

        // 읽기 리소스 수 검증
        RC_ASSERT(pass.reads.size() == exp.expectedReads.size());

        // 쓰기 리소스 수 검증
        RC_ASSERT(pass.writes.size() == exp.expectedWrites.size());

        // 읽기 리소스 핸들이 올바른 텍스처를 가리키는지 검증
        for (const std::string& readName : exp.expectedReads)
        {
            RGTextureHandle expectedHandle = graph.GetResourceHandle(readName);
            RC_ASSERT(expectedHandle.IsValid());
            bool found = std::find(pass.reads.begin(), pass.reads.end(), expectedHandle) != pass.reads.end();
            RC_ASSERT(found);
        }

        // 쓰기 리소스 핸들이 올바른 텍스처를 가리키는지 검증
        for (const std::string& writeName : exp.expectedWrites)
        {
            RGTextureHandle expectedHandle = graph.GetResourceHandle(writeName);
            RC_ASSERT(expectedHandle.IsValid());
            bool found = std::find(pass.writes.begin(), pass.writes.end(), expectedHandle) != pass.writes.end();
            RC_ASSERT(found);
        }
    }
}

/**
 * Property 24 Extended: 다중 리소스 읽기/쓰기 관계 기록
 * **Validates: Requirements 8.3**
 *
 * 하나의 Pass가 여러 리소스를 읽거나 쓸 때, 모든 관계가 빠짐없이 기록되어야 합니다.
 */
RC_GTEST_PROP(RenderGraphPassRelationshipProperty, MultipleResourceRelationshipsRecorded,
              ())
{
    // 1~4개의 패스, 각 패스당 읽기/쓰기 리소스 수 1~3개
    const int generatedPassCount = *rc::gen::inRange(1, 5);
    std::vector<int> readCounts;
    std::vector<int> writeCounts;
    readCounts.reserve(generatedPassCount);
    writeCounts.reserve(generatedPassCount);
    for (int i = 0; i < generatedPassCount; ++i)
    {
        readCounts.push_back(*rc::gen::inRange(0, 4));
        writeCounts.push_back(*rc::gen::inRange(0, 4));
    }

    RenderGraph graph;
    RGTextureDesc desc(512, 512, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    const size_t numPasses = readCounts.size();

    // 모든 패스에서 사용할 리소스를 미리 생성
    // 각 패스마다 최대 readCount + writeCount 개의 독립 리소스 필요
    std::vector<std::vector<std::string>> passReadResources(numPasses);
    std::vector<std::vector<std::string>> passWriteResources(numPasses);

    size_t resourceCounter = 0;
    for (size_t i = 0; i < numPasses; ++i)
    {
        for (int r = 0; r < readCounts[i]; ++r)
        {
            std::string name = "ReadTex_" + std::to_string(resourceCounter++);
            graph.CreateTexture(name, desc);
            passReadResources[i].push_back(name);
        }
        for (int w = 0; w < writeCounts[i]; ++w)
        {
            std::string name = "WriteTex_" + std::to_string(resourceCounter++);
            graph.CreateTexture(name, desc);
            passWriteResources[i].push_back(name);
        }
    }

    // 패스 추가
    for (size_t i = 0; i < numPasses; ++i)
    {
        std::string passName = "Pass" + std::to_string(i);
        const auto& reads  = passReadResources[i];
        const auto& writes = passWriteResources[i];

        graph.AddPass(passName,
            [reads, writes](RGBuilder& builder) {
                for (const auto& name : reads)  builder.ReadTexture(name);
                for (const auto& name : writes) builder.WriteTexture(name);
            },
            [](CommandList&) {});
    }

    RC_ASSERT(graph.GetPassCount() == numPasses);

    // 각 패스의 읽기/쓰기 관계 검증
    for (size_t i = 0; i < numPasses; ++i)
    {
        const RGPass& pass = graph.GetPass(i);

        // 읽기 리소스 수 검증
        RC_ASSERT(pass.reads.size() == passReadResources[i].size());

        // 쓰기 리소스 수 검증
        RC_ASSERT(pass.writes.size() == passWriteResources[i].size());

        // 각 읽기 리소스 핸들 검증
        for (const std::string& name : passReadResources[i])
        {
            RGTextureHandle handle = graph.GetResourceHandle(name);
            RC_ASSERT(handle.IsValid());
            RC_ASSERT(std::find(pass.reads.begin(), pass.reads.end(), handle) != pass.reads.end());
        }

        // 각 쓰기 리소스 핸들 검증
        for (const std::string& name : passWriteResources[i])
        {
            RGTextureHandle handle = graph.GetResourceHandle(name);
            RC_ASSERT(handle.IsValid());
            RC_ASSERT(std::find(pass.writes.begin(), pass.writes.end(), handle) != pass.writes.end());
        }
    }
}

/**
 * Property 24 Reset: Reset 후 새로 추가된 Pass의 관계도 정확히 기록
 * **Validates: Requirements 8.3**
 *
 * RenderGraph를 Reset한 후 새로 추가된 Pass의 읽기/쓰기 관계도 정확히 기록되어야 합니다.
 */
RC_GTEST_PROP(RenderGraphPassRelationshipProperty, RelationshipsRecordedAfterReset,
              ())
{
    const int generatedPassCount = *rc::gen::inRange(1, 7);
    std::vector<int> passConfigs;
    passConfigs.reserve(generatedPassCount);
    for (int i = 0; i < generatedPassCount; ++i)
    {
        passConfigs.push_back(*rc::gen::inRange(0, 3));
    }

    RenderGraph graph;
    RGTextureDesc desc(256, 256, TextureFormat::RGBA8, TextureUsage::RenderTarget);

    // 첫 번째 라운드: 임의 패스 추가 후 Reset
    graph.CreateTexture("TempTex", desc);
    graph.AddPass("TempPass",
        [](RGBuilder& builder) { builder.WriteTexture("TempTex"); },
        [](CommandList&) {});
    graph.Reset();

    // Reset 후 상태 확인
    RC_ASSERT(graph.GetPassCount() == 0);
    RC_ASSERT(graph.GetResourceCount() == 0);

    // 두 번째 라운드: 새 패스 추가
    const size_t numPasses = passConfigs.size();
    std::vector<std::string> resourceNames;
    for (size_t i = 0; i < numPasses; ++i)
    {
        std::string name = "Tex" + std::to_string(i);
        resourceNames.push_back(name);
        graph.CreateTexture(name, desc);
    }

    struct PassExpectation
    {
        std::vector<std::string> expectedReads;
        std::vector<std::string> expectedWrites;
    };
    std::vector<PassExpectation> expectations(numPasses);

    for (size_t i = 0; i < numPasses; ++i)
    {
        int config = passConfigs[i];
        bool doRead  = (config == 0 || config == 2);
        bool doWrite = (config == 1 || config == 2);

        // config==0이면 읽기만, config==1이면 쓰기만, config==2이면 둘 다
        // 읽기만인 경우 쓰기 없이 읽기만 허용 (RGBuilder::ReadTexture는 기존 리소스 필요)
        if (doRead)  expectations[i].expectedReads.push_back(resourceNames[i]);
        if (doWrite) expectations[i].expectedWrites.push_back(resourceNames[i]);

        // 읽기만인 경우 쓰기가 없으면 lifetime이 기록되지 않을 수 있으므로
        // 읽기만 패스는 쓰기 패스가 먼저 있어야 하지만, 여기서는 단순히 관계 기록만 검증
        graph.AddPass("Pass" + std::to_string(i),
            [doRead, doWrite, i, &resourceNames](RGBuilder& builder) {
                if (doRead)  builder.ReadTexture(resourceNames[i]);
                if (doWrite) builder.WriteTexture(resourceNames[i]);
            },
            [](CommandList&) {});
    }

    RC_ASSERT(graph.GetPassCount() == numPasses);

    for (size_t i = 0; i < numPasses; ++i)
    {
        const RGPass& pass = graph.GetPass(i);
        const PassExpectation& exp = expectations[i];

        RC_ASSERT(pass.reads.size()  == exp.expectedReads.size());
        RC_ASSERT(pass.writes.size() == exp.expectedWrites.size());

        for (const std::string& name : exp.expectedReads)
        {
            RGTextureHandle handle = graph.GetResourceHandle(name);
            RC_ASSERT(handle.IsValid());
            RC_ASSERT(std::find(pass.reads.begin(), pass.reads.end(), handle) != pass.reads.end());
        }

        for (const std::string& name : exp.expectedWrites)
        {
            RGTextureHandle handle = graph.GetResourceHandle(name);
            RC_ASSERT(handle.IsValid());
            RC_ASSERT(std::find(pass.writes.begin(), pass.writes.end(), handle) != pass.writes.end());
        }
    }
}
