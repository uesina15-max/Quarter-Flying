#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "core/memory/StackAllocator.h"
#include "core/memory/MemoryTracker.h"
#include <vector>

using namespace Engine;

// ============================================================
// Unit Tests
// ============================================================

class StackAllocatorTest : public ::testing::Test {
protected:
    StackAllocator alloc;

    void SetUp() override {
        auto result = alloc.Initialize(1024);
        ASSERT_TRUE(result.has_value()) << "StackAllocator initialization failed: " << result.error().message;
    }

    void TearDown() override {
        alloc.Shutdown();
    }
};

TEST_F(StackAllocatorTest, InitialStateIsEmpty) {
    EXPECT_EQ(alloc.GetUsedMemory(), 0u);
    EXPECT_EQ(alloc.GetCapacity(), 1024u);
}

TEST_F(StackAllocatorTest, AllocateReturnsNonNull) {
    void* ptr = alloc.Allocate(64);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(StackAllocatorTest, AllocateAdvancesOffset) {
    alloc.Allocate(64);
    EXPECT_GE(alloc.GetUsedMemory(), 64u);
}

TEST_F(StackAllocatorTest, GetMarkerReturnsCurrentTop) {
    void* marker = alloc.GetMarker();
    EXPECT_EQ(marker, static_cast<uint8_t*>(alloc.GetMarker()));
}

TEST_F(StackAllocatorTest, PopRestoresMarker) {
    void* marker = alloc.GetMarker();
    alloc.Allocate(128);
    EXPECT_GT(alloc.GetUsedMemory(), 0u);

    alloc.Pop(marker);
    EXPECT_EQ(alloc.GetUsedMemory(), 0u);
}

TEST_F(StackAllocatorTest, NestedAllocationsAndPops) {
    // Level 0 marker
    void* m0 = alloc.GetMarker();
    alloc.Allocate(64);

    // Level 1 marker
    void* m1 = alloc.GetMarker();
    alloc.Allocate(64);

    // Level 2 marker
    void* m2 = alloc.GetMarker();
    alloc.Allocate(64);

    size_t topOffset = alloc.GetUsedMemory();
    EXPECT_GE(topOffset, 192u);

    // Pop back to level 2
    alloc.Pop(m2);
    EXPECT_EQ(alloc.GetMarker(), m2);

    // Pop back to level 1
    alloc.Pop(m1);
    EXPECT_EQ(alloc.GetMarker(), m1);

    // Pop back to level 0
    alloc.Pop(m0);
    EXPECT_EQ(alloc.GetUsedMemory(), 0u);
}

TEST_F(StackAllocatorTest, AlignmentIsRespected) {
    // Allocate 1 byte to misalign, then allocate with 16-byte alignment
    alloc.Allocate(1, 1);
    void* ptr = alloc.Allocate(16, 16);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 16, 0u);
}

TEST_F(StackAllocatorTest, PopToBeginningAllowsReuse) {
    void* m0 = alloc.GetMarker();
    void* first = alloc.Allocate(64);
    alloc.Pop(m0);

    void* second = alloc.Allocate(64);
    EXPECT_EQ(first, second);
}

// ============================================================
// Property-Based Tests
// Validates: Requirements 11.3
// ============================================================

TEST(StackAllocatorPropertyTest, NestedAllocationMarkerPop) {
    // Property 33: StackAllocator 중첩 할당 지원
    // For all nested allocation patterns, Pop with a previous marker frees all subsequent allocations.
    rc::check("Pop restores allocator to marker position for any nested pattern", []() {
        const size_t bufSize = 4096;
        StackAllocator alloc;
        auto initResult = alloc.Initialize(bufSize);
        RC_ASSERT(initResult.has_value());

        // Generate a random number of levels (1..8)
        int levels = *rc::gen::inRange(1, 9);

        std::vector<void*> markers;
        std::vector<size_t> offsets;

        for (int i = 0; i < levels; ++i) {
            markers.push_back(alloc.GetMarker());
            offsets.push_back(alloc.GetUsedMemory());

            size_t allocSize = *rc::gen::inRange<size_t>(1, 128);
            // Ensure we don't overflow
            if (alloc.GetUsedMemory() + allocSize + 16 > bufSize) break;
            alloc.Allocate(allocSize);
        }

        // Pop in reverse order and verify offset is restored
        for (int i = static_cast<int>(markers.size()) - 1; i >= 0; --i) {
            alloc.Pop(markers[i]);
            RC_ASSERT(alloc.GetUsedMemory() == offsets[i]);
        }

        alloc.Shutdown();
    });
}

// ============================================================
// MemoryTracker Integration Tests (Debug only)
// Validates: Initialize registers the backing buffer,
//            Shutdown unregisters it.
// ============================================================

#ifdef _DEBUG

TEST(StackAllocatorMemoryTracking, InitializeRegistersAllocation)
{
    MemoryTracker& tracker = MemoryTracker::Get();
    const size_t before = tracker.GetAllocationCount();

    StackAllocator alloc;
    alloc.Initialize(512);

    // After Initialize, the backing buffer should be tracked.
    EXPECT_GT(tracker.GetAllocationCount(), before)
        << "StackAllocator backing buffer was not registered in MemoryTracker";

    alloc.Shutdown();
}

TEST(StackAllocatorMemoryTracking, ShutdownUnregistersAllocation)
{
    MemoryTracker& tracker = MemoryTracker::Get();

    StackAllocator alloc;
    alloc.Initialize(512);
    const size_t afterInit = tracker.GetAllocationCount();

    alloc.Shutdown();

    // After Shutdown, the count should decrease (buffer freed).
    EXPECT_LT(tracker.GetAllocationCount(), afterInit)
        << "StackAllocator backing buffer was not unregistered after Shutdown";
}

TEST(StackAllocatorMemoryTracking, InternalBumpDoesNotAlterTrackerCount)
{
    // Internal allocations (bump pointer moves, Pop) must NOT
    // create spurious MemoryTracker entries.
    MemoryTracker& tracker = MemoryTracker::Get();

    StackAllocator alloc;
    alloc.Initialize(1024);
    const size_t afterInit = tracker.GetAllocationCount();

    void* m = alloc.GetMarker();
    alloc.Allocate(64);
    alloc.Allocate(128, 16);
    alloc.Pop(m);

    // Tracker count should be unchanged — no new backing allocs.
    EXPECT_EQ(tracker.GetAllocationCount(), afterInit)
        << "Internal bump-pointer operations changed MemoryTracker count";

    alloc.Shutdown();
}

#endif // _DEBUG
