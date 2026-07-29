#pragma once
#include <cstddef>
#include <cstdint>
#include "../EngineError.h"

namespace Engine {

// Stack (LIFO) linear allocator.
// Manages a single contiguous buffer; allocations advance a bump pointer.
// Deallocation is performed by restoring a previously-captured marker via Pop().
//
// Thread safety: NOT thread-safe. Use one allocator per thread or
//               wrap externally.
//
// MemoryTracker integration: the backing buffer (Initialize/Shutdown) is tracked
// as a single allocation. Internal bump-pointer moves are not tracked individually
// — only the buffer lifetime is observable via MemoryTracker.
class StackAllocator {
public:
    // Allocate backing buffer of 'size' bytes.
    // Must not be called more than once without a matching Shutdown().
    Result<void> Initialize(size_t size);

    // Release backing buffer. Safe to call if Initialize() was never called.
    void Shutdown();

    // Bump-allocate 'size' bytes aligned to 'alignment'.
    // Returns a pointer inside the backing buffer.
    // Returns nullptr on overflow.
    void* Allocate(size_t size, size_t alignment = 8);

    // Restore the bump pointer to the position recorded in 'marker'.
    // 'marker' must have been obtained via GetMarker() and must not
    // point past the current top.
    void Pop(void* marker);

    // Return a pointer to the current top of the stack (current bump position).
    // Capture this before allocating to use as a rollback point for Pop().
    void* GetMarker() const { return buffer + offset; }

    size_t GetUsedMemory() const { return offset; }
    size_t GetCapacity()   const { return capacity; }

    // 오버플로우 발생 여부 확인
    bool HadOverflow() const { return hadOverflow; }

private:
    uint8_t* buffer   = nullptr;
    size_t   capacity = 0;
    size_t   offset   = 0;
    bool     hadOverflow = false;
};

} // namespace Engine
