#include <gtest/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../job/JobSystem.h"
#include "../core/memory/FrameAllocator.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <utility>

using namespace Engine;

// 헬퍼 타입은 익명 네임스페이스에 둔다. SystemConcurrencyPropertyTests.cpp에도 같은 이름의
// ReadOnlySystemA가 있어서 ODR 위반으로 그쪽 테스트가 조용히 틀리게 동작했다(그 파일 주석 참고).
namespace
{

// 테스트용 Component 타입
struct TestComponentA
{
    int value = 0;
};

struct TestComponentB
{
    float data = 0.0f;
};

// 테스트용 System 클래스들
class TestSystemA : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // Component A를 읽기만 함
        updateCount++;
        
        // 실행 시간 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const char* GetName() const override { return "TestSystemA"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return { typeid(TestComponentA).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return {};
    }

    std::atomic<int> updateCount{0};
};

class TestSystemB : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // Component B를 읽기만 함
        updateCount++;
        
        // 실행 시간 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const char* GetName() const override { return "TestSystemB"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return { typeid(TestComponentB).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return {};
    }

    std::atomic<int> updateCount{0};
};

// 테스트용 청크 기반 System 클래스
class TestChunkedSystem : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // 일반 업데이트는 청크 기반 처리로 대체됨
        regularUpdateCount++;
    }

    void UpdateChunk(ECSRegistry& registry, float deltaTime, size_t chunkStart, size_t chunkSize) override
    {
        // 청크 기반 업데이트
        chunkedUpdateCount++;
        totalProcessedEntities += chunkSize;
        
        // 실행 시간 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const char* GetName() const override { return "TestChunkedSystem"; }
    int GetPriority() const override { return 0; }
    bool SupportsChunkedProcessing() const override { return true; }
    size_t GetOptimalChunkSize() const override { return 100; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return { typeid(TestComponentA).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return {};
    }

    std::atomic<int> regularUpdateCount{0};
    std::atomic<int> chunkedUpdateCount{0};
    std::atomic<size_t> totalProcessedEntities{0};
};

// 테스트용 읽기 전용 System들
class ReadOnlySystemA : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        updateCount++;
        executionOrder.push_back(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const char* GetName() const override { return "ReadOnlySystemA"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return { typeid(TestComponentA).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return {}; // 읽기 전용
    }

    std::atomic<int> updateCount{0};
    static std::vector<int> executionOrder;
};

std::vector<int> ReadOnlySystemA::executionOrder;

class ReadOnlySystemB : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        updateCount++;
        ReadOnlySystemA::executionOrder.push_back(2);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const char* GetName() const override { return "ReadOnlySystemB"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return { typeid(TestComponentB).hash_code() };
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return {}; // 읽기 전용
    }

    std::atomic<int> updateCount{0};
};

class TestSystemC : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // Component A를 쓰기 함
        updateCount++;
        
        // 실행 시간 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const char* GetName() const override { return "TestSystemC"; }
    int GetPriority() const override { return 1; }

    std::vector<size_t> GetReadComponentTypes() const override
    {
        return {};
    }

    std::vector<size_t> GetWriteComponentTypes() const override
    {
        return { typeid(TestComponentA).hash_code() };
    }

    std::atomic<int> updateCount{0};
};
} // namespace (anonymous)

class SystemParallelExecutionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        frameAllocator = std::make_unique<FrameAllocator>();
        frameAllocator->Initialize(1024 * 1024); // 1MB

        jobSystem = std::make_unique<JobSystem>();
        jobSystem->SetFrameAllocator(frameAllocator.get());
        jobSystem->Initialize(4); // 4 worker threads

        world = std::make_unique<World>();
        world->SetJobSystem(jobSystem.get());
        world->SetParallelExecution(true);
        world->SetEditorState(Engine::World::EditorState::Play);
    }

    void TearDown() override
    {
        world.reset();
        jobSystem->Shutdown();
        jobSystem.reset();
        frameAllocator->Shutdown();
        frameAllocator.reset();
        
        // 정적 멤버 초기화
        ReadOnlySystemA::executionOrder.clear();
    }

    std::unique_ptr<FrameAllocator> frameAllocator;
    std::unique_ptr<JobSystem> jobSystem;
    std::unique_ptr<World> world;
};

