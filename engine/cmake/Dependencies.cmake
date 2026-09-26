# ========================================
# Dependencies Management
# ========================================

include(FetchContent)

# Configure FetchContent for better caching and performance
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/deps")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Disable FetchContent updates for faster builds" FORCE)

function(CheckFetchContentDependency NAME URL)
    message(STATUS "Fetching ${NAME}...")
    FetchContent_Declare(
        ${NAME}
        URL ${URL}
    )
    
    # Try to make available and check for errors
    FetchContent_MakeAvailable(${NAME})

    # FetchContent_MakeAvailable sets <name>_SOURCE_DIR / <name>_BINARY_DIR as
    # normal (non-cache) variables in the calling scope. Because this call happens
    # inside a function(), those variables are local to this function and would
    # otherwise be invisible to CMakeLists.txt after it returns (e.g. glew_SOURCE_DIR,
    # stb_SOURCE_DIR ended up empty, breaking GLEW/stb include paths). Propagate them.
    set(${NAME}_SOURCE_DIR ${${NAME}_SOURCE_DIR} PARENT_SCOPE)
    set(${NAME}_BINARY_DIR ${${NAME}_BINARY_DIR} PARENT_SCOPE)

    if(NOT ${NAME}_POPULATED)
        message(FATAL_ERROR "Failed to download/populate ${NAME}. Possible causes:")
        message(FATAL_ERROR "  1. No internet connection")
        message(FATAL_ERROR "  2. GitHub is blocked or unreachable")
        message(FATAL_ERROR "  3. URL is incorrect: ${URL}")
        message(FATAL_ERROR "  4. Insufficient disk space")
        message(FATAL_ERROR "  5. Antivirus/firewall blocking downloads")
        message(FATAL_ERROR "Solution: Check internet connection, try manual download, or use system package manager")
    else()
        message(STATUS "✓ ${NAME} fetched successfully")
    endif()
endfunction()

# 1. pybind11
CheckFetchContentDependency(pybind11 https://github.com/pybind/pybind11/archive/refs/tags/v2.11.1.tar.gz)

# 1-1. nlohmann_json
CheckFetchContentDependency(json https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz)

# 1-2. fmt
CheckFetchContentDependency(fmt https://github.com/fmtlib/fmt/archive/refs/tags/10.1.1.tar.gz)

# 1-3. glm
CheckFetchContentDependency(glm https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz)

# 1-4. glew
CheckFetchContentDependency(glew https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip)
set(GLEW_SOURCE_DIR "${glew_SOURCE_DIR}/build/cmake" CACHE PATH "" FORCE)
# GLEW_SOURCE_DIR was set above but nothing ever add_subdirectory()'d it -- the glew_s
# target referenced by CMakeLists.txt's BASE_LIBRARIES was therefore never created.
# CMake silently treats an unknown target name in target_link_libraries() as a raw
# linker input, so this only surfaced at link time as LNK1181 ("cannot open input file
# glew_s.lib"), long after configure succeeded. GLEW's CMakeLists.txt lives nested under
# build/cmake (not at the fetched archive's root), which is also why
# FetchContent_MakeAvailable's automatic add_subdirectory() never picked it up.
if(NOT TARGET glew_s)
    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)  # skip glewinfo/visualinfo executables -- only the glew_s library is needed
    add_subdirectory(${GLEW_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/deps/glew-build)
endif()

# 1-5. glfw - removed (Python editor doesn't need GLFW)
message(STATUS "GLFW dependency removed (using Python editor)")

# 1-6. stb (for texture loading)
CheckFetchContentDependency(stb https://github.com/nothings/stb/archive/refs/heads/master.zip)

# 1-7. tinyobjloader
CheckFetchContentDependency(tinyobjloader https://github.com/tinyobjloader/tinyobjloader/archive/refs/heads/master.zip)
if(TARGET tinyobjloader)
    # tinyobjloader vendors fast_float, whose constexpr digit-parsing helpers fail to
    # compile under MSVC 19.44 (C3615: "cannot produce a constant expression"). Confirmed
    # this reproduces under C++20 too (not a language-standard issue), and we're already
    # pinned to refs/heads/master (i.e. already latest) so there's no newer version to move
    # to. tinyobjloader ships an official opt-out for exactly this: defining
    # TINYOBJLOADER_DISABLE_FAST_FLOAT switches to its built-in (slower, but not
    # MSVC-19.44-broken) float parser instead of the vendored fast_float path. Applied here
    # for the separately-built tinyobjloader.lib target; renderer/Mesh.cpp defines the same
    # macro for its own `#define TINYOBJLOADER_IMPLEMENTATION` + include.
    target_compile_definitions(tinyobjloader PUBLIC TINYOBJLOADER_DISABLE_FAST_FLOAT)
endif()

message(STATUS "========================================")
message(STATUS "All dependencies fetched successfully")
message(STATUS "========================================")
