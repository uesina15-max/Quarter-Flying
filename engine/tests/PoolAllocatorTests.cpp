// Feature: game-engine-core-systems
// Unit tests for PoolAllocator – validates Requirements 11.1, 11.2

#include <gtest/gtest.h>
#include "core/memory/PoolAllocator.h"
#include <vector>
#include <random>

using namespace Engine;

class PoolAllocatorTest : public ::testing::Test
{
protected:
    static constexpr size_t kElemSize = 32;
    static constexpr size_t kCapacity = 16;

    PoolAllocator pool;

    void SetUp() override   {
        auto result = pool.Initialize(kElemSize, kCapacity);
        ASSERT_TRUE(result.has_value()) << "PoolAllocator initialization failed: " << result.error().message;
    }
    void TearDown() override { pool.Shutdown(); }
};

// --- Basic allocation ---

TEST_F(PoolAllocatorTest, AllocateReturnsNonNull)
{
    void* p = pool.Allocate();
    ASSERT_NE(p, nullptr);
    pool.Free(p);
}

TEST_F(PoolAllocatorTest, AllocateDecreasesFreeCnt)
{
    EXPECT_EQ(pool.GetFreeCount(), kCapacity);
    void* p = pool.Allocate();
    EXPECT_EQ(pool.GetFreeCount(), kCapacity - 1);
    pool.Free(p);
}

TEST_F(PoolAllocatorTest, FreeIncreasesFreeCnt)
{
    void* p = pool.Allocate();
    size_t after_alloc = pool.GetFreeCount();
    pool.Free(p);
    EXPECT_EQ(pool.GetFreeCount(), after_alloc + 1);
}

// --- Exhaustion ---

TEST_F(PoolAllocatorTest, ExhaustPoolReturnsNullptr)
{
    std::vector<void*> ptrs;
    for (size_t i = 0; i < kCapacity; ++i)
        ptrs.push_back(pool.Allocate());

    // Pool is now full – next alloc must return nullptr
    EXPECT_EQ(pool.Allocate(), nullptr);

    for (void* p : ptrs) pool.Free(p);
}

// --- Memory reuse ---

TEST_F(PoolAllocatorTest, FreedBlockIsReused)
{
    void* first = pool.Allocate();
    pool.Free(first);
    void* second = pool.Allocate();
    // Free list is LIFO, so the same block should come back
    EXPECT_EQ(first, second);
    pool.Free(second);
}

// --- Null free ---

TEST_F(PoolAllocatorTest, FreeNullptrIsNoop)
{
    size_t before = pool.GetFreeCount();
    pool.Free(nullptr);
    EXPECT_EQ(pool.GetFreeCount(), before);
}

// --- Capacity / used counts ---

TEST_F(PoolAllocatorTest, UsedCountMatchesAllocations)
{
    void* p1 = pool.Allocate();
    void* p2 = pool.Allocate();
    EXPECT_EQ(pool.GetUsedCount(), 2u);
    pool.Free(p1);
    pool.Free(p2);
    EXPECT_EQ(pool.GetUsedCount(), 0u);
}

// --- Multiple alloc/free cycles ---

TEST_F(PoolAllocatorTest, MultipleAllocFreeCycles)
{
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        std::vector<void*> ptrs;
        for (size_t i = 0; i < kCapacity; ++i)
            ptrs.push_back(pool.Allocate());

        EXPECT_EQ(pool.GetFreeCount(), 0u);

        for (void* p : ptrs) pool.Free(p);

        EXPECT_EQ(pool.GetFreeCount(), kCapacity);
    }
}

// --- Fuzz Testing ---

TEST_F(PoolAllocatorTest, FuzzTest)
{
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> op_dist(0, 1); // 0: Allocate, 1: Free
    
    std::vector<void*> allocated;
    allocated.reserve(kCapacity);
    
    const int num_iterations = 100000;
    
    for (int i = 0; i < num_iterations; ++i)
    {
        bool do_alloc = (op_dist(rng) == 0);
        
        // Force free if full, force alloc if empty
        if (allocated.size() >= kCapacity) do_alloc = false;
        else if (allocated.empty()) do_alloc = true;
        
        if (do_alloc)
        {
            void* p = pool.Allocate();
            ASSERT_NE(p, nullptr);
            allocated.push_back(p);
        }
        else
        {
            std::uniform_int_distribution<size_t> idx_dist(0, allocated.size() - 1);
            size_t idx = idx_dist(rng);
            
            pool.Free(allocated[idx]);
            
            // Swap with back and pop
            allocated[idx] = allocated.back();
            allocated.pop_back();
        }
    }
    
    // Clean up remaining allocations
    for (void* p : allocated)
    {
        pool.Free(p);
    }
    
    EXPECT_EQ(pool.GetUsedCount(), 0u);
    EXPECT_EQ(pool.GetFreeCount(), kCapacity);
}
