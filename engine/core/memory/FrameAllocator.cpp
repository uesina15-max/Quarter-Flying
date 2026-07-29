#include "FrameAllocator.h"
#include "core/assert/Assert.h"
#include "core/logging/Logger.h"
#include "MemoryTracker.h"
#include <cstdlib>

namespace Engine {

void FrameAllocator::Initialize(size_t size) {
  buffer = static_cast<uint8_t *>(std::malloc(size));
  capacity = size;
  offset = 0;
  hadOverflow = false;
  warningLogged = false;
  MEMORY_TRACK_ALLOC_NAMED(buffer, size, "FrameAllocatorBuffer", "FrameAllocator");
}

void FrameAllocator::Shutdown() {
  if (buffer) {
    MEMORY_TRACK_FREE(buffer);
    std::free(buffer);
    buffer = nullptr;
  }
  capacity = 0;
  offset = 0;
  hadOverflow = false;
  warningLogged = false;
}

void *FrameAllocator::Allocate(size_t size, size_t alignment) {
  size_t current = reinterpret_cast<size_t>(buffer + offset);
  size_t aligned = (current + (alignment - 1)) & ~(alignment - 1);

  size_t newOffset = (aligned - reinterpret_cast<size_t>(buffer)) + size;

  if (newOffset > capacity)
  {
    // 오버플로우 발생 시 플래그 설정 및 로그
    hadOverflow = true;
    Logger::Error("FrameAllocator overflow! Requested: {} bytes, Available: {} bytes, Used: {} bytes",
                 size, capacity - offset, offset);
    ENGINE_ASSERT(newOffset <= capacity, "FrameAllocator overflow!");
    return nullptr;
  }

  offset = newOffset;

  // 메모리 사용량이 80% 이상이면 경고 로그 (한 번만)
  if (IsNearCapacity() && !warningLogged)
  {
    Logger::Warning("FrameAllocator near capacity! Used: {} bytes ({}%), Capacity: {} bytes",
                   offset, (offset * 100) / capacity, capacity);
    warningLogged = true;
  }

  return reinterpret_cast<void *>(aligned);
}

void FrameAllocator::Reset() {
  offset = 0;
  hadOverflow = false;
  warningLogged = false;
}

} // namespace Engine
