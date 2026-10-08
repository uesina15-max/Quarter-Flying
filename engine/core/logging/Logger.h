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

    // 최소 레벨은 Logger.cpp(엔진 DLL) 안에 하나만 둔다.
    // 증상: 게임 실행 파일(PlayerMain.cpp)에서 SetMinLevel(Info)를 불러도 엔진 DLL이 찍는 TRACE 로그가
    // 그대로 나왔다(60프레임 스모크 테스트에서 1000줄 이상). 원인: 예전에는 `static inline LogLevel
    // s_MinLevel`이 헤더에 있어서, 이 헤더를 include한 모듈(엔진 DLL, quarterflying.pyd, Player exe)마다
    // 사본이 따로 생겼다. CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS는 함수만 내보내고 데이터는 내보내지 않으므로
    // 한 모듈에서 바꾼 값이 다른 모듈에 보이지 않았다(컴파일도 되고 에러도 없어서 드러나지 않음).
    static void SetMinLevel(LogLevel level);
    static LogLevel GetMinLevel();
    static bool IsLoggable(LogLevel level);

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

