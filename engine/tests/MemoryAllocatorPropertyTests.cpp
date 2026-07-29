// Feature: game-engine-core-systems
// Property 32: PoolAllocator 할당/해제 라운드트립  – validates Requirement 11.2
// Property 33: StackAllocator 중첩 할당 지원       – validates Requirement 11.3

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include "core/memory/PoolAllocator.h"
#include "core/memory/StackAllocator.h"
#include <vector>
#include <unordered_set>
#include <algorithm> // for std::max

using namespace Engine;

// ============================================================================
// Property 32: PoolAllocator 할당/해제 라운드트립
//
// For any sequence of alloc/free operations within pool capacity:
//   - Every Allocate() returns a non-null, unique pointer.
//   - After Free(), the block is available for reuse.
//   - GetFreeCount() + GetUsedCount() == GetCapacity() at all times.
//
// Validates: Requirement 11.2
// ============================================================================

RC_GTEST_PROP(PoolAllocatorPropertyTest, AllocFreeRoundTrip, ())
{
    // Generate a small capacity so tests stay fast
    const size_t capacity = *rc::gen::inRange<size_t>(4, 32);
    const size_t elemSize = *rc::gen::inRange<size_t>(8, 64);
    // elemSize must be >= sizeof(void*) for the free-list node
    const size_t safeElemSize = std::max(elemSize, sizeof(void*));

    PoolAllocator pool;
    auto initResult = pool.Initialize(safeElemSize, capacity);
    RC_ASSERT(initResult.has_value());

    // RAII guard to ensure Shutdown is called even if RC_ASSERT fails
    struct PoolGuard {
        PoolAllocator& p;
        ~PoolGuard() { p.Shutdown(); }
    } guard{pool};

    // --- Invariant: freeCount + usedCount == capacity at all times ---
    auto verifyInvariants = [&]() {
        RC_ASSERT(pool.GetFreeCount() + pool.GetUsedCount() == pool.GetCapacity());
    };
    verifyInvariants();

    // Allocate up to capacity blocks, collecting unique pointers
    std::vector<void*> ptrs;
    std::unordered_set<void*> seen;

    const size_t numAlloc = *rc::gen::inRange<size_t>(1, capacity + 1);
    for (size_t i = 0; i < numAlloc; ++i)
    {
        void* p = pool.Allocate();
        bool pValid = (p != nullptr);
        RC_ASSERT(pValid);
        // Each returned pointer must be unique
        RC_ASSERT(seen.find(p) == seen.end());
        seen.insert(p);
        ptrs.push_back(p);

        // Invariant holds after every alloc
        verifyInvariants();
    }

    // Free all blocks
    for (void* p : ptrs)
    {
        pool.Free(p);
        verifyInvariants();
    }

    // After freeing everything, pool is fully available again
    RC_ASSERT(pool.GetFreeCount() == pool.GetCapacity());
    RC_ASSERT(pool.GetUsedCount() == 0u);

    // Reuse: allocate again – must succeed (no memory leak)
    void* reused = pool.Allocate();
    bool reusedValid = (reused != nullptr);
    RC_ASSERT(reusedValid);
    // The reused pointer must have been one we previously freed
    RC_ASSERT(seen.find(reused) != seen.end());
    pool.Free(reused);
}

// ============================================================================
// Property 33: StackAllocator 중첩 할당 지원
//
// For any nested allocation pattern, Pop(marker) restores the allocator
// to exactly the state it was in when the marker was captured.
//
// (This property also exists in StackAllocatorTests.cpp; kept here for
//  completeness of the memory system property test file.)
//
// Validates: Requirement 11.3
// ============================================================================

RC_GTEST_PROP(StackAllocatorPropertyTest, NestedAllocMarkerPop, ())
{
    static constexpr size_t kBufSize = 8192;
    StackAllocator alloc;
    auto initResult = alloc.Initialize(kBufSize);
    RC_ASSERT(initResult.has_value());

    // RAII guard for shutdown safety
    struct StackGuard {
        StackAllocator& a;
        ~StackGuard() { a.Shutdown(); }
    } guard{alloc};

    const int levels = *rc::gen::inRange(1, 10);

    std::vector<void*>  markers;
    std::vector<size_t> offsets;

    for (int i = 0; i < levels; ++i)
    {
        markers.push_back(alloc.GetMarker());
        offsets.push_back(alloc.GetUsedMemory());

        const size_t sz = *rc::gen::inRange<size_t>(1, 256);
        // Guard against overflow
        const size_t kSafetyMargin = 64; // Alignment padding and safety margin
        if (alloc.GetUsedMemory() + sz + kSafetyMargin > kBufSize) break;
        
        void* p = alloc.Allocate(sz);
        bool pValid = (p != nullptr);
        RC_ASSERT(pValid);
    }

    // Pop in reverse order – each pop must restore the recorded offset
    for (int i = static_cast<int>(markers.size()) - 1; i >= 0; --i)
    {
        alloc.Pop(markers[i]);
        RC_ASSERT(alloc.GetUsedMemory() == offsets[i]);
    }

    // After popping all the way back, allocator is empty
    RC_ASSERT(alloc.GetUsedMemory() == 0u);
}
