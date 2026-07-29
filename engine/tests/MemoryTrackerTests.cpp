#ifdef _DEBUG

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include "core/memory/MemoryTracker.h"
#include <vector>
#include <string>
#include <cstdlib>

using namespace Engine;

// ============================================================================
// Helper: isolated MemoryTracker instance for each test
// (avoids cross-test pollution from the global singleton)
// ============================================================================
static MemoryTracker MakeTracker() { return MemoryTracker{}; }

// ============================================================================
// Unit Tests – Task 15.3
// Validates: Requirements 11.4, 11.5
// ============================================================================

class MemoryTrackerTest : public ::testing::Test
{
protected:
    MemoryTracker tracker;

    // Dummy heap pointers – we never actually free them through the system
    // allocator here; we just use the addresses as keys.
    std::vector<void*> ptrs;

    void* NewPtr(size_t size = 16)
    {
        void* p = std::malloc(size);
        ptrs.push_back(p);
        return p;
    }

    void TearDown() override
    {
        for (void* p : ptrs) std::free(p);
        ptrs.clear();
    }
};

// --- RecordAllocation ---

TEST_F(MemoryTrackerTest, RecordAllocation_TracksPointer)
{
    void* p = NewPtr(32);
    tracker.RecordAllocation(p, 32, "test", __FILE__, __LINE__);
    EXPECT_TRUE(tracker.IsTracked(p));
    EXPECT_EQ(tracker.GetAllocationCount(), 1u);
}

TEST_F(MemoryTrackerTest, RecordAllocation_NullPointerIgnored)
{
    tracker.RecordAllocation(nullptr, 16, "tag", __FILE__, __LINE__);
    EXPECT_EQ(tracker.GetAllocationCount(), 0u);
}

TEST_F(MemoryTrackerTest, RecordAllocation_MultiplePointers)
{
    void* p1 = NewPtr(8);
    void* p2 = NewPtr(16);
    void* p3 = NewPtr(32);
    tracker.RecordAllocation(p1, 8,  "a", __FILE__, __LINE__);
    tracker.RecordAllocation(p2, 16, "b", __FILE__, __LINE__);
    tracker.RecordAllocation(p3, 32, "c", __FILE__, __LINE__);
    EXPECT_EQ(tracker.GetAllocationCount(), 3u);
    EXPECT_TRUE(tracker.IsTracked(p1));
    EXPECT_TRUE(tracker.IsTracked(p2));
    EXPECT_TRUE(tracker.IsTracked(p3));
}

// --- RecordDeallocation ---

TEST_F(MemoryTrackerTest, RecordDeallocation_RemovesPointer)
{
    void* p = NewPtr(16);
    tracker.RecordAllocation(p, 16, "tag", __FILE__, __LINE__);
    tracker.RecordDeallocation(p);
    EXPECT_FALSE(tracker.IsTracked(p));
    EXPECT_EQ(tracker.GetAllocationCount(), 0u);
}

TEST_F(MemoryTrackerTest, RecordDeallocation_NullPointerIgnored)
{
    // Should not crash or change state
    tracker.RecordDeallocation(nullptr);
    EXPECT_EQ(tracker.GetAllocationCount(), 0u);
}

TEST_F(MemoryTrackerTest, RecordDeallocation_DoubleFreeDoesNotCrash)
{
    void* p = NewPtr(16);
    tracker.RecordAllocation(p, 16, "tag", __FILE__, __LINE__);
    tracker.RecordDeallocation(p);
    // Second deallocation should warn but not crash
    EXPECT_NO_FATAL_FAILURE(tracker.RecordDeallocation(p));
    EXPECT_EQ(tracker.GetAllocationCount(), 0u);
}

TEST_F(MemoryTrackerTest, RecordDeallocation_UntrackedPointerDoesNotCrash)
{
    void* p = NewPtr(16);
    // Never recorded – should warn but not crash
    EXPECT_NO_FATAL_FAILURE(tracker.RecordDeallocation(p));
}

// --- ReportLeaks ---

TEST_F(MemoryTrackerTest, ReportLeaks_NoLeaksWhenAllFreed)
{
    void* p = NewPtr(16);
    tracker.RecordAllocation(p, 16, "tag", __FILE__, __LINE__);
    tracker.RecordDeallocation(p);
    // Should not crash and should report no leaks
    EXPECT_NO_FATAL_FAILURE(tracker.ReportLeaks());
    EXPECT_EQ(tracker.GetAllocationCount(), 0u);
}

TEST_F(MemoryTrackerTest, ReportLeaks_DetectsUnfreedAllocation)
{
    void* p = NewPtr(64);
    tracker.RecordAllocation(p, 64, "leaked", __FILE__, __LINE__);
    // Do NOT free – this is the leak
    EXPECT_EQ(tracker.GetAllocationCount(), 1u);
    EXPECT_NO_FATAL_FAILURE(tracker.ReportLeaks());
    // Cleanup for TearDown
    tracker.RecordDeallocation(p);
}

// --- Singleton ---

TEST(MemoryTrackerSingletonTest, GetReturnsSameInstance)
{
    MemoryTracker& a = MemoryTracker::Get();
    MemoryTracker& b = MemoryTracker::Get();
    EXPECT_EQ(&a, &b);
}

