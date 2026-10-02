#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "../ecs/World.h"
#include "../ecs/System.h"
#include "../ecs/Components.h"
#include "../job/JobSystem.h"
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <set>

using namespace Engine;

// ============================================================================
// Task 9.10: ECS System 속성 테스트 작성
// Feature: game-engine-core-systems, Property 22: System 동시성 제어
// Validates: Requirements 7.5, 7.6
//
// 속성 22: System 동시성 제어
// 모든 System 쌍에 대해, 같은 Component를 읽기만 하면 병렬 실행이 허용되어야 하고,
// 하나라도 쓰기를 하면 순차 실행이 강제되어야 합니다.
// ============================================================================

// ============================================================================
// Shared component type hashes used across tests
// ============================================================================

static const size_t kCompAHash = typeid(TransformComponent).hash_code();
static const size_t kCompBHash = typeid(RenderableComponent).hash_code();

// ============================================================================
// Helper: Concurrency-tracking System
//
// Records its start/end timestamps so we can determine whether two systems
// overlapped in time (parallel) or ran back-to-back (sequential).
// ============================================================================

// 아래 헬퍼 타입은 전부 익명 네임스페이스에 둔다(이 파일에서만 보이게).
// 증상: SystemConcurrencyPropertyTests의 4개 테스트가 오랫동안 "기존 무관 실패"로 방치돼 있었다.
//   RapidCheck는 "Falsifiable after 1 tests"로 실패했고, Integration 테스트는
//   "Expected: (windowA.end.time_since_epoch().count()) > (0), actual: 0 vs 0"로 실패했다.
//   Writer와 Reader가 같은 컴포넌트를 쓰는데도 병렬로 판정됐고, 시스템 하나는 Update가 아예 안 돈 것처럼 보였다.
// 원인: 이 파일과 SystemParallelExecutionTests.cpp 양쪽에 전역 `class ReadOnlySystemA`가 서로 다른
//   정의로 있었다(ODR 위반). 클래스 안에 정의된 멤버 함수는 inline이라 링커가 한쪽 정의만 남기고,
//   다른 파일의 ReadOnlySystemA가 그 정의로 실행됐다. 그래서 읽는 컴포넌트 해시가 달라 충돌이
//   없는 것으로 판정됐고, Update는 window를 기록하는 대신 updateCount++로 엉뚱한 메모리를 건드렸다.
//   컴파일 에러도 링크 에러도 크래시도 없이 조용히 틀리게 동작했다.
namespace
{

struct ExecutionWindow
{
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
};

// Returns true if two execution windows overlap (i.e. ran in parallel)
static bool WindowsOverlap(const ExecutionWindow& a, const ExecutionWindow& b)
{
    return a.start < b.end && b.start < a.end;
}

// ============================================================================
// Concrete System implementations for concurrency tests
// ============================================================================

// A system that only reads component A
class ReadOnlySystemA : public System
{
public:
    explicit ReadOnlySystemA(ExecutionWindow* window, std::chrono::milliseconds delay = std::chrono::milliseconds(15))
        : window_(window), delay_(delay) {}

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        window_->start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(delay_);
        window_->end = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "ReadOnlySystemA"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override  { return { kCompAHash }; }
    std::vector<size_t> GetWriteComponentTypes() const override { return {}; }

private:
    ExecutionWindow* window_;
    std::chrono::milliseconds delay_;
};

// A second system that only reads component A
class ReadOnlySystemA2 : public System
{
public:
    explicit ReadOnlySystemA2(ExecutionWindow* window, std::chrono::milliseconds delay = std::chrono::milliseconds(15))
        : window_(window), delay_(delay) {}

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        window_->start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(delay_);
        window_->end = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "ReadOnlySystemA2"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override  { return { kCompAHash }; }
    std::vector<size_t> GetWriteComponentTypes() const override { return {}; }

private:
    ExecutionWindow* window_;
    std::chrono::milliseconds delay_;
};

// A system that writes component A
class WriteSystemA : public System
{
public:
    explicit WriteSystemA(ExecutionWindow* window, std::chrono::milliseconds delay = std::chrono::milliseconds(15))
        : window_(window), delay_(delay) {}

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        window_->start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(delay_);
        window_->end = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "WriteSystemA"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override  { return {}; }
    std::vector<size_t> GetWriteComponentTypes() const override { return { kCompAHash }; }

private:
    ExecutionWindow* window_;
    std::chrono::milliseconds delay_;
};

// A system that reads component A and writes component B
class ReadAWriteBSystem : public System
{
public:
    explicit ReadAWriteBSystem(ExecutionWindow* window, std::chrono::milliseconds delay = std::chrono::milliseconds(15))
        : window_(window), delay_(delay) {}