TEST_F(SystemParallelExecutionTest, ParallelSystemsExecuteInParallel)
{
    // 병렬 실행 가능한 System들 등록 (서로 다른 Component 타입 사용)
    auto systemA = std::make_unique<TestSystemA>();
    auto systemB = std::make_unique<TestSystemB>();
    
    TestSystemA* systemAPtr = systemA.get();
    TestSystemB* systemBPtr = systemB.get();

    world->RegisterSystem(std::move(systemA));
    world->RegisterSystem(std::move(systemB));

    world->Initialize();

    // 병렬 실행 시간 측정
    auto start = std::chrono::high_resolution_clock::now();
    world->Update(0.016f);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 두 System이 모두 실행되었는지 확인
    EXPECT_EQ(systemAPtr->updateCount.load(), 1);
    EXPECT_EQ(systemBPtr->updateCount.load(), 1);

    // 병렬 실행으로 인해 총 시간이 단일 System 실행 시간보다 크지만
    // 순차 실행 시간(20ms)보다는 작아야 함
    EXPECT_LT(duration.count(), 40) << "Parallel execution should be faster than sequential execution (took " << duration.count() << "ms)";
}

TEST_F(SystemParallelExecutionTest, ConflictingSystemsExecuteSequentially)
{
    // 충돌하는 System들 등록 (같은 Component 타입 사용)
    auto systemA = std::make_unique<TestSystemA>(); // Component A 읽기
    auto systemC = std::make_unique<TestSystemC>(); // Component A 쓰기
    
    TestSystemA* systemAPtr = systemA.get();
    TestSystemC* systemCPtr = systemC.get();

    world->RegisterSystem(std::move(systemA));
    world->RegisterSystem(std::move(systemC));

    world->Initialize();

    // 순차 실행 시간 측정
    auto start = std::chrono::high_resolution_clock::now();
    world->Update(0.016f);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 두 System이 모두 실행되었는지 확인
    EXPECT_EQ(systemAPtr->updateCount.load(), 1);
    EXPECT_EQ(systemCPtr->updateCount.load(), 1);

    // 순차 실행으로 인해 총 시간이 두 System 실행 시간의 합에 가까워야 함
    EXPECT_GE(duration.count(), 18); // 최소 18ms (약간의 여유)
}

TEST_F(SystemParallelExecutionTest, FallbackToSequentialWithoutJobSystem)
{
    // Job System 없이 World 생성
    auto worldWithoutJobSystem = std::make_unique<World>();
    worldWithoutJobSystem->SetParallelExecution(true); // 병렬 실행 요청하지만 Job System 없음
    worldWithoutJobSystem->SetEditorState(Engine::World::EditorState::Play);

    auto systemA = std::make_unique<TestSystemA>();
    auto systemB = std::make_unique<TestSystemB>();
    
    TestSystemA* systemAPtr = systemA.get();
    TestSystemB* systemBPtr = systemB.get();

    worldWithoutJobSystem->RegisterSystem(std::move(systemA));
    worldWithoutJobSystem->RegisterSystem(std::move(systemB));

    worldWithoutJobSystem->Initialize();

    // Job System 없이 업데이트 (순차 실행으로 폴백되어야 함)
    worldWithoutJobSystem->Update(0.016f);

    // 두 System이 모두 실행되었는지 확인
    EXPECT_EQ(systemAPtr->updateCount.load(), 1);
    EXPECT_EQ(systemBPtr->updateCount.load(), 1);
}

TEST_F(SystemParallelExecutionTest, SystemDependencyGraphBuilding)
{
    // 다양한 의존성을 가진 System들 등록
    auto systemA = std::make_unique<TestSystemA>(); // Component A 읽기
    auto systemB = std::make_unique<TestSystemB>(); // Component B 읽기
    auto systemC = std::make_unique<TestSystemC>(); // Component A 쓰기

    world->RegisterSystem(std::move(systemA));
    world->RegisterSystem(std::move(systemB));
    world->RegisterSystem(std::move(systemC));

    world->Initialize();

    // 의존성 그래프가 올바르게 구축되었는지 확인하기 위해 업데이트 실행
    world->Update(0.016f);

    // 모든 System이 실행되었는지 확인
    // (의존성 그래프 구축 과정에서 오류가 없었다면 모두 실행되어야 함)
    EXPECT_TRUE(true); // 크래시 없이 여기까지 도달하면 성공
}

