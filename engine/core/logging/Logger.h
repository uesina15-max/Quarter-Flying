#pragma once
#include <string>
#include <utility>
#include <fmt/core.h>

namespace Engine {

enum class LogLevel
{
    Trace,
    Info,
    Warning,
    Error,
    Fatal
};

class Logger
{
public:
    static void Initialize();
    static void Shutdown();

    static inline LogLevel s_MinLevel = LogLevel::Trace;
    static void SetMinLevel(LogLevel level) { s_MinLevel = level; }
    static bool IsLoggable(LogLevel level) { return level >= s_MinLevel; }

    template <typename... Args>
    static void Log(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args)
    {
        Write(level, fmt::format(fmt, std::forward<Args>(args)...));
    }

    static void Log(LogLevel level, const char* message) { Write(level, message); }
    static void Log(LogLevel level, const std::string& message) { Write(level, message); }
    
    template <typename... Args>
    static void Trace(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Trace, fmt, std::forward<Args>(args)...); }
    
    template <typename... Args>
    static void Info(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Info, fmt, std::forward<Args>(args)...); }
    
    template <typename... Args>
    static void Warning(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Warning, fmt, std::forward<Args>(args)...); }
    
    template <typename... Args>
    static void Error(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Error, fmt, std::forward<Args>(args)...); }
    
    template <typename... Args>
    static void Fatal(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Fatal, fmt, std::forward<Args>(args)...); }
    
    template <typename... Args>
    static void Debug(fmt::format_string<Args...> fmt, Args&&... args) { Log(LogLevel::Trace, fmt, std::forward<Args>(args)...); }

private:
    static void Write(LogLevel level, const std::string& message);
};

} // namespace Engine

#define ENGINE_LOG_TRACE(...) do { if (Engine::Logger::IsLoggable(Engine::LogLevel::Trace))   Engine::Logger::Trace(__VA_ARGS__); } while(0)
#define ENGINE_LOG_INFO(...)  do { if (Engine::Logger::IsLoggable(Engine::LogLevel::Info))    Engine::Logger::Info(__VA_ARGS__); } while(0)
#define ENGINE_LOG_WARN(...)  do { if (Engine::Logger::IsLoggable(Engine::LogLevel::Warning)) Engine::Logger::Warning(__VA_ARGS__); } while(0)
#define ENGINE_LOG_ERROR(...) do { if (Engine::Logger::IsLoggable(Engine::LogLevel::Error))   Engine::Logger::Error(__VA_ARGS__); } while(0)
#define ENGINE_LOG_FATAL(...) do { if (Engine::Logger::IsLoggable(Engine::LogLevel::Fatal))   Engine::Logger::Fatal(__VA_ARGS__); } while(0)

#define LOG_TRACE(...) ENGINE_LOG_TRACE(__VA_ARGS__)
#define LOG_INFO(...)  ENGINE_LOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)  ENGINE_LOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) ENGINE_LOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) ENGINE_LOG_FATAL(__VA_ARGS__)

