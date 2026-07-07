# Core simulation library
add_library(bomber_core STATIC
    core/rng.c
    core/config.c
    core/math_util.c
    core/ring_buffer.c
    core/replay.c
    core/metrics.c
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
    sim/runner.c
    sim/benchmark.c
    sim/evaluator.c
)

target_include_directories(bomber_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# CLI: headless runner
add_executable(bomber_headless cli/main_headless.c)
target_link_libraries(bomber_headless PRIVATE bomber_core)

# CLI: benchmark
add_executable(bomber_benchmark cli/main_benchmark.c)
target_link_libraries(bomber_benchmark PRIVATE bomber_core)

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