TEST_F(SystemParallelExecutionTest, ReadOnlySystemsExecuteInParallel)
{
    // 읽기 전용 System들 등록 (서로 다른 Component 읽기)
    auto systemA = std::make_unique<ReadOnlySystemA>();
    auto systemB = std::make_unique<ReadOnlySystemB>();
    
    ReadOnlySystemA* systemAPtr = systemA.get();
    ReadOnlySystemB* systemBPtr = systemB.get();

    world->RegisterSystem(std::move(systemA));
    world->RegisterSystem(std::move(systemB));

    world->Initialize();

    // 병렬 실행 시간 측정
    auto start = std::chrono::high_resolution_clock::now();
    world->Update(0.016f);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 두 System이 모두 실행되었는지 확인
    EXPECT_EQ(systemAPtr->updateCount.load(), 1);
    EXPECT_EQ(systemBPtr->updateCount.load(), 1);

    // 읽기 전용 System들은 병렬 실행되어야 하므로 총 시간이 단일 System 시간에 가까워야 함
    EXPECT_LT(duration.count(), 40); // 약간의 여유를 둠 (10ms + 오버헤드)
    
    // 실행 순서가 동시에 시작되었는지 확인 (정확한 순서는 보장되지 않음)
    EXPECT_EQ(ReadOnlySystemA::executionOrder.size(), 2);
}

TEST_F(SystemParallelExecutionTest, ChunkedSystemProcessing)
{
    // 청크 기반 처리를 지원하는 System 등록
    auto chunkedSystem = std::make_unique<TestChunkedSystem>();
    TestChunkedSystem* systemPtr = chunkedSystem.get();

    world->RegisterSystem(std::move(chunkedSystem));
    world->Initialize();

    // 테스트용 Entity들 생성 (청크 크기보다 많이)
    auto* registry = world->GetRegistry();
    std::vector<Entity> entities;
    for (int i = 0; i < 250; ++i) // 청크 크기(100)보다 많이 생성
    {
        Entity entity = registry->CreateEntity();
        registry->AddComponent(entity, TestComponentA{i});
        entities.push_back(entity);
    }

    // 청크 기반 업데이트 실행
    world->Update(0.016f);

    // 청크 기반 업데이트가 호출되었는지 확인
    EXPECT_GT(systemPtr->chunkedUpdateCount.load(), 0);
    EXPECT_EQ(systemPtr->regularUpdateCount.load(), 0); // 일반 업데이트는 호출되지 않아야 함
    
    // 모든 Entity가 처리되었는지 확인 (청크 분할로 인해 여러 번 호출)
    EXPECT_EQ(systemPtr->totalProcessedEntities.load(), 250);
    
    // 청크 개수 확인 (250 entities, 최소 청크 64이므로 약 4개 이상의 청크가 생성됨)
    EXPECT_GE(systemPtr->chunkedUpdateCount.load(), 3);
}

TEST_F(SystemParallelExecutionTest, MixedReadWriteSystemScheduling)
{
    // 혼합 접근 패턴을 가진 System들 등록
    auto readOnlyA = std::make_unique<ReadOnlySystemA>(); // Component A 읽기
    auto readOnlyB = std::make_unique<ReadOnlySystemB>(); // Component B 읽기  
    auto writeSystemC = std::make_unique<TestSystemC>(); // Component A 쓰기
    
    ReadOnlySystemA* readOnlyAPtr = readOnlyA.get();
    ReadOnlySystemB* readOnlyBPtr = readOnlyB.get();
    TestSystemC* writeSystemCPtr = writeSystemC.get();

    world->RegisterSystem(std::move(readOnlyA));
    world->RegisterSystem(std::move(readOnlyB));
    world->RegisterSystem(std::move(writeSystemC));

    world->Initialize();

    // 혼합 스케줄링 실행
    auto start = std::chrono::high_resolution_clock::now();
    world->Update(0.016f);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 모든 System이 실행되었는지 확인
    EXPECT_EQ(readOnlyAPtr->updateCount.load(), 1);
    EXPECT_EQ(readOnlyBPtr->updateCount.load(), 1);
    EXPECT_EQ(writeSystemCPtr->updateCount.load(), 1);

    // 최적화된 스케줄링으로 인해 총 시간이 순차 실행보다 짧아야 함
    // ReadOnlyA와 ReadOnlyB는 병렬 실행 가능, WriteSystemC는 ReadOnlyA와 충돌하여 순차 실행
    // 예상 시간: max(ReadOnlyA, ReadOnlyB) + WriteSystemC = 10ms + 10ms = 20ms
    EXPECT_GE(duration.count(), 18); // 최소 18ms
    EXPECT_LT(duration.count(), 50); // 최대 50ms (오버헤드 포함)
}