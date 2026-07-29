# =============================================================================
# SimpleEngineTests — Test Pipeline
# =============================================================================

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${rapidcheck_SOURCE_DIR}/extras/gtest/include)

# Core sources (excluding main.cpp)
file(GLOB_RECURSE CORE_SOURCES 
    "../core/*.h"
    "../core/*.cpp"
)

file(GLOB_RECURSE PLATFORM_SOURCES
    "../platform/*.h"
    "../platform/*.cpp"
)

file(GLOB_RECURSE JOB_SOURCES
    "../job/*.h"
    "../job/*.cpp"
)

file(GLOB_RECURSE ECS_SOURCES
    "../ecs/*.h"
    "../ecs/*.cpp"
)
list(FILTER ECS_SOURCES EXCLUDE REGEX "ScriptSystem\\.cpp$")

file(GLOB_RECURSE RENDERER_SOURCES
    "../renderer/*.h"
    "../renderer/*.cpp"
)

# =============================================================================
# Test executable — Active Unit & Property Tests
# =============================================================================
add_executable(SimpleEngineTests
    TestMain.cpp

    # --- ECS Core Tests ---
    ECSTests.cpp
    SystemParallelExecutionTests.cpp
    SimpleWorldSystemTests.cpp
    WorldSystemTests.cpp

    # --- Memory System Tests ---
    StackAllocatorTests.cpp
    PoolAllocatorTests.cpp
    MemorySystemTests.cpp
    MemoryAllocatorPropertyTests.cpp
    MemoryTrackerTests.cpp

    # --- Component & Registry Tests ---
    ComponentTypeRegistryTests.cpp
    ComponentArrayAccessPropertyTests.cpp

    # --- System Scheduling & Query Tests ---
    SystemUpdateOrderPropertyTests.cpp
    SystemUpdateOrderSimpleTests.cpp
    SystemComponentQueryPropertyTests.cpp
    SystemConcurrencyPropertyTests.cpp

    # --- Engine & Job Tests ---
    EngineTests.cpp
    JobSystemTests.cpp
    ResultPropertyTests.cpp
    StringHashTests.cpp

    # --- RenderGraph Tests ---
    HandleTypeContractTests.cpp
    RendererInstancingTests.cpp
    RenderGraphBasicTests.cpp
    RenderGraphBarrierTests.cpp
    RenderGraphBarrierPropertyTests.cpp
    RenderGraphResourceLifetimeTests.cpp
    RenderGraphExecuteTests.cpp
    RenderGraphPhysicalResourceTests.cpp
    RenderGraphPropertyTests.cpp
    RenderGraphUnitTests.cpp

    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${JOB_SOURCES}
    ${ECS_SOURCES}
    ${RENDERER_SOURCES}
)

# Link Google Test, RapidCheck, and engine dependencies
target_link_libraries(SimpleEngineTests PRIVATE gtest rapidcheck fmt::fmt glm::glm nlohmann_json::nlohmann_json glew_s glfw)
target_include_directories(SimpleEngineTests PRIVATE ${GLEW_TRUE_INCLUDE_DIR})

# Definitions
target_compile_definitions(SimpleEngineTests PRIVATE ENABLE_TESTS)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    target_compile_definitions(SimpleEngineTests PRIVATE _DEBUG)
endif()

# Add /FS flag for MSVC to handle parallel compilation
if(MSVC)
    target_compile_options(SimpleEngineTests PRIVATE /FS)
endif()

# Add test
add_test(NAME SimpleEngineTests COMMAND SimpleEngineTests)