// ============================================================================
// Property-Based Tests – Task 15.3
//
// Property 34: 메모리 추적 정확성 (디버그)
//   For all allocations, allocation info is recorded and removed on deallocation.
//
// Property 35: 메모리 누수 감지 (디버그)
//   For all alloc/dealloc sequences, unreleased allocations are detected as leaks.
//
// Validates: Requirements 11.4, 11.5
// ============================================================================

class MemoryTrackerPropertyTest : public ::testing::Test
{
protected:
    MemoryTracker tracker;
    std::vector<void*> heap;

    void* Alloc(size_t sz = 16)
    {
        void* p = std::malloc(sz);
        heap.push_back(p);
        return p;
    }

    void TearDown() override
    {
        for (void* p : heap) std::free(p);
        heap.clear();
    }
};

/**
 * // Feature: game-engine-core-systems, Property 34: 메모리 추적 정확성 (디버그)
 *
 * Property 34: 메모리 추적 정확성 (디버그)
 *
 * For any sequence of allocations with arbitrary sizes and tags, the allocation
 * info (size, tag, file, line) must be correctly stored, and after deallocation
 * the entry must be removed from tracking.
 *
 * **Validates: Requirements 11.4**
 */
RC_GTEST_FIXTURE_PROP(MemoryTrackerPropertyTest, AllocationTrackingAccuracy, ())
{
    // Generate arbitrary number of allocations (1..20)
    const int n = *rc::gen::inRange(1, 20);

    // Use a fixed set of tag strings to avoid dangling pointer issues
    static const char* const kTags[] = { "alpha", "beta", "gamma", "delta", "epsilon" };
    static const int kTagCount = 5;

    struct AllocRecord {
        void*       ptr;
        size_t      size;
        const char* tag;
        int         line;
    };

    std::vector<AllocRecord> records;
    records.reserve(n);

    for (int i = 0; i < n; ++i)
    {
        // Arbitrary size in [1, 512]
        const size_t sz = static_cast<size_t>(*rc::gen::inRange(1, 512));
        const char*  tag = kTags[i % kTagCount];
        const int    ln  = 100 + i;

        void* p = Alloc(sz);
        tracker.RecordAllocation(p, sz, tag, __FILE__, ln);

        // Property: pointer must be tracked immediately after recording
        RC_ASSERT(tracker.IsTracked(p));

        // Property: stored info must match what was passed in
        const AllocationInfo* info = tracker.GetAllocationInfo(p);
        bool infoValid = (info != nullptr);
        RC_ASSERT(infoValid);
        RC_ASSERT(info->size == sz);
        bool tagValid = (info->tag != nullptr && std::string(info->tag) == std::string(tag));
        RC_ASSERT(tagValid);
        RC_ASSERT(info->line == ln);

        records.push_back({p, sz, tag, ln});
    }

    // Property: total tracked count must equal number of allocations
    RC_ASSERT(tracker.GetAllocationCount() == static_cast<size_t>(n));

    // Deallocate all and verify each is removed
    for (const auto& rec : records)
    {
        tracker.RecordDeallocation(rec.ptr);
        // Property: pointer must no longer be tracked after deallocation
        RC_ASSERT(!tracker.IsTracked(rec.ptr));
        bool afterNull = (tracker.GetAllocationInfo(rec.ptr) == nullptr);
        RC_ASSERT(afterNull);
    }

    // Property: tracker must be empty after all deallocations
    RC_ASSERT(tracker.GetAllocationCount() == 0u);
}

/**
 * Property 35: 메모리 누수 감지 (디버그)
 *
 * For any alloc/dealloc sequence, the number of tracked allocations equals
 * (total allocated) - (total deallocated), and ReportLeaks does not crash.
 *
 * **Validates: Requirements 11.5**
 */
RC_GTEST_FIXTURE_PROP(MemoryTrackerPropertyTest, LeakDetectionAccuracy, ())
{
    const int total   = *rc::gen::inRange(2, 40);
    const int toFree  = *rc::gen::inRange(0, total);  // 0..total freed, rest are "leaks"

    std::vector<void*> ptrs;
    ptrs.reserve(total);

    for (int i = 0; i < total; ++i)
    {
        void* p = Alloc(16);
        tracker.RecordAllocation(p, 16, "prop35", __FILE__, __LINE__);
        ptrs.push_back(p);
    }

    // Free the first `toFree` pointers
    for (int i = 0; i < toFree; ++i)
    {
        tracker.RecordDeallocation(ptrs[i]);
    }

    const size_t expectedLeaks = static_cast<size_t>(total - toFree);
    RC_ASSERT(tracker.GetAllocationCount() == expectedLeaks);

    // ReportLeaks must not crash regardless of how many leaks exist
    bool threw = false;
    try { tracker.ReportLeaks(); } catch (...) { threw = true; }
    RC_ASSERT(!threw);

    // Cleanup remaining tracked allocations
    for (int i = toFree; i < total; ++i)
    {
        tracker.RecordDeallocation(ptrs[i]);
    }

    RC_ASSERT(tracker.GetAllocationCount() == 0u);
}

#endif // _DEBUG
