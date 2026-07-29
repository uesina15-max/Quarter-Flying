#include "PoolAllocator.h"
#include "MemoryTracker.h"
#include "core/assert/Assert.h"
#include "core/logging/Logger.h"
#include <cstdlib>
#include <algorithm>
#include <new>

namespace Engine {

// Helper to align up
static inline size_t AlignUp(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

Result<void> PoolAllocator::Initialize(size_t elementSize, size_t capacity)
{
    if (capacity == 0)
    {
        return MakeUnexpected(EngineErrorCode::InvalidParameter, "capacity must be > 0", "PoolAllocator");
    }

    // Align elementSize to standard max_align_t
    elementSize_ = AlignUp(std::max(elementSize, sizeof(FreeNode)), alignof(std::max_align_t));
    capacity_    = capacity;
    freeCount_   = capacity;

    buffer_ = static_cast<uint8_t*>(std::malloc(elementSize_ * capacity_));
    if (buffer_ == nullptr)
    {
        return MakeUnexpected(EngineErrorCode::MemoryAllocationFailed, "PoolAllocator: malloc failed", "PoolAllocator");
    }
    MEMORY_TRACK_ALLOC_NAMED(buffer_, elementSize_ * capacity_, "PoolAllocatorBuffer", "PoolAllocator");

#ifdef _DEBUG
    allocated_.assign(capacity_, 0);
#endif

    // Build the free list by linking each block to the next.
    freeList_ = nullptr;
    for (size_t i = capacity_; i-- > 0;)
    {
        auto* node = reinterpret_cast<FreeNode*>(buffer_ + i * elementSize_);
        node->next = freeList_;
        freeList_  = node;
    }

    return {};
}

void PoolAllocator::Shutdown()
{
    if (buffer_)
    {
        MEMORY_TRACK_FREE(buffer_);
        std::free(buffer_);
        buffer_ = nullptr;
    }
    freeList_    = nullptr;
    elementSize_ = 0;
    capacity_    = 0;
    freeCount_   = 0;
#ifdef _DEBUG
    allocated_.clear();
#endif
}

void* PoolAllocator::Allocate()
{
    if (!freeList_) return nullptr;

    FreeNode* node = freeList_;
    freeList_ = node->next;
    --freeCount_;

#ifdef _DEBUG
    size_t idx = (reinterpret_cast<uint8_t*>(node) - buffer_) / elementSize_;
    allocated_[idx] = 1;
#endif

    return node;
}

void PoolAllocator::Free(void* ptr)
{
    if (!ptr) return;

    ENGINE_ASSERT(
        ptr >= buffer_ && ptr < buffer_ + elementSize_ * capacity_,
        "PoolAllocator::Free: pointer out of range"
    );
    ENGINE_ASSERT(
        (static_cast<uint8_t*>(ptr) - buffer_) % elementSize_ == 0,
        "PoolAllocator::Free: invalid alignment"
    );

#ifdef _DEBUG
    size_t idx = (static_cast<uint8_t*>(ptr) - buffer_) / elementSize_;
    ENGINE_ASSERT(allocated_[idx] == 1, "PoolAllocator::Free: double free detected!");
    allocated_[idx] = 0;
#endif

    auto* node = static_cast<FreeNode*>(ptr);
    node->next = freeList_;
    freeList_  = node;
    ++freeCount_;
}

} // namespace Engine
