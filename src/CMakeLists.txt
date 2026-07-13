# Core simulation library
add_library(bomber_core STATIC
    core/rng.c
    core/config.c
    core/math_util.c
    core/ring_buffer.c
    core/replay.c
    core/metrics.c
    core/playback_clock.c
    core/match_history.c
    env/bomber_env.c
    env/bomber_rules.c
    env/bomber_map.c
    env/bomber_bombs.c
    env/bomber_blast.c
    env/bomber_danger.c
    env/bomber_observation.c
    env/bomber_reward.c
    agents/agent.c
    agents/random_agent.c
    agents/scripted_agent.c
    agents/heuristic_bomber_agent.c
    agents/greedy_crate_agent.c
    agents/search_agent.c
    agents/evasive_agent.c
    sim/runner.c
    sim/benchmark.c
    sim/evaluator.c
    training/encoding.c
)

set_target_properties(bomber_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(bomber_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Opaque shared-library bridge used by the Python AlphaZero trainer. Keeping the
# ABI narrow prevents ctypes from depending on internal C struct layouts.
add_library(bomber_training SHARED training/training_api.c)
target_link_libraries(bomber_training PRIVATE bomber_core)
target_include_directories(bomber_training PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CLI: headless runner
add_executable(bomber_headless cli/main_headless.c)
target_link_libraries(bomber_headless PRIVATE bomber_core)

# CLI: benchmark
add_executable(bomber_benchmark cli/main_benchmark.c)
target_link_libraries(bomber_benchmark PRIVATE bomber_core)

if(AI_BOMBER_BUILD_NATIVE_ALPHAZERO)
    if(NOT AI_BOMBER_TORCH_ROOT AND
       EXISTS "${CMAKE_SOURCE_DIR}/.venv-gpu/Lib/site-packages/torch")
        set(AI_BOMBER_TORCH_ROOT
            "${CMAKE_SOURCE_DIR}/.venv-gpu/Lib/site-packages/torch")
    endif()
    if(NOT EXISTS
       "${AI_BOMBER_TORCH_ROOT}/include/torch/csrc/api/include/torch/torch.h")
        message(FATAL_ERROR
            "AI_BOMBER_TORCH_ROOT must point to a LibTorch or Python torch package")
    endif()
    set(AI_BOMBER_TORCH_LIB_DIR "${AI_BOMBER_TORCH_ROOT}/lib")
    set(AI_BOMBER_TORCH_LIB_DIR "${AI_BOMBER_TORCH_LIB_DIR}"
        CACHE INTERNAL "LibTorch runtime directory" FORCE)
    set(AI_BOMBER_TORCH_LIBRARIES
        "${AI_BOMBER_TORCH_LIB_DIR}/torch.lib"
        "${AI_BOMBER_TORCH_LIB_DIR}/torch_cpu.lib"
        "${AI_BOMBER_TORCH_LIB_DIR}/torch_cuda.lib"
        "${AI_BOMBER_TORCH_LIB_DIR}/c10.lib"
        "${AI_BOMBER_TORCH_LIB_DIR}/c10_cuda.lib")
    add_executable(bomber_alphazero_native
        training/native/main.cpp
        training/native/model.cpp
        training/native/trainer.cpp
    )
    target_link_libraries(bomber_alphazero_native PRIVATE
        bomber_core ${AI_BOMBER_TORCH_LIBRARIES})
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(bomber_alphazero_native PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(bomber_alphazero_native PRIVATE
            AI_BOMBER_NATIVE_OPENMP=1)
    endif()
    target_include_directories(bomber_alphazero_native PRIVATE
        "${AI_BOMBER_TORCH_ROOT}/include"
        "${AI_BOMBER_TORCH_ROOT}/include/torch/csrc/api/include")
    if(MSVC)
        target_compile_options(bomber_alphazero_native PRIVATE /bigobj /utf-8)
    endif()
    set_target_properties(bomber_alphazero_native PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${AI_BOMBER_TORCH_LIB_DIR};%PATH%")
endif()

# Visualizer (raylib)
if(AI_BOMBER_BUILD_VIZ)
    find_package(raylib QUIET)
    if(NOT raylib_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.0
        )
        set(FETCHCONTENT_QUIET OFF)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        FetchContent_MakeAvailable(raylib)
    endif()

    add_executable(bomber_viz
        viz/main_viz.c
        viz/renderer.c
        viz/dashboard.c
        viz/charts.c
        viz/ui_controls.c
        viz/viz_session.c
        viz/theme.c
        viz/layout.c
    )
    target_link_libraries(bomber_viz PRIVATE bomber_core raylib)
endif()
