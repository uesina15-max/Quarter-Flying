#include "MemoryTracker.h"

#ifdef _DEBUG

#include "../logging/Logger.h"
#include <cstdio>

namespace Engine {

// ---------------------------------------------------------------------------
// MemoryTracker implementation
// ---------------------------------------------------------------------------

void MemoryTracker::RecordAllocation(void* ptr, size_t size, const char* tag,
                                     const char* file, int line, uint32_t threadId, const char* allocatorName)
{
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(mutex_);

    AllocationInfo info;
    info.address       = ptr;
    info.size          = size;
    info.tag           = tag  ? tag  : "<untagged>";
    info.file          = file ? file : "<unknown>";
    info.line          = line;
    info.threadId      = threadId;
    info.allocatorName = allocatorName ? allocatorName : "Global";

    allocations_[ptr] = info;
}

void MemoryTracker::RecordDeallocation(void* ptr)
{
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = allocations_.find(ptr);
    if (it == allocations_.end())
    {
        // Double-free or untracked pointer
        Logger::Warning("[MemoryTracker] Double-free or untracked deallocation: ptr={}", ptr);
        return;
    }

    allocations_.erase(it);
}

void MemoryTracker::ReportLeaks() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (allocations_.empty())
    {
        Logger::Info("[MemoryTracker] No memory leaks detected.");
        return;
    }

    Logger::Warning("[MemoryTracker] === MEMORY LEAK REPORT ===");
    Logger::Warning("[MemoryTracker] {} leak(s) detected:", allocations_.size());

    for (const auto& [ptr, info] : allocations_)
    {
        Logger::Warning("[MemoryTracker]   ptr={}  size={}  tag='{}'  file={}  line={}  allocator={}  thread={}",
                        info.address, info.size, info.tag, info.file, info.line, info.allocatorName, info.threadId);
    }

    Logger::Warning("[MemoryTracker] =========================");
}

size_t MemoryTracker::GetAllocationCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.size();
}

bool MemoryTracker::IsTracked(void* ptr) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return allocations_.find(ptr) != allocations_.end();
}

const AllocationInfo* MemoryTracker::GetAllocationInfo(void* ptr) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.find(ptr);
    if (it == allocations_.end()) return nullptr;
    return &it->second;
}

MemoryTracker& MemoryTracker::Get()
{
    static MemoryTracker instance;
    return instance;
}

} // namespace Engine

#endif // _DEBUG
