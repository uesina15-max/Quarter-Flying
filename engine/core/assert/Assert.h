#pragma once
#include "core/logging/Logger.h"
#include <intrin.h> // For __debugbreak

#ifdef _DEBUG
#define ENGINE_ASSERT(cond, msg)                                               \
  if (!(cond)) {                                                               \
    Logger::Log(LogLevel::Fatal, "ASSERT FAILED: %s", msg);                    \
    __debugbreak();                                                            \
  }
#else
#define ENGINE_ASSERT(cond, msg)
#endif
