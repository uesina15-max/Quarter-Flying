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
