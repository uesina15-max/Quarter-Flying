#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "../EngineError.h"

namespace Engine {

// Fixed-size object pool allocator.
// Manages a contiguous buffer divided into equal-sized blocks,
// linked together via a free list for O(1) alloc/free.
//
// MemoryTracker integration: the backing buffer is tracked as a single
// allocation on Initialize / Shutdown. Individual block alloc/free
// operations are not tracked.
class PoolAllocator
{
public:
    // Initialize with elementSize bytes per block and capacity blocks.
    Result<void> Initialize(size_t elementSize, size_t capacity);
    void Shutdown();

    // Allocate one block. Returns nullptr if the pool is exhausted.
    void* Allocate();

    // Return a block to the pool. ptr must have been returned by Allocate().
    void Free(void* ptr);

    size_t GetCapacity()  const { return capacity_; }
    size_t GetFreeCount() const { return freeCount_; }
    size_t GetUsedCount() const { return capacity_ - freeCount_; }

private:
    struct FreeNode { FreeNode* next; };

    uint8_t*  buffer_      = nullptr;
    FreeNode* freeList_    = nullptr;
    size_t    elementSize_ = 0;
    size_t    capacity_    = 0;
    size_t    freeCount_   = 0;

#ifdef _DEBUG
    std::vector<uint8_t> allocated_;
#endif
};

} // namespace Engine
