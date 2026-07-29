#include "StackAllocator.h"
#include "MemoryTracker.h"
#include "core/assert/Assert.h"
#include "core/logging/Logger.h"
#include <cstdlib>

namespace Engine {

Result<void> StackAllocator::Initialize(size_t size) {
    // Guard against double-initialize without matching Shutdown.
    if (buffer != nullptr)
    {
        return MakeUnexpected(EngineErrorCode::AlreadyInitialized, "StackAllocator::Initialize called while already initialized!", "StackAllocator");
    }

    buffer = static_cast<uint8_t*>(std::malloc(size));

    if (buffer == nullptr)
    {
        return MakeUnexpected(EngineErrorCode::MemoryAllocationFailed, "StackAllocator::Initialize: malloc failed — out of memory!", "StackAllocator");
    }

    capacity = size;
    offset   = 0;
    hadOverflow = false;

    MEMORY_TRACK_ALLOC_NAMED(buffer, size, "StackAllocatorBuffer", "StackAllocator");

    return {};
}

void StackAllocator::Shutdown() {
    if (buffer) {
        MEMORY_TRACK_FREE(buffer);
        std::free(buffer);
        buffer = nullptr;
    }
    capacity = 0;
    offset   = 0;
    hadOverflow = false;
}

void* StackAllocator::Allocate(size_t size, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(buffer + offset);
    size_t aligned = (current + (alignment - 1)) & ~(alignment - 1);

    size_t newOffset = (aligned - reinterpret_cast<size_t>(buffer)) + size;

    if (newOffset > capacity)
    {
        hadOverflow = true;
        Logger::Error("StackAllocator overflow! Requested: {} bytes, Available: {} bytes, Used: {} bytes",
                      size, capacity - offset, offset);
        ENGINE_ASSERT(newOffset <= capacity, "StackAllocator overflow!");
        return nullptr;
    }

    offset = newOffset;

    return reinterpret_cast<void*>(aligned);
}

void StackAllocator::Pop(void* marker) {
    ENGINE_ASSERT(marker >= buffer && marker <= buffer + capacity,
                  "StackAllocator::Pop marker out of range!");
    ENGINE_ASSERT(marker <= buffer + offset,
                  "StackAllocator::Pop marker is ahead of current top!");

    offset = static_cast<uint8_t*>(marker) - buffer;
}

} // namespace Engine
