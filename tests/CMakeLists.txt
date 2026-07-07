if(MSVC)
    # C tests use assert() as their executable contract. Release normally defines
    # NDEBUG, which silently removed both checks and setup calls inside assert.
    add_compile_options(/wd4189 /UNDEBUG)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-unused-variable -Wno-unused-but-set-variable -UNDEBUG)
endif()

add_executable(test_rng test_rng.c)
target_link_libraries(test_rng PRIVATE bomber_core)
add_test(NAME test_rng COMMAND test_rng)

add_executable(test_bomber_env test_bomber_env.c)
target_link_libraries(test_bomber_env PRIVATE bomber_core)
add_test(NAME test_bomber_env COMMAND test_bomber_env)

add_executable(test_map test_map.c)
target_link_libraries(test_map PRIVATE bomber_core)
add_test(NAME test_map COMMAND test_map)

add_executable(test_bombs test_bombs.c)
target_link_libraries(test_bombs PRIVATE bomber_core)
add_test(NAME test_bombs COMMAND test_bombs)

add_executable(test_blast test_blast.c)
target_link_libraries(test_blast PRIVATE bomber_core)
add_test(NAME test_blast COMMAND test_blast)

add_executable(test_danger test_danger.c)
target_link_libraries(test_danger PRIVATE bomber_core)
add_test(NAME test_danger COMMAND test_danger)

add_executable(test_observation test_observation.c)
target_link_libraries(test_observation PRIVATE bomber_core)
add_test(NAME test_observation COMMAND test_observation)

add_executable(test_reward test_reward.c)
target_link_libraries(test_reward PRIVATE bomber_core)
add_test(NAME test_reward COMMAND test_reward)

add_executable(test_replay test_replay.c)
target_link_libraries(test_replay PRIVATE bomber_core)
add_test(NAME test_replay COMMAND test_replay)

add_executable(test_agents test_agents.c)
target_link_libraries(test_agents PRIVATE bomber_core)
add_test(NAME test_agents COMMAND test_agents)

add_executable(test_determinism test_determinism.c)
target_link_libraries(test_determinism PRIVATE bomber_core)
add_test(NAME test_determinism COMMAND test_determinism)

add_executable(test_hardened test_hardened.c)
target_link_libraries(test_hardened PRIVATE bomber_core)
add_test(NAME test_hardened COMMAND test_hardened)

add_executable(test_search_api test_search_api.c)
target_link_libraries(test_search_api PRIVATE bomber_core)
add_test(NAME test_search_api COMMAND test_search_api)
add_executable(test_search_agents test_search_agents.c)
target_link_libraries(test_search_agents PRIVATE bomber_core)
add_test(NAME test_search_agents COMMAND test_search_agents)
add_executable(test_tournament test_tournament.c)
target_link_libraries(test_tournament PRIVATE bomber_core)
add_test(NAME test_tournament COMMAND test_tournament)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME test_learning COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_learning.py)
  set_tests_properties(test_learning PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

add_executable(test_playback_clock test_playback_clock.c)
target_link_libraries(test_playback_clock PRIVATE bomber_core)
add_test(NAME test_playback_clock COMMAND test_playback_clock)
add_executable(test_match_history test_match_history.c)
target_link_libraries(test_match_history PRIVATE bomber_core)
add_test(NAME test_match_history COMMAND test_match_history)