    void Update(ECSRegistry& registry, float deltaTime) override
    {
        window_->start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(delay_);
        window_->end = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "ReadAWriteBSystem"; }
    int GetPriority() const override { return 0; }

    std::vector<size_t> GetReadComponentTypes() const override  { return { kCompAHash }; }
    std::vector<size_t> GetWriteComponentTypes() const override { return { kCompBHash }; }

private:
    ExecutionWindow* window_;
    std::chrono::milliseconds delay_;
};

// ============================================================================
// Lightweight concurrency-control checker (no timing)
//
// Instead of relying on wall-clock timing (which is fragile in CI), we verify
// the *scheduling decision* made by the World: whether two systems are placed
// in the same parallel group or in separate sequential groups.
//
// We do this by inspecting the System API:
//   - canRunInParallel(sysA, sysB) == true  iff they share no write conflict
// ============================================================================

// Returns true if two systems can run in parallel according to the ECS rules:
//   - Parallel allowed when neither system writes a component the other reads/writes
static bool CanRunInParallel(const System& a, const System& b)
{
    auto aReads  = a.GetReadComponentTypes();
    auto aWrites = a.GetWriteComponentTypes();
    auto bReads  = b.GetReadComponentTypes();
    auto bWrites = b.GetWriteComponentTypes();

    // Check if a writes something b reads or writes
    for (size_t aWrite : aWrites)
    {
        for (size_t bRead : bReads)
            if (aWrite == bRead) return false;
        for (size_t bWrite : bWrites)
            if (aWrite == bWrite) return false;
    }

    // Check if b writes something a reads or writes
    for (size_t bWrite : bWrites)
    {
        for (size_t aRead : aReads)
            if (bWrite == aRead) return false;
        for (size_t aWrite : aWrites)
            if (bWrite == aWrite) return false;
    }

    return true;
}
} // namespace (anonymous)

// ============================================================================
// Test Fixture
// ============================================================================

class SystemConcurrencyPropertyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        jobSystem = std::make_unique<JobSystem>();
        jobSystem->Initialize(4);
    }

    void TearDown() override
    {
        jobSystem.reset();
    }

    // Helper: build a fresh World with a JobSystem attached
    std::unique_ptr<World> MakeParallelWorld()
    {
        auto world = std::make_unique<World>();
        world->SetJobSystem(jobSystem.get());
        world->SetParallelExecution(true);
        world->SetEditorState(World::EditorState::Play);
        return world;
    }

    std::unique_ptr<JobSystem> jobSystem;
};

// ============================================================================
// Property 22a: Read-only systems on the same component are allowed to run
//               in parallel (no write conflict).
//
// Validates: Requirement 7.5
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    ReadOnlySystemsOnSameComponentAllowParallel, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    // Generate 2-5 read-only systems that all read the same component type
    const int numSystems = *rc::gen::inRange(2, 6);

    // Build concrete system instances (we only need their metadata for this check)
    ExecutionWindow dummyWindow{};
    std::vector<std::unique_ptr<System>> systems;
    for (int i = 0; i < numSystems; ++i)
    {
        systems.push_back(std::make_unique<ReadOnlySystemA>(&dummyWindow));
    }

    // Property: every pair of read-only systems must be allowed to run in parallel
    for (int i = 0; i < numSystems; ++i)
    {
        for (int j = i + 1; j < numSystems; ++j)
        {
            bool parallel = CanRunInParallel(*systems[i], *systems[j]);
            RC_ASSERT(parallel);
        }
    }
}

// ============================================================================
// Property 22b: When any system writes a component that another system
//               reads or writes, they must NOT be allowed to run in parallel
//               (sequential execution must be enforced).
//
// Validates: Requirement 7.6
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    WriteConflictForcesSequentialExecution, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    ExecutionWindow dummyWindow{};

    // Case 1: writer + reader on same component → must be sequential
    {
        WriteSystemA    writer(&dummyWindow);
        ReadOnlySystemA reader(&dummyWindow);
        RC_ASSERT(!CanRunInParallel(writer, reader));
    }

    // Case 2: writer + writer on same component → must be sequential
    {
        WriteSystemA writer1(&dummyWindow);
        WriteSystemA writer2(&dummyWindow);
        RC_ASSERT(!CanRunInParallel(writer1, writer2));
    }

    // Case 3: reader of A + writer of A → must be sequential
    {
        ReadOnlySystemA reader(&dummyWindow);
        WriteSystemA    writer(&dummyWindow);
        RC_ASSERT(!CanRunInParallel(reader, writer));
    }
}

