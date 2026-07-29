#pragma once

#ifdef _DEBUG

#include <cstddef>
#include <unordered_map>
#include <mutex>

namespace Engine {

// Allocation info recorded per tracked pointer
struct AllocationInfo
{
    void*       address;
    size_t      size;
    const char* tag;
    const char* file;
    int         line;
    uint32_t    threadId;
    const char* allocatorName;
};

class MemoryTracker
{
public:
    MemoryTracker() = default;
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    MemoryTracker(MemoryTracker&&) = delete;
    MemoryTracker& operator=(MemoryTracker&&) = delete;

    // Record a new allocation
    void RecordAllocation(void* ptr, size_t size, const char* tag, const char* file, int line, uint32_t threadId = 0, const char* allocatorName = "Default");

    // Record a deallocation; warns on double-free
    void RecordDeallocation(void* ptr);

    // Log all unreleased allocations (memory leaks)
    void ReportLeaks() const;

    // Returns the number of currently tracked (live) allocations
    size_t GetAllocationCount() const;

    // Returns true if the given pointer is currently tracked
    bool IsTracked(void* ptr) const;

    // Returns the AllocationInfo for a tracked pointer; returns nullptr if not tracked
    const AllocationInfo* GetAllocationInfo(void* ptr) const;

    // Global singleton accessor
    static MemoryTracker& Get();

private:
    mutable std::mutex                          mutex_;
    std::unordered_map<void*, AllocationInfo>   allocations_;
};

// Helper to get thread id as uint32_t
inline uint32_t GetCurrentThreadIdAsUint() {
    return 0; // Simplified for cross-platform. Would typically hash std::this_thread::get_id() or use OS API
}

// Convenience macros for tagging allocations
#define MEMORY_TRACK_ALLOC(ptr, size, tag) \
    Engine::MemoryTracker::Get().RecordAllocation((ptr), (size), (tag), __FILE__, __LINE__, Engine::GetCurrentThreadIdAsUint(), "Global")

#define MEMORY_TRACK_ALLOC_NAMED(ptr, size, tag, allocatorName) \
    Engine::MemoryTracker::Get().RecordAllocation((ptr), (size), (tag), __FILE__, __LINE__, Engine::GetCurrentThreadIdAsUint(), (allocatorName))

#define MEMORY_TRACK_FREE(ptr) \
    Engine::MemoryTracker::Get().RecordDeallocation((ptr))

} // namespace Engine

#else // _DEBUG not defined

// No-op stubs for release builds
#define MEMORY_TRACK_ALLOC(ptr, size, tag) ((void)0)
#define MEMORY_TRACK_ALLOC_NAMED(ptr, size, tag, allocatorName) ((void)0)
#define MEMORY_TRACK_FREE(ptr)             ((void)0)

#endif // _DEBUG
