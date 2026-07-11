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

add_executable(test_replay_flame test_replay_flame.c)
target_link_libraries(test_replay_flame PRIVATE bomber_core)
add_test(NAME test_replay_flame COMMAND test_replay_flame)

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
  add_test(NAME test_alphazero COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_alphazero.py)
  set_tests_properties(test_alphazero PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    ENVIRONMENT "AI_BOMBER_TRAINING_LIB=$<TARGET_FILE:bomber_training>")
  add_test(NAME test_native_run_analysis
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_native_run_analysis.py)
  set_tests_properties(test_native_run_analysis PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

add_executable(test_playback_clock test_playback_clock.c)
target_link_libraries(test_playback_clock PRIVATE bomber_core)
add_test(NAME test_playback_clock COMMAND test_playback_clock)
add_executable(test_match_history test_match_history.c)
target_link_libraries(test_match_history PRIVATE bomber_core)
add_test(NAME test_match_history COMMAND test_match_history)

add_executable(test_training_api test_training_api.c)
target_link_libraries(test_training_api PRIVATE bomber_training)
if(WIN32)
  add_custom_command(TARGET test_training_api POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:bomber_training> $<TARGET_FILE_DIR:test_training_api>)
endif()
add_test(NAME test_training_api COMMAND test_training_api)

if(TARGET bomber_alphazero_native)
  add_test(NAME test_native_alphazero_smoke
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-smoke
      --fresh --iterations 1 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_smoke PROPERTIES TIMEOUT 60)
  add_test(NAME test_native_alphazero_resume
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-smoke
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_resume PROPERTIES
    DEPENDS test_native_alphazero_smoke TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_gate
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_gate.py
        ${CMAKE_BINARY_DIR}/native-alphazero-smoke)
    set_tests_properties(test_native_alphazero_gate PROPERTIES
      DEPENDS test_native_alphazero_resume TIMEOUT 20)
  endif()

  # KL-101 regression: a checkpoint trained at a non-default arena-crush-win-value must not
  # silently evaluate at the trainer's struct default when the flag isn't repeated - semantics
  # are inherited from the checkpoint's manifest, not from CLI defaults.
  add_test(NAME test_native_alphazero_semantic_fresh
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --fresh --iterations 1 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --arena-crush-win-value 0.15)
  set_tests_properties(test_native_alphazero_semantic_fresh PROPERTIES TIMEOUT 60)
  add_test(NAME test_native_alphazero_semantic_evaluate
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-games 1 --eval-simulations 2
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/eval-result.json
      --no-progress)
  set_tests_properties(test_native_alphazero_semantic_evaluate PROPERTIES
    DEPENDS test_native_alphazero_semantic_fresh TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_semantic_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_semantic_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke)
    set_tests_properties(test_native_alphazero_semantic_check PROPERTIES
      DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 20)
  endif()

  # KL-101 regression: extending --iterations on a resume must not silently re-derive a new
  # LR schedule horizon against the same accumulated global_updates (the bug that produced the
  # observed jump from ~1e-5 to 1.596e-4 mid-run). The horizon is pinned on first resolution
  # and inherited from the checkpoint on every later resume unless explicitly overridden.
  add_test(NAME test_native_alphazero_schedule_fresh
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-schedule-smoke
      --fresh --iterations 2 --games 2 --simulations 2
      --train-steps 4 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_schedule_fresh PROPERTIES TIMEOUT 60)
  add_test(NAME test_native_alphazero_schedule_extend
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-schedule-smoke
      --iterations 5 --games 2 --simulations 2
      --train-steps 4 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_schedule_extend PROPERTIES
    DEPENDS test_native_alphazero_schedule_fresh TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_schedule_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_schedule_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-schedule-smoke)
    set_tests_properties(test_native_alphazero_schedule_check PROPERTIES
      DEPENDS test_native_alphazero_schedule_extend TIMEOUT 20)
  endif()
endif()