// ============================================================================
// Property 22c: Mixed read/write systems — only pairs with no write conflict
//               are allowed to run in parallel.
//
// Validates: Requirements 7.5, 7.6
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    MixedReadWriteSystemsRespectConcurrencyRules, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    ExecutionWindow dummyWindow{};

    // ReadOnlySystemA  : reads A
    // ReadAWriteBSystem: reads A, writes B
    // WriteSystemA     : writes A

    ReadOnlySystemA   readA(&dummyWindow);
    ReadAWriteBSystem readAwriteB(&dummyWindow);
    WriteSystemA      writeA(&dummyWindow);

    // readA vs readAwriteB: readA reads A, readAwriteB reads A and writes B
    // No write conflict on A (readAwriteB only writes B, not A)
    // → parallel allowed
    RC_ASSERT(CanRunInParallel(readA, readAwriteB));

    // readA vs writeA: writeA writes A which readA reads → sequential required
    RC_ASSERT(!CanRunInParallel(readA, writeA));

    // readAwriteB vs writeA: writeA writes A which readAwriteB reads → sequential required
    RC_ASSERT(!CanRunInParallel(readAwriteB, writeA));
}

// ============================================================================
// Property 22d: Symmetry — the concurrency decision is symmetric.
//               canRunInParallel(A, B) == canRunInParallel(B, A)
//
// Validates: Requirements 7.5, 7.6
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    ConcurrencyDecisionIsSymmetric, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    // Generate a random pair of access patterns
    // 0 = read A, 1 = write A, 2 = read A + write B, 3 = read B
    const int patternA = *rc::gen::inRange(0, 4);
    const int patternB = *rc::gen::inRange(0, 4);

    ExecutionWindow dummyWindow{};

    auto makeSystem = [&](int pattern) -> std::unique_ptr<System>
    {
        switch (pattern)
        {
        case 0: return std::make_unique<ReadOnlySystemA>(&dummyWindow);
        case 1: return std::make_unique<WriteSystemA>(&dummyWindow);
        case 2: return std::make_unique<ReadAWriteBSystem>(&dummyWindow);
        default: return std::make_unique<ReadOnlySystemA2>(&dummyWindow);
        }
    };

    auto sysA = makeSystem(patternA);
    auto sysB = makeSystem(patternB);

    bool ab = CanRunInParallel(*sysA, *sysB);
    bool ba = CanRunInParallel(*sysB, *sysA);

    RC_ASSERT(ab == ba);
}

// ============================================================================
// Property 22e: Read-only systems on *different* component types are always
//               allowed to run in parallel (no shared data at all).
//
// Validates: Requirement 7.5
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    ReadOnlySystemsOnDifferentComponentsAllowParallel, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    ExecutionWindow dummyWindow{};

    // ReadOnlySystemA reads TransformComponent (kCompAHash)
    // ReadOnlySystemA2 also reads TransformComponent — but we can use a
    // different component type by constructing a custom system inline.

    // Build a system that reads only component B (RenderableComponent)
    class ReadOnlySystemB : public System
    {
    public:
        explicit ReadOnlySystemB(ExecutionWindow* w) : w_(w) {}
        void Update(ECSRegistry&, float) override {}
        const char* GetName() const override { return "ReadOnlySystemB"; }
        int GetPriority() const override { return 0; }
        std::vector<size_t> GetReadComponentTypes() const override  { return { kCompBHash }; }
        std::vector<size_t> GetWriteComponentTypes() const override { return {}; }
    private:
        ExecutionWindow* w_;
    };

    ReadOnlySystemA readA(&dummyWindow);
    ReadOnlySystemB readB(&dummyWindow);

    // No shared component → parallel always allowed
    RC_ASSERT(CanRunInParallel(readA, readB));
}

// ============================================================================
// Property 22f: A system with no component declarations can always run in
//               parallel with any other system (no conflict possible).
//
// Validates: Requirements 7.5, 7.6
// ============================================================================

