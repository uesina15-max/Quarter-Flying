// JobSystemTests.cpp
// Smoke tests for the Job System — validates minimum contract:
//   1. Initialize and Shutdown without crashing.
//   2. A single job dispatched is executed before Wait() returns.
//   3. Shutdown can be called multiple times safely.
//
// Out of scope for this test file (P2/later work):
//   - Fairness / work-stealing quality
//   - Starvation properties
//   - Throughput / performance numbers
//   - Completion ordering guarantees beyond single-job

#include <gtest/gtest.h>
#include "job/JobSystem.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace Engine;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class JobSystemSmokeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Use 2 worker threads so parallelism path is exercised
        // even on machines with few cores.
        jobSystem.Initialize(2);
    }

    void TearDown() override
    {
        jobSystem.Shutdown();
    }

    JobSystem jobSystem;
};

// ---------------------------------------------------------------------------
// Test 1: Initialize / Shutdown — basic lifecycle
// ---------------------------------------------------------------------------

TEST(JobSystemLifecycle, InitializeAndShutdown)
{
    // A fresh JobSystem should initialize and shutdown without crashing.
    JobSystem js;
    EXPECT_NO_THROW(js.Initialize(1));
    EXPECT_NO_THROW(js.Shutdown());
}

// ---------------------------------------------------------------------------
// Test 2: Single job — dispatch and wait
// ---------------------------------------------------------------------------

TEST_F(JobSystemSmokeTest, SingleJobExecuted)
{
    std::atomic<bool> executed{false};

    struct JobData { std::atomic<bool>* flag; };
    auto* data = jobSystem.AllocateJobData<JobData>();
    data->flag = &executed;

    JobHandle handle = jobSystem.Dispatch(
        [](void* rawData) {
            auto* d = static_cast<JobData*>(rawData);
            d->flag->store(true, std::memory_order_release);
        },
        data
    );

    jobSystem.Wait(handle);

    EXPECT_TRUE(executed.load(std::memory_order_acquire))
        << "Job was not executed before Wait() returned";
}

// ---------------------------------------------------------------------------
// Test 3: Worker thread count
// ---------------------------------------------------------------------------

TEST_F(JobSystemSmokeTest, WorkerThreadCountIsPositive)
{
    EXPECT_GT(jobSystem.GetWorkerThreadCount(), 0u);
}

// ---------------------------------------------------------------------------
// Test 4: IsComplete returns true after Wait
// ---------------------------------------------------------------------------

TEST_F(JobSystemSmokeTest, IsCompleteAfterWait)
{
    struct NullData {};
    auto* data = jobSystem.AllocateJobData<NullData>();

    JobHandle handle = jobSystem.Dispatch(
        [](void* /*data*/) { /* no-op */ },
        data
    );

    jobSystem.Wait(handle);
    EXPECT_TRUE(jobSystem.IsComplete(handle));
}

// ── 의존성 디스패치 (DependencyResolver) ─────────────────────────────────────
// 예전에는 이 경로의 테스트가 없었고, 의존성이 있는 잡은 절대 실행되지 않았다(이중 카운트).
// 교착이면 Wait가 영원히 멈추므로 별도 스레드에서 기다리고 제한 시간을 둔다.
#include <future>
#include <vector>

namespace
{
    // handle을 제한 시간 안에 기다린다. 교착이면 false(기다리던 스레드는 버린다).
    bool WaitWithTimeout(Engine::JobSystem& js, Engine::JobHandle h, std::chrono::milliseconds limit)
    {
        auto done = std::make_shared<std::promise<void>>();
        auto fut = done->get_future();
        std::thread([&js, h, done]() { js.Wait(h); done->set_value(); }).detach();
        return fut.wait_for(limit) == std::future_status::ready;
    }

    struct Counter { std::atomic<int> value{0}; std::atomic<int> orderViolations{0}; };

    void Increment(void* data) { static_cast<Counter*>(data)->value.fetch_add(1); }
}

TEST(JobDependencyTest, DependentJobRunsAfterDependency)
{
    Engine::JobSystem js;
    js.Initialize(2);
    struct Data { std::atomic<bool> firstDone{false}; std::atomic<bool> secondSawFirst{false}; } d;
    auto first = js.Dispatch([](void* p) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        static_cast<Data*>(p)->firstDone.store(true);
    }, &d);
    auto second = js.Dispatch([](void* p) {
        auto* dd = static_cast<Data*>(p);
        dd->secondSawFirst.store(dd->firstDone.load());
    }, &d, &first, 1);
    ASSERT_TRUE(second.IsValid());
    ASSERT_TRUE(WaitWithTimeout(js, second, std::chrono::milliseconds(5000))) << "의존성 잡이 실행되지 않음(교착)";
    EXPECT_TRUE(d.secondSawFirst.load());
    js.Shutdown();
}

TEST(JobDependencyTest, DependencyAlreadyCompletedAndRetired_StillRuns)
{
    Engine::JobSystem js;
    js.Initialize(2);
    Counter c;
    auto first = js.Dispatch(Increment, &c);
    js.Wait(first);                                    // 끝나서 회수된 뒤
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto second = js.Dispatch(Increment, &c, &first, 1);
    ASSERT_TRUE(second.IsValid()) << "이미 끝난 선행 잡을 이유로 디스패치가 실패하면 안 된다";
    ASSERT_TRUE(WaitWithTimeout(js, second, std::chrono::milliseconds(5000)));
    EXPECT_EQ(c.value.load(), 2);
    js.Shutdown();
}

TEST(JobDependencyTest, ManyChains_NoDeadlock_NoDoubleExecution)
{
    // 체인 200개: a, b(a에 의존), tail(a, b에 의존). 잡마다 자기 슬롯을 올려서, 실패 시 어떤 역할의 잡이
    // 빠졌는지(유실) 또는 두 번 돌았는지(중복)를 보고한다.
    Engine::JobSystem js;
    js.Initialize(4);
    constexpr int kChains = 200;
    static std::atomic<int> slots[kChains * 3];
    for (auto& v : slots) v.store(0);
    auto bump = [](void* p) { static_cast<std::atomic<int>*>(p)->fetch_add(1); };

    std::vector<Engine::JobHandle> tails;
    for (int i = 0; i < kChains; ++i)
    {
        auto a = js.Dispatch(bump, &slots[i * 3 + 0]);
        auto b = js.Dispatch(bump, &slots[i * 3 + 1], &a, 1);
        Engine::JobHandle ab[2] = {a, b};
        tails.push_back(js.Dispatch(bump, &slots[i * 3 + 2], ab, 2));
        ASSERT_TRUE(a.IsValid() && b.IsValid() && tails.back().IsValid());
    }
    for (auto& t : tails)
    {
        ASSERT_TRUE(WaitWithTimeout(js, t, std::chrono::milliseconds(5000))) << "체인 교착";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string report;
    int missing = 0, duplicated = 0;
    const char* role[3] = {"a", "b", "tail"};
    for (int i = 0; i < kChains * 3; ++i)
    {
        const int n = slots[i].load();
        if (n != 1)
        {
            (n == 0 ? missing : duplicated)++;
            if (report.size() < 300)
                report += " chain" + std::to_string(i / 3) + "." + role[i % 3] + "=" + std::to_string(n);
        }
    }
    EXPECT_EQ(missing, 0) << report;
    EXPECT_EQ(duplicated, 0) << report;
    js.Shutdown();
}
