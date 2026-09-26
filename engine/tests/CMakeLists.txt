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

file(GLOB_RECURSE ANIMATION_SOURCES
    "../animation/*.h"
    "../animation/*.cpp"
)

file(GLOB_RECURSE PREFAB_SOURCES
    "../prefab/*.h"
    "../prefab/*.cpp"
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

    # --- Command / Transaction Tests ---
    TransactionTests.cpp
    RotateScaleEntityCommandTests.cpp
    CreateDestroyEntityCommandTests.cpp
    AddRemoveComponentCommandTests.cpp
    EditorCommandIntegrationTests.cpp

    # --- Editor Command Sources ---
    ../editor/commands/MoveEntityCommand.cpp
    ../editor/commands/RotateEntityCommand.cpp
    ../editor/commands/ScaleEntityCommand.cpp
    ../editor/commands/CreateEntityCommand.cpp
    ../editor/commands/DestroyEntityCommand.cpp
    ../editor/commands/AddComponentCommand.cpp
    ../editor/commands/RemoveComponentCommand.cpp
    ../editor/commands/InstantiatePrefabCommand.cpp
    ../editor/commands/RevertPrefabInstanceCommand.cpp

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

    # --- Motion Mixer / Animation Tests ---
    SkeletonTests.cpp
    AnimationSamplerTests.cpp
    AnimationJsonIOTests.cpp
    LayerMixerTests.cpp
    MotionPreviewStateTests.cpp

    # --- RenderSystem Tests (P0-2) ---
    RenderSystemTests.cpp

    # --- Sound Lite Tests (Phase 5) ---
    ActionPlayerComponentTests.cpp

    # --- VFX Lite Tests (Phase 1 / Phase 2 / Phase 5) ---
    ParticleEffectComponentTests.cpp
    ParticleSystemTests.cpp
    ParticlePrefabTests.cpp

    # --- Prefab Tests (P1.6, Phase 1 / Phase 2) ---
    PrefabAssetTests.cpp
    InstantiatePrefabCommandTests.cpp
    RevertPrefabInstanceCommandTests.cpp

    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${JOB_SOURCES}
    ${ECS_SOURCES}
    ${RENDERER_SOURCES}
    ${ANIMATION_SOURCES}
    ${PREFAB_SOURCES}
)

# Link Google Test, RapidCheck, and engine dependencies
# tinyobjloader: RENDERER_SOURCES(glob)에 Mesh.cpp가 포함되고 tiny_obj_loader.h를 참조하는데,
# 이 타겟은 그동안 tinyobjloader를 링크하지 않아 include 디렉터리가 전파되지 않고 있었다
# (quarterflying_engine은 BASE_LIBRARIES를 통해 링크하므로 문제없이 빌드됨 - 이 테스트 타겟만 누락).
# glfw: 프로젝트 전체에서 이미 제거된 의존성인데("GLFW dependency excluded" - Dependencies.cmake)
# 이 링크 목록에만 죽은 참조로 남아 있어서 링크 단계에서 LNK1181로 실패하고 있었다. 제거.
target_link_libraries(SimpleEngineTests PRIVATE gtest rapidcheck fmt::fmt glm::glm nlohmann_json::nlohmann_json glew_s tinyobjloader)
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
