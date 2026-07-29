/*
 * Copyright 2026 Quarter Flying Game Engine Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Logger.h"
#include <ctime>
#include <iostream>
#include <mutex>
#include <string>

namespace Engine {

static std::mutex g_LogMutex;

void Logger::Initialize() {
}

void Logger::Shutdown() {
}

void Logger::Write(LogLevel level, const std::string &message) {
    if (!IsLoggable(level)) return;

    std::lock_guard<std::mutex> lock(g_LogMutex);

    const char *levelStr = "INFO";
    switch (level) {
        case LogLevel::Trace:   levelStr = "TRACE"; break;
        case LogLevel::Info:    levelStr = "INFO "; break;
        case LogLevel::Warning: levelStr = "WARN "; break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
        case LogLevel::Fatal:   levelStr = "FATAL"; break;
    }

    std::time_t now = std::time(nullptr);
    char timeStr[20];
    
    // Use thread-safe localtime_s on Windows or localtime_r on POSIX
#ifdef _WIN32
    struct tm timeInfo;
    localtime_s(&timeInfo, &now);
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
#else
    struct tm timeInfo;
    localtime_r(&now, &timeInfo);
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
#endif

    std::cout << "[" << timeStr << "] [" << levelStr << "] " << message << std::endl;
}

} // namespace Engine