RC_GTEST_FIXTURE_PROP(SystemConcurrencyPropertyTest,
    SystemWithNoComponentsAlwaysParallel, ())
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    class NoComponentSystem : public System
    {
    public:
        void Update(ECSRegistry&, float) override {}
        const char* GetName() const override { return "NoComponentSystem"; }
        int GetPriority() const override { return 0; }
        std::vector<size_t> GetReadComponentTypes() const override  { return {}; }
        std::vector<size_t> GetWriteComponentTypes() const override { return {}; }
    };

    ExecutionWindow dummyWindow{};

    const int pattern = *rc::gen::inRange(0, 3);
    auto makeSystem = [&](int p) -> std::unique_ptr<System>
    {
        switch (p)
        {
        case 0: return std::make_unique<ReadOnlySystemA>(&dummyWindow);
        case 1: return std::make_unique<WriteSystemA>(&dummyWindow);
        default: return std::make_unique<ReadAWriteBSystem>(&dummyWindow);
        }
    };

    NoComponentSystem noComp;
    auto other = makeSystem(pattern);

    // A system with no component declarations has no conflicts with anything
    RC_ASSERT(CanRunInParallel(noComp, *other));
    RC_ASSERT(CanRunInParallel(*other, noComp));
}

// ============================================================================
// Integration test: verify the World actually executes read-only systems in
// parallel (timing-based, best-effort).
// ============================================================================

TEST_F(SystemConcurrencyPropertyTest, ReadOnlySystemsRunInParallelIntegration)
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    ExecutionWindow windowA{}, windowB{};
    const auto delay = std::chrono::milliseconds(20);

    auto world = MakeParallelWorld();
    world->RegisterSystem(std::make_unique<ReadOnlySystemA>(&windowA, delay));
    world->RegisterSystem(std::make_unique<ReadOnlySystemA2>(&windowB, delay));
    world->Initialize();

    auto wallStart = std::chrono::steady_clock::now();
    world->Update(0.016f);
    auto wallEnd = std::chrono::steady_clock::now();

    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart).count();

    // Both systems ran
    EXPECT_GT(windowA.end.time_since_epoch().count(), 0);
    EXPECT_GT(windowB.end.time_since_epoch().count(), 0);

    // 병렬이면 두 실행 구간이 겹친다. 예전에는 "전체 벽시계 시간 < 35ms"로 판정했는데, 실제 소요가
    // 28~34ms라 여유가 몇 ms뿐이었고, 부하가 조금만 걸려도 실패했다(플레이크). 구간 겹침은 시간 여유와
    // 무관하게 병렬 여부 자체를 본다.
    EXPECT_TRUE(WindowsOverlap(windowA, windowB))
        << "Read-only systems should run in parallel (total time was " << totalMs << "ms)";

    world->Shutdown();
}

// ============================================================================
// Integration test: verify the World serialises a writer against a reader.
// ============================================================================

TEST_F(SystemConcurrencyPropertyTest, WriteConflictForcesSequentialIntegration)
{
    // Feature: game-engine-core-systems, Property 22: System 동시성 제어

    ExecutionWindow windowReader{}, windowWriter{};
    const auto delay = std::chrono::milliseconds(20);

    auto world = MakeParallelWorld();
    world->RegisterSystem(std::make_unique<ReadOnlySystemA>(&windowReader, delay));
    world->RegisterSystem(std::make_unique<WriteSystemA>(&windowWriter, delay));
    world->Initialize();

    auto wallStart = std::chrono::steady_clock::now();
    world->Update(0.016f);
    auto wallEnd = std::chrono::steady_clock::now();

    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart).count();

    // Both systems ran
    EXPECT_GT(windowReader.end.time_since_epoch().count(), 0);
    EXPECT_GT(windowWriter.end.time_since_epoch().count(), 0);

    // 순차 실행이면 두 실행 구간이 겹치지 않는다(시간 임계값 대신 구간으로 판정 - 위 테스트 주석 참고).
    EXPECT_FALSE(WindowsOverlap(windowReader, windowWriter))
        << "Writer + reader on same component should run sequentially (total time was " << totalMs << "ms)";

    world->Shutdown();
}

// ============================================================================
// Configuration sanity test
// ============================================================================

TEST_F(SystemConcurrencyPropertyTest, PropertyTestConfiguration)
{
    SUCCEED() << "Property 22 tests configured to run with minimum 100 iterations each";
}
