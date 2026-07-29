// Feature: game-engine-core-systems
// Unit tests for PoolAllocator and StackAllocator
// Validates Requirements 11.2, 11.3

#include <gtest/gtest.h>
#include "core/memory/PoolAllocator.h"
#include "core/memory/StackAllocator.h"
#include <vector>
#include <cstdint>

using namespace Engine;

// ============================================================
// PoolAllocator Unit Tests
// Requirement 11.2
// ============================================================

class PoolAllocatorSystemTest : public ::testing::Test
{
protected:
    static constexpr size_t kElemSize = 64;
    static constexpr size_t kCapacity = 8;

    PoolAllocator pool;

    void SetUp() override   {
        auto result = pool.Initialize(kElemSize, kCapacity);
        ASSERT_TRUE(result.has_value()) << "PoolAllocator initialization failed: " << result.error().message;
    }
    void TearDown() override { pool.Shutdown(); }
};

// Requirement 11.2: Basic allocation returns non-null pointer
TEST_F(PoolAllocatorSystemTest, BasicAllocationReturnsNonNull)
{
    void* p = pool.Allocate();
    ASSERT_NE(p, nullptr);
    pool.Free(p);
}

// Requirement 11.2: Allocated memory is writable and readable
TEST_F(PoolAllocatorSystemTest, AllocatedMemoryIsWritableAndReadable)
{
    uint8_t* p = static_cast<uint8_t*>(pool.Allocate());
    ASSERT_NE(p, nullptr);

    // Write a pattern across the full element size
    for (size_t i = 0; i < kElemSize; ++i)
        p[i] = static_cast<uint8_t>(i & 0xFF);

    // Read back and verify
    for (size_t i = 0; i < kElemSize; ++i)
        EXPECT_EQ(p[i], static_cast<uint8_t>(i & 0xFF));

    pool.Free(p);
}

// Requirement 11.2: Free makes memory available for reallocation
TEST_F(PoolAllocatorSystemTest, FreeIncreasesAvailableCount)
{
    void* p = pool.Allocate();
    size_t usedAfterAlloc = pool.GetUsedCount();
    pool.Free(p);
    EXPECT_EQ(pool.GetUsedCount(), usedAfterAlloc - 1);
}

// Requirement 11.2: Multiple allocations return different pointers
TEST_F(PoolAllocatorSystemTest, MultipleAllocationsReturnDifferentPointers)
{
    void* p1 = pool.Allocate();
    void* p2 = pool.Allocate();
    void* p3 = pool.Allocate();

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    EXPECT_NE(p1, p2);
    EXPECT_NE(p2, p3);
    EXPECT_NE(p1, p3);

    pool.Free(p1);
    pool.Free(p2);
    pool.Free(p3);
}

// Requirement 11.2: Allocation beyond capacity returns nullptr
TEST_F(PoolAllocatorSystemTest, AllocationBeyondCapacityReturnsNull)
{
    std::vector<void*> ptrs;
    for (size_t i = 0; i < kCapacity; ++i)
        ptrs.push_back(pool.Allocate());

    // Pool exhausted – next allocation must return nullptr
    void* overflow = pool.Allocate();
    EXPECT_EQ(overflow, nullptr);

    for (void* p : ptrs) pool.Free(p);
}

// Requirement 11.2: Free list reuse – after freeing, next allocation reuses the slot
TEST_F(PoolAllocatorSystemTest, FreeListReuseAfterFree)
{
    void* first = pool.Allocate();
    ASSERT_NE(first, nullptr);
    pool.Free(first);

    // Free list is LIFO, so the same block should be returned
    void* second = pool.Allocate();
    EXPECT_EQ(first, second);

    pool.Free(second);
}

// ============================================================
// StackAllocator Unit Tests
// Requirement 11.3
// ============================================================

class StackAllocatorSystemTest : public ::testing::Test
{
protected:
    static constexpr size_t kBufSize = 2048;

    StackAllocator stack;

    void SetUp() override   {
        auto result = stack.Initialize(kBufSize);
        ASSERT_TRUE(result.has_value()) << "StackAllocator initialization failed: " << result.error().message;
    }
    void TearDown() override { stack.Shutdown(); }
};

// Requirement 11.3: Basic allocation returns non-null pointer
TEST_F(StackAllocatorSystemTest, BasicAllocationReturnsNonNull)
{
    void* p = stack.Allocate(64);
    EXPECT_NE(p, nullptr);
}

// Requirement 11.3: Marker-based deallocation
// Allocate, get marker, allocate more, pop to marker, verify state is restored
TEST_F(StackAllocatorSystemTest, MarkerBasedDeallocation)
{
    // Initial allocation
    stack.Allocate(32);
    size_t usedBeforeMarker = stack.GetUsedMemory();

    // Capture marker
    void* marker = stack.GetMarker();

    // Allocate more after the marker
    stack.Allocate(128);
    stack.Allocate(64);
    EXPECT_GT(stack.GetUsedMemory(), usedBeforeMarker);

    // Pop back to marker
    stack.Pop(marker);

    // State should be restored to the point where marker was captured
    EXPECT_EQ(stack.GetUsedMemory(), usedBeforeMarker);
    EXPECT_EQ(stack.GetMarker(), marker);
}

// Requirement 11.3: Nested allocation pattern with multiple markers at different depths
TEST_F(StackAllocatorSystemTest, NestedAllocationPattern)
{
    // Depth 0
    void* m0 = stack.GetMarker();
    size_t off0 = stack.GetUsedMemory();
    stack.Allocate(64);

    // Depth 1
    void* m1 = stack.GetMarker();
    size_t off1 = stack.GetUsedMemory();
    stack.Allocate(64);

    // Depth 2
    void* m2 = stack.GetMarker();
    size_t off2 = stack.GetUsedMemory();
    stack.Allocate(64);

    EXPECT_GE(stack.GetUsedMemory(), 192u);

    // Unwind depth 2
    stack.Pop(m2);
    EXPECT_EQ(stack.GetUsedMemory(), off2);

    // Unwind depth 1
    stack.Pop(m1);
    EXPECT_EQ(stack.GetUsedMemory(), off1);

    // Unwind depth 0
    stack.Pop(m0);
    EXPECT_EQ(stack.GetUsedMemory(), off0);
}

// Requirement 11.3: Allocation respects alignment
TEST_F(StackAllocatorSystemTest, AllocationRespectsAlignment)
{
    // Misalign the stack with a 1-byte allocation
    stack.Allocate(1, 1);

    // Request 16-byte aligned allocation
    void* p = stack.Allocate(32, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
}

// Requirement 11.3: Sequential allocations are contiguous in memory
TEST_F(StackAllocatorSystemTest, SequentialAllocationsAreContiguous)
{
    // With default alignment, sequential allocations should be adjacent
    void* m = stack.GetMarker();

    void* p1 = stack.Allocate(64, 1);
    void* p2 = stack.Allocate(64, 1);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    // p2 should start immediately after p1 (no padding with alignment=1)
    EXPECT_EQ(static_cast<uint8_t*>(p1) + 64, static_cast<uint8_t*>(p2));

    stack.Pop(m);
}
