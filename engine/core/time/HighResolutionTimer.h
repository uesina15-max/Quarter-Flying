#pragma once
#include <chrono>

class HighResolutionTimer {
public:
  HighResolutionTimer() { Reset(); }

  void Reset() {
    start = clock::now();
    last = start;
  }

  float GetDeltaTime() {
    auto now = clock::now();
    std::chrono::duration<float> delta = now - last;
    last = now;
    return delta.count();
  }

  float GetTotalTime() const {
    auto now = clock::now();
    std::chrono::duration<float> total = now - start;
    return total.count();
  }

private:
  using clock = std::chrono::high_resolution_clock;
  clock::time_point start;
  clock::time_point last;
};
