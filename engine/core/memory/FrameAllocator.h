#pragma once
#include <cstddef>
#include <cstdint>
#include "../EngineError.h"

namespace Engine {

class FrameAllocator {
public:
  void Initialize(size_t size);
  void Shutdown();

  void *Allocate(size_t size, size_t alignment = 8);
  void Reset();

  size_t GetUsedMemory() const { return offset; }
  size_t GetCapacity() const { return capacity; }

  // 오버플로우 발생 여부 확인 (Reset 시 초기화됨)
  bool HadOverflow() const { return hadOverflow; }

  // 메모리 사용량 경고 임계값 확인 (80%)
  bool IsNearCapacity() const { return offset > (capacity * 8 / 10); }

private:
  uint8_t *buffer = nullptr;
  size_t capacity = 0;
  size_t offset = 0;
  bool hadOverflow = false;
  bool warningLogged = false;
};

} // namespace Engine
