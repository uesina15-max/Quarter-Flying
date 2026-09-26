# ========================================
# Compiler Flags & Environment Checks
# ========================================

message(STATUS "========================================")
message(STATUS "Quarter Flying Build Environment Check")
message(STATUS "========================================")

# System Information
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "CMake Generator: ${CMAKE_GENERATOR}")

# Compiler Information
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Compiler Path: ${CMAKE_CXX_COMPILER}")

# C++ Standard Support
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "MSVC Version: ${MSVC_VERSION}")
    if(MSVC_VERSION LESS 1930)
        message(WARNING "MSVC version ${MSVC_VERSION} may not fully support C++23. Recommended: VS 2022 (1930+)")
    endif()
    add_compile_options(/utf-8 /FS)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
        message(WARNING "GCC version ${CMAKE_CXX_COMPILER_VERSION} may not fully support C++23. Recommended: GCC 11+")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0)
        message(WARNING "Clang version ${CMAKE_CXX_COMPILER_VERSION} may not fully support C++23. Recommended: Clang 14+")
    endif()
endif()

# Build Type
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Platform-specific checks
if(WIN32)
    message(STATUS "Platform: Windows")
    if(NOT MSVC)
        message(WARNING "Non-MSVC compiler on Windows may have compatibility issues")
    endif()
elseif(APPLE)
    message(STATUS "Platform: macOS")
    message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
elseif(UNIX)
    message(STATUS "Platform: Linux/Unix")
endif()

message(STATUS "========================================")

# ========================================
# System Requirements Check
# ========================================

# Check disk space (at least 2GB required)
if(UNIX)
    execute_process(COMMAND df -k . OUTPUT_VARIABLE DISK_INFO OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCHALL "[0-9]+" DISK_NUMBERS ${DISK_INFO})
    if(DISK_NUMBERS)
        list(GET DISK_NUMBERS 3 AVAILABLE_KB)
        math(EXPR AVAILABLE_MB "${AVAILABLE_KB} / 1024")
        if(AVAILABLE_MB LESS 2048)
            message(WARNING "Low disk space: ${AVAILABLE_MB}MB available. Recommended: 2GB+")
        else()
            message(STATUS "Disk space: ${AVAILABLE_MB}MB available ✓")
        endif()
    endif()
elseif(WIN32)
    message(STATUS "Disk space check skipped on Windows (check manually)")
endif()

# Check memory (at least 4GB recommended)
if(UNIX)
    execute_process(COMMAND free -m OUTPUT_VARIABLE MEM_INFO OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCHALL "[0-9]+" MEM_NUMBERS ${MEM_INFO})
    if(MEM_NUMBERS)
        list(GET MEM_NUMBERS 1 TOTAL_MB)
        if(TOTAL_MB LESS 4096)
            message(WARNING "Low memory: ${TOTAL_MB}MB total. Recommended: 4GB+")
        else()
            message(STATUS "Memory: ${TOTAL_MB}MB total ✓")
        endif()
    endif()
elseif(WIN32)
    message(STATUS "Memory check skipped on Windows (check manually)")
endif()

# Check Python (for Python bindings)
find_program(PYTHON_EXECUTABLE NAMES python3 python)
if(PYTHON_EXECUTABLE)
    message(STATUS "Python found: ${PYTHON_EXECUTABLE}")
    execute_process(COMMAND ${PYTHON_EXECUTABLE} --version OUTPUT_VARIABLE GE_PYTHON_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Python version: ${GE_PYTHON_VERSION}")
else()
    message(WARNING "Python not found. Python bindings will not be built.")
endif()

message(STATUS "========================================")

# Clang-tidy integration
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found! Please install it for static analysis.")
endif()
