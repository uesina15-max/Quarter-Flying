cmake_minimum_required(VERSION 3.15)
project("Quarter Flying")
if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()
cmake_policy(SET CMP0135 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Include custom modules
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source lists
set(PLATFORM_SOURCES
    core/PlatformFactory.cpp
    platform/Win32Platform.cpp
)

set(CORE_SOURCES
    core/Engine.cpp
    core/logging/Logger.cpp
    core/memory/FrameAllocator.cpp
    core/memory/MemoryTracker.cpp
    core/UUID.cpp
    ${PLATFORM_SOURCES}
)

set(JOB_SOURCES
    job/JobSystem.cpp
    job/WorkStealingDeque.cpp
    job/ValidationLayer.cpp
    job/JobLifecycleManager.cpp
    job/DependencyResolver.cpp
    job/JobScheduler.cpp
)

set(RENDERER_SOURCES
    renderer/Renderer.cpp
    renderer/RenderGraph.cpp
    renderer/RGBuilder.cpp
    renderer/opengl/OpenGLCommandList.cpp
    renderer/opengl/OpenGLResourceManager.cpp
    renderer/Mesh.cpp
    renderer/Shader.cpp
    renderer/Camera.cpp
    renderer/Frustum.cpp
    renderer/InstancedBatchManager.cpp
    renderer/LODSystem.cpp
    renderer/OcclusionCulling.cpp
    renderer/DynamicResolution.cpp
)

set(ECS_SOURCES
    ecs/World.cpp
    ecs/WorldManager.cpp
    ecs/Reflection.cpp
    ecs/ScriptSystem.cpp
    ecs/ComponentManager.cpp
    ecs/ComponentTypeRegistry.cpp
    ecs/ECSRegistry.cpp
    ecs/EntityManager.cpp
    ecs/SystemDependencyAnalyzer.cpp
    ecs/ParallelGroupBuilder.cpp
    ecs/SystemDispatcher.cpp
)

set(ASSET_SOURCES
    asset/AssetRegistry.cpp
    asset/AssetManager.cpp
    asset/importers/TextureImporter.cpp
)

# 2. Engine Shared Library
add_library(ge_engine SHARED
    ${CORE_SOURCES}
    ${JOB_SOURCES}
    ${RENDERER_SOURCES}
    ${ECS_SOURCES}
    ${ASSET_SOURCES}
)

get_filename_component(GLEW_TRUE_INCLUDE_DIR "${glew_SOURCE_DIR}/include" ABSOLUTE)
message(STATUS "GLEW_TRUE_INCLUDE_DIR: ${GLEW_TRUE_INCLUDE_DIR}")

get_filename_component(STB_INCLUDE_DIR "${stb_SOURCE_DIR}" ABSOLUTE)
message(STATUS "STB_INCLUDE_DIR: ${STB_INCLUDE_DIR}")

target_include_directories(ge_engine PUBLIC ${GLEW_TRUE_INCLUDE_DIR} ${STB_INCLUDE_DIR})

# Base libraries (always required - GLEW needed for OpenGL in Qt editor too)
set(BASE_LIBRARIES pybind11::embed nlohmann_json::nlohmann_json fmt::fmt glm::glm glew_s tinyobjloader)

# GLFW removed (using Qt-based editor with native window handles)
message(STATUS "GLFW dependency excluded (using Qt-based editor)")

target_link_libraries(ge_engine PUBLIC ${BASE_LIBRARIES})

if(MSVC)
    target_compile_definitions(ge_engine PRIVATE GE_EXPORTS)
    target_compile_options(ge_engine PRIVATE /FS)
    target_link_libraries(ge_engine PRIVATE opengl32.lib user32.lib gdi32.lib)
    # Enable AddressSanitizer for MSVC (Visual Studio 2019+)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_VERSION GREATER_EQUAL 1920)
        option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
        if(ENABLE_ASAN)
            target_compile_options(ge_engine PRIVATE /fsanitize=address)
            target_link_options(ge_engine PRIVATE /fsanitize=address)
        endif()
    endif()
endif()

# 3. Python Bindings Module
pybind11_add_module(ge_python 
    bindings/PythonModule.cpp
    bindings/MathBindings.cpp
    bindings/ECSBindings.cpp
    bindings/EngineBindings.cpp
)
target_link_libraries(ge_python PRIVATE ge_engine)

# 4. Simple test executable (optional - legacy GLFW main)
# Removed legacy GLFW main (using Qt-based editor instead)

# 5. Tests
enable_testing()
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    rapidcheck
    URL https://github.com/emil-e/rapidcheck/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(rapidcheck)

add_subdirectory(tests)

# 5. Installation (Python Editor as main entry point)
# Install Python module
install(TARGETS ge_python
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Install editor files
install(DIRECTORY editor/
    DESTINATION editor
    FILES_MATCHING PATTERN "*.py"
)

# Install assets
install(DIRECTORY assets/
    DESTINATION assets
)

# Create startup script
if(WIN32)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_editor.bat.in
        ${CMAKE_CURRENT_BINARY_DIR}/run_editor.bat
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/run_editor.bat
        DESTINATION .
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    )
else()
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_editor.sh.in
        ${CMAKE_CURRENT_BINARY_DIR}/run_editor.sh
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/run_editor.sh
        DESTINATION .
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    )
endif()

message(STATUS "Python editor configured as main entry point")