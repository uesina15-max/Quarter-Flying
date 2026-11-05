cmake_minimum_required(VERSION 3.15)
project("Quarter Flying")
if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()
cmake_policy(SET CMP0135 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# 엔진 버전. Player 템플릿의 runtime_manifest.json과 quarterflying.__version__에 같이 들어가서,
# Build Game이 "에디터와 템플릿이 같은 엔진인가"를 확인하는 데 쓴다(docs/GAME_BUILD_PLAN.md 빌드 단계 1).
set(QF_ENGINE_VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Include custom modules
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)

# Include directories: 전역 include_directories(${CMAKE_CURRENT_SOURCE_DIR}) 대신 엔진 타깃에만 붙인다
# (아래 target_include_directories). 전역으로 두면 이 디렉터리에서 FetchContent로 추가되는 서드파티
# 타깃(googletest, rapidcheck 등)까지 엔진 헤더 경로를 물려받는다(docs/REVIEW_BASED_IMPROVEMENT_PLAN.md P2-3).

# Source lists
set(PLATFORM_SOURCES
    core/PlatformFactory.cpp
    platform/Win32Platform.cpp
)

set(CORE_SOURCES
    core/Engine.cpp
    core/CommandManager.cpp
    core/Transaction.cpp
    core/logging/Logger.cpp
    core/memory/FrameAllocator.cpp
    core/memory/MemoryTracker.cpp
    core/UUID.cpp
    ${PLATFORM_SOURCES}
)

set(EDITOR_SOURCES
    editor/EditorAPI.cpp
    editor/commands/MoveEntityCommand.cpp
    editor/commands/RotateEntityCommand.cpp
    editor/commands/ScaleEntityCommand.cpp
    editor/commands/CreateEntityCommand.cpp
    editor/commands/DestroyEntityCommand.cpp
    editor/commands/DuplicateEntityCommand.cpp
    editor/commands/AddComponentCommand.cpp
    editor/commands/RemoveComponentCommand.cpp
    editor/commands/InstantiatePrefabCommand.cpp
    editor/commands/RevertPrefabInstanceCommand.cpp
    editor/commands/EntitySnapshot.cpp
    editor/commands/SetParentCommand.cpp
    editor/commands/SetComponentFieldCommand.cpp
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
    renderer/Texture2D.cpp
renderer/Shader.cpp
    renderer/Camera.cpp
    renderer/Frustum.cpp
    renderer/InstancedBatchManager.cpp
    renderer/LODSystem.cpp
    renderer/OcclusionCulling.cpp
    renderer/DynamicResolution.cpp
    renderer/DebugGridRenderer.cpp
    renderer/BoneLineRenderer.cpp
    renderer/TransformGizmo.cpp
    renderer/GameOverlay.cpp
    renderer/SceneMeshRenderer.cpp
    renderer/ParticleRenderer.cpp
)

set(ECS_SOURCES
    ecs/World.cpp
    ecs/WorldManager.cpp
    ecs/Reflection.cpp
    ecs/RotationConversion.cpp
    ecs/ScriptSystem.cpp
    ecs/ScriptContext.cpp
    ecs/RenderSystem.cpp
    ecs/CameraSystem.cpp
    ecs/CameraRigSystem.cpp
    ecs/KeyboardMovementSystem.cpp
    ecs/StaticCollision.cpp
    animation/LocomotionSystem.cpp
    ecs/Picking.cpp
    ecs/CameraGizmoSystem.cpp
    ecs/Hierarchy.cpp
    input/InputState.cpp
    input/InputActions.cpp
    ecs/ParticleSystem.cpp
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
    asset/MaterialAsset.cpp
)

set(ANIMATION_SOURCES
    animation/Skeleton.cpp
    animation/SkeletonSignature.cpp
    animation/AnimationSampler.cpp
    animation/AnimationPlayer.cpp
    animation/SkeletonJsonIO.cpp
    animation/ClipJsonIO.cpp
    animation/LayerMixer.cpp
    animation/MotionPreviewState.cpp
)

set(PREFAB_SOURCES
    prefab/PrefabAsset.cpp
)

# 씬 파일 저장/불러오기 (docs/GAME_BUILD_PLAN.md Phase 1)
set(SCENE_SOURCES
    scene/SceneIO.cpp
)

# 2. Engine Shared Library
add_library(quarterflying_engine SHARED
    ${CORE_SOURCES}
    ${EDITOR_SOURCES}
    ${JOB_SOURCES}
    ${RENDERER_SOURCES}
    ${ECS_SOURCES}
    ${ASSET_SOURCES}
    ${ANIMATION_SOURCES}
    ${PREFAB_SOURCES}
    ${SCENE_SOURCES}
)

get_filename_component(GLEW_TRUE_INCLUDE_DIR "${glew_SOURCE_DIR}/include" ABSOLUTE)
message(STATUS "GLEW_TRUE_INCLUDE_DIR: ${GLEW_TRUE_INCLUDE_DIR}")

get_filename_component(STB_INCLUDE_DIR "${stb_SOURCE_DIR}" ABSOLUTE)
message(STATUS "STB_INCLUDE_DIR: ${STB_INCLUDE_DIR}")

target_include_directories(quarterflying_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GLEW_TRUE_INCLUDE_DIR} ${STB_INCLUDE_DIR})

# Base libraries (always required - GLEW needed for OpenGL in Qt editor too)
# pybind11::embed: ecs/ScriptSystem.cpp가 이미 실행 중인 인터프리터 안에서 pybind11 API(py::module_::import,
# py::object, GIL)를 쓰므로 엔진 DLL이 Python 라이브러리에 링크돼야 한다. "쓰지 않는 링크"가 아니다
# (개선안 P2-4 정정).
set(BASE_LIBRARIES pybind11::embed nlohmann_json::nlohmann_json fmt::fmt glm::glm glew_s tinyobjloader)

# GLFW removed (using Qt-based editor with native window handles)
message(STATUS "GLFW dependency excluded (using Qt-based editor)")

target_link_libraries(quarterflying_engine PUBLIC ${BASE_LIBRARIES})

if(MSVC)
    target_compile_definitions(quarterflying_engine PRIVATE GE_EXPORTS)
    target_compile_options(quarterflying_engine PRIVATE /FS)
    target_link_libraries(quarterflying_engine PRIVATE opengl32.lib user32.lib gdi32.lib)
    target_link_libraries(quarterflying_engine PRIVATE gdiplus.lib)
    # Enable AddressSanitizer for MSVC (Visual Studio 2019+)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND MSVC_VERSION GREATER_EQUAL 1920)
        option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
        if(ENABLE_ASAN)
            target_compile_options(quarterflying_engine PRIVATE /fsanitize=address)
            target_link_options(quarterflying_engine PRIVATE /fsanitize=address)
        endif()
    endif()
endif()

# 3. Python Bindings Module
pybind11_add_module(quarterflying
    bindings/PythonModule.cpp
    bindings/MathBindings.cpp
    bindings/ECSBindings.cpp
    bindings/EngineBindings.cpp
    bindings/EditorBindings.cpp
    bindings/AnimationBindings.cpp
)
target_link_libraries(quarterflying PRIVATE quarterflying_engine)
target_compile_definitions(quarterflying PRIVATE QF_ENGINE_VERSION="${QF_ENGINE_VERSION}")

# 3-1. 게임 실행 파일(Player) — docs/GAME_BUILD_PLAN.md Phase 2.
# Build Game은 이 실행 파일을 컴파일하지 않고 복사해 이름만 바꾼다(D1). Python은 내장한다(D2).
add_executable(quarterflying_player
    runtime/PlayerMain.cpp
    runtime/GameConfig.cpp
)
target_link_libraries(quarterflying_player PRIVATE quarterflying_engine)
option(QF_PLAYER_CONSOLE "게임 실행 파일을 콘솔 창과 함께 빌드" ON)
if(WIN32 AND NOT QF_PLAYER_CONSOLE)
    set_target_properties(quarterflying_player PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(quarterflying_player PRIVATE /ENTRY:mainCRTStartup)
endif()

# 빌드에 쓰인 Python 설치 폴더. 개발 빌드(build/Release)에서 Player를 실행할 때 표준 라이브러리를
# 찾는 데만 쓴다(PlayerMain.cpp ConfigurePython). Build Game이 조립한 폴더는 python314._pth를 쓰므로
# 이 경로를 보지 않는다.
get_filename_component(QF_PYTHON_BASE "${PYTHON_INCLUDE_DIRS}/.." ABSOLUTE)
get_filename_component(QF_PYTHON_DLL_NAME "${PYTHON_LIBRARIES}" NAME_WE)   # python314
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/qf_dev_python_home.h"
     "#pragma once
#define QF_DEV_PYTHON_HOME L\"${QF_PYTHON_BASE}\"
")
target_include_directories(quarterflying_player PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Player가 Qt 없이 쓰는 Python 파일. 원본은 하나만 둔다 — 액션 재생 규칙은 에디터와 같은 코드다.
set(QF_RUNTIME_PYTHON_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/python/runtime_bootstrap.py
)
set(QF_RUNTIME_PYTHON_CORE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/core/__init__.py
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/core/action_data.py
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/core/action_playback.py
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/core/scene_action_playback.py
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/core/sound_player.py
)
# 개발 빌드에서 바로 실행할 수 있게, 실행 파일 옆에 Python DLL과 런타임 Python 파일을 둔다.
add_custom_command(TARGET quarterflying_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QF_PYTHON_BASE}/${QF_PYTHON_DLL_NAME}.dll" "${QF_PYTHON_BASE}/python3.dll"
            "$<TARGET_FILE_DIR:quarterflying_player>"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:quarterflying_player>/python/core"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QF_RUNTIME_PYTHON_FILES}
            "$<TARGET_FILE_DIR:quarterflying_player>/python"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QF_RUNTIME_PYTHON_CORE_FILES}
            "$<TARGET_FILE_DIR:quarterflying_player>/python/core"
    COMMENT "Staging Python runtime next to quarterflying_player"
)

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

# Only add tests directory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

# 4-1. Player 템플릿 (docs/GAME_BUILD_PLAN.md Phase 3)
# Build Game이 복사해 가는 원본 폴더(runtime/win-x64)를 조립한다. 표준 라이브러리 zip을 매번 다시 만들면
# 빌드가 느려지므로 기본 빌드(ALL)에는 넣지 않는다:
#     cmake --build . --config Release --target player_template
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)          # -> CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS (VC 런타임 DLL 경로)
set(QF_TEMPLATE_DIR "${CMAKE_BINARY_DIR}/runtime/win-x64")
add_custom_target(player_template
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/runtime/stage_template.py"
            --out "${QF_TEMPLATE_DIR}"
            --bin "$<TARGET_FILE_DIR:quarterflying_player>"
            --python-home "${QF_PYTHON_BASE}"
            --python-dll "${QF_PYTHON_DLL_NAME}"
            --pyd "$<TARGET_FILE_NAME:quarterflying>"
            --engine-version "${QF_ENGINE_VERSION}"
            --runtime-python ${QF_RUNTIME_PYTHON_FILES}
            --runtime-python-core ${QF_RUNTIME_PYTHON_CORE_FILES}
            --extra-dll ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
    COMMENT "Staging Player template into ${QF_TEMPLATE_DIR}"
    VERBATIM
)
add_dependencies(player_template quarterflying_player quarterflying)

# 템플릿 자체 검증: 템플릿 폴더의 Player로 시험용 게임을 30프레임 돌린다(Python·Qt가 없는 조건과 같음 —
# 템플릿의 python314._pth가 site-packages를 막는다).
add_test(NAME player_template_smoke
    COMMAND "${QF_TEMPLATE_DIR}/quarterflying_player.exe"
            --game "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/smoke_game/game.json" --smoke-test 30)

# 5. Installation — 에디터 완제품(②)의 폴더 구성(docs/GAME_BUILD_PLAN.md §3.1)
#   bin/            엔진 DLL, quarterflying.pyd, VC 런타임
#   editor/         에디터 Python (테스트 파일 제외)
#   assets/         샘플 프로젝트 에셋(프로젝트를 열지 않았을 때 쓰는 샘플 = 설치 폴더 자체)
#   runtime/win-x64 Player 템플릿(player_template 타깃의 결과)
install(TARGETS quarterflying_engine
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)
install(TARGETS quarterflying
    LIBRARY DESTINATION bin
    RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin)

install(DIRECTORY editor/
    DESTINATION editor
    FILES_MATCHING PATTERN "*.py"
    PATTERN "__pycache__" EXCLUDE
    PATTERN "test_*.py" EXCLUDE
)

install(DIRECTORY assets/
    DESTINATION assets
)

install(CODE "
    if(NOT EXISTS \"${QF_TEMPLATE_DIR}/runtime_manifest.json\")
        message(FATAL_ERROR \"Player template not staged. Build the 'player_template' target first.\")
    endif()
")
install(DIRECTORY "${QF_TEMPLATE_DIR}/" DESTINATION runtime/win-x64)

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
