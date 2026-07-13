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
      --overwrite-evidence
      --no-progress)
  set_tests_properties(test_native_alphazero_semantic_evaluate PROPERTIES
    DEPENDS test_native_alphazero_semantic_fresh TIMEOUT 60)
  add_test(NAME test_native_alphazero_semantic_draw_alias_evaluate
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-games 1 --eval-simulations 2 --draw-value -0.7
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/eval-draw-alias.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_semantic_draw_alias_evaluate PROPERTIES
    DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_semantic_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_semantic_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke)
    set_tests_properties(test_native_alphazero_semantic_check PROPERTIES
      DEPENDS test_native_alphazero_semantic_draw_alias_evaluate TIMEOUT 20)
  endif()
  add_test(NAME test_native_alphazero_evidence_refuses_overwrite
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-games 1 --eval-simulations 2
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/eval-result.json
      --no-progress)
  set_tests_properties(test_native_alphazero_evidence_refuses_overwrite PROPERTIES
    DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 20 WILL_FAIL TRUE)

  # KL-107 v3 regression: trace-output must carry a genuinely raw (pre-mask) policy/value
  # recomputation, not another copy of the search's already-masked prior - the v1/v2 bug this
  # guards against is exactly the earlier raw_policy mislabeling (see the correction recorded
  # in Linear KL-98/KL-100 and docs/experiment-memory/10-autonomous-brick-execution.md).
  add_test(NAME test_native_alphazero_trace_raw_evaluate
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-games 1 --eval-simulations 2
      --eval-mcts --mcts-eval-games 4
      --baseline-mcts-simulations 4 --baseline-mcts-depth 4
      --trace-output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/trace-raw-check.jsonl
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/eval-trace-raw.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_trace_raw_evaluate PROPERTIES
    DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_trace_raw_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_trace_raw_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/trace-raw-check.jsonl)
    set_tests_properties(test_native_alphazero_trace_raw_check PROPERTIES
      DEPENDS test_native_alphazero_trace_raw_evaluate TIMEOUT 20)
  endif()

  # KL-105 Phase 2b: deterministic tactical gates. Reuses the same tiny fixture checkpoint as
  # the semantic/trace tests above (native-alphazero-semantic-smoke) rather than training a
  # dedicated one - gates is read-only and channel/blocks/max-steps must match that checkpoint's
  # config_signature exactly to load it at all, same requirement the trace/semantic tests above
  # already satisfy with this identical flag set.
  add_test(NAME test_native_alphazero_gates_run
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-network-1.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_run PROPERTIES
    DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 120)
  # Run again to a second output path, same checkpoint/flags/seeds - the determinism check
  # (test_native_alphazero_gates_check.py) diffs the two scenarios arrays byte-for-byte.
  add_test(NAME test_native_alphazero_gates_rerun
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-network-2.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_rerun PROPERTIES
    DEPENDS test_native_alphazero_gates_run TIMEOUT 120)
  # Achievability reference: substitutes the heuristic agent for the network entirely. Proves
  # the gates are solvable by something, not just an aspirational target no policy could ever
  # pass. See test_native_alphazero_gates_check.py for exactly which scenarios are pinned and
  # why (empirically determined - not all six; see that file's header comment).
  add_test(NAME test_native_alphazero_gates_agent_run
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8 --gates-agent heuristic
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-heuristic.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_agent_run PROPERTIES
    DEPENDS test_native_alphazero_gates_rerun TIMEOUT 60)

  # KL-110 Phase B: aligned-opponent gate ablation. --gates-opponent-model self, run explicitly,
  # must byte-match the no-flag default run above (gates-network-1.json) on the scenarios array -
  # this, not rerun-determinism (already covered by gates_rerun above), is what proves "self" is
  # really the unchanged default and not a second, subtly different code path.
  add_test(NAME test_native_alphazero_gates_self_explicit_run
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8 --gates-opponent-model self
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-self-explicit.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_self_explicit_run PROPERTIES
    DEPENDS test_native_alphazero_gates_agent_run TIMEOUT 60)
  # Two independent --gates-opponent-model aligned runs - determinism must hold in aligned mode
  # exactly like it does in self mode (gates_run/gates_rerun above), checked the same way (diff
  # the scenarios arrays byte-for-byte in the python check, not just "both exit 0").
  add_test(NAME test_native_alphazero_gates_aligned_run
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8 --gates-opponent-model aligned
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-aligned-1.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_aligned_run PROPERTIES
    DEPENDS test_native_alphazero_gates_self_explicit_run TIMEOUT 60)
  add_test(NAME test_native_alphazero_gates_aligned_rerun
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> gates
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke
      --checkpoint latest.pt
      --channels 16 --blocks 1 --max-steps 8 --replay-capacity 128
      --eval-simulations 8 --gates-opponent-model aligned
      --output ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/gates-aligned-2.json
      --overwrite-evidence --no-progress)
  set_tests_properties(test_native_alphazero_gates_aligned_rerun PROPERTIES
    DEPENDS test_native_alphazero_gates_aligned_run TIMEOUT 60)

  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_gates_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_gates_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke)
    set_tests_properties(test_native_alphazero_gates_check PROPERTIES
      DEPENDS test_native_alphazero_gates_aligned_rerun TIMEOUT 20)

    # KL-110 Phase B verdict tool: pure-Python logic, no native binary/GPU dependency at
    # runtime, but deliberately kept INSIDE this TARGET-guarded block (not the dependency-free
    # Python block near the top of this file) so the non-GPU "build" tree's test count stays
    # untouched - this tool exists specifically to consume gates_format_version 2 evidence,
    # which only the native alphazero target can produce.
    add_test(NAME test_analyze_h3a_verdict
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_analyze_h3a_verdict.py)
    set_tests_properties(test_analyze_h3a_verdict PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()

  add_test(NAME test_native_alphazero_unknown_option_fails
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate --definitely-unknown)
  set_tests_properties(test_native_alphazero_unknown_option_fails PROPERTIES
    TIMEOUT 20 WILL_FAIL TRUE)
  add_test(NAME test_native_alphazero_missing_value_fails
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> evaluate --eval-games)
  set_tests_properties(test_native_alphazero_missing_value_fails PROPERTIES
    TIMEOUT 20 WILL_FAIL TRUE)

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

  # KL-101 Part C regression: --fork-from must inherit weights/semantics/champion-lineage from
  # an external checkpoint, record full provenance, and NOT re-trigger the "iter-110 fake
  # initial promotion" bug (has_incumbent used to be a pure best.pt file-existence check, blind
  # to inherited lineage - see write_fork_manifest()'s best.pt materialization in trainer.cpp).
  add_test(NAME test_native_alphazero_fork_parent
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-fork-parent
      --fresh --iterations 1 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 64
      --channels 8 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --arena-crush-win-value 0.22)
  set_tests_properties(test_native_alphazero_fork_parent PROPERTIES TIMEOUT 60)
  # Advance latest.pt beyond the retained champion so the fork regression proves child
  # best.pt is the parent's exact champion artifact, not a relabeled copy of current weights.
  add_test(NAME test_native_alphazero_fork_parent_advance
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-fork-parent
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 64
      --channels 8 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2 --promotion-margin 0.49
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_fork_parent_advance PROPERTIES
    DEPENDS test_native_alphazero_fork_parent TIMEOUT 60)
  add_test(NAME test_native_alphazero_fork_child
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-fork-child
      --fresh --fork-from ${CMAKE_BINARY_DIR}/native-alphazero-fork-parent/latest.pt
      --iterations 3 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 64
      --channels 8 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --dirty-diff-digest ctest-regression-digest)
  set_tests_properties(test_native_alphazero_fork_child PROPERTIES
    DEPENDS test_native_alphazero_fork_parent_advance TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_fork_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_fork_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-fork-parent
        ${CMAKE_BINARY_DIR}/native-alphazero-fork-child)
    set_tests_properties(test_native_alphazero_fork_check PROPERTIES
      DEPENDS test_native_alphazero_fork_child TIMEOUT 20)

    # A historical bug could leave current/latest tensors in best.pt while selection metadata
    # still named an older champion. Ordinary resume must detect that mismatch, not silently
    # keep using the fake incumbent. The script restores the fixture in a finally block.
    add_test(NAME test_native_alphazero_corrupt_best_check
      COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_corrupt_best_check.py
        $<TARGET_FILE:bomber_alphazero_native>
        ${CMAKE_BINARY_DIR}/native-alphazero-fork-parent)
    set_tests_properties(test_native_alphazero_corrupt_best_check PROPERTIES
      DEPENDS test_native_alphazero_fork_check RUN_SERIAL TRUE TIMEOUT 30)
  endif()

  # KL-105 Phase 3 (docs/experiment-memory/13-kl105-experiment-design.md section 3): capped
  # cause-balanced replay sampling. cap=0 (the default, exercised by every test above this
  # block) must stay bit-for-bit the pre-existing uniform sampler; these tests exercise cap>0.
  add_test(NAME test_native_alphazero_cause_balance_fresh
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-smoke
      --fresh --iterations 2 --games 2 --simulations 2
      --train-steps 4 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --replay-cause-balance-cap 0.25)
  set_tests_properties(test_native_alphazero_cause_balance_fresh PROPERTIES TIMEOUT 60)
  # Resume with NO --replay-cause-balance-cap override - inherited from the manifest (0.25, not
  # the compiled default 0) exactly like any other semantic field on an ordinary resume.
  add_test(NAME test_native_alphazero_cause_balance_resume
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-smoke
      --iterations 3 --games 2 --simulations 2
      --train-steps 4 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_cause_balance_resume PROPERTIES
    DEPENDS test_native_alphazero_cause_balance_fresh TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_cause_balance_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_cause_balance_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-smoke)
    set_tests_properties(test_native_alphazero_cause_balance_check PROPERTIES
      DEPENDS test_native_alphazero_cause_balance_resume TIMEOUT 20)
  endif()

  # Fork from the shared semantic-smoke fixture (built earlier in this same suite; its cap is
  # left at the compiled default 0, explicitly present in its manifest since it was built by
  # THIS binary) with an explicit cap override - exercises the normal semantic-fork path
  # (present key, value differs) end to end. Do NOT resume/mutate the shared fixture in place -
  # other tests above depend on it; --fork-from only reads it (load_checkpoint), never writes.
  # The parent finished at iteration 1 (--iterations 1 from iteration 0), so --iterations 2
  # here is exactly one new iteration, not zero - fork inherits the parent's iteration counter,
  # it does not reset to 0 (see fork_child's identical "iterations = parent + 1" pattern above).
  add_test(NAME test_native_alphazero_cause_balance_fork_legacy
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-fork-legacy
      --fresh --fork-from ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/latest.pt
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --replay-cause-balance-cap 0.25)
  set_tests_properties(test_native_alphazero_cause_balance_fork_legacy PROPERTIES
    DEPENDS test_native_alphazero_semantic_evaluate TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_cause_balance_fork_legacy_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_cause_balance_fork_legacy_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-fork-legacy)
    set_tests_properties(test_native_alphazero_cause_balance_fork_legacy_check PROPERTIES
      DEPENDS test_native_alphazero_cause_balance_fork_legacy TIMEOUT 20)
  endif()

  # KL-110 hygiene regression: --fresh must truncate semantic-fork-log.jsonl exactly like it
  # truncates metrics.jsonl/config-history.jsonl. Re-run the IDENTICAL --fresh --fork-from
  # command above into the SAME run-dir a second time, after the first run/check pair above has
  # already validated the pre-rerun state - on the old (buggy) code this second run would leave
  # TWO accumulated semantic-fork-log.jsonl entries (one per run); on fixed code, exactly one
  # (only the current run's own).
  add_test(NAME test_native_alphazero_cause_balance_fork_legacy_rerun
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-fork-legacy
      --fresh --fork-from ${CMAKE_BINARY_DIR}/native-alphazero-semantic-smoke/latest.pt
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --replay-cause-balance-cap 0.25)
  set_tests_properties(test_native_alphazero_cause_balance_fork_legacy_rerun PROPERTIES
    DEPENDS test_native_alphazero_cause_balance_fork_legacy_check TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_cause_balance_fork_legacy_rerun_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_cause_balance_fork_legacy_rerun_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-fork-legacy)
    set_tests_properties(test_native_alphazero_cause_balance_fork_legacy_rerun_check PROPERTIES
      DEPENDS test_native_alphazero_cause_balance_fork_legacy_rerun TIMEOUT 20)
  endif()

  # Fork from a checkpoint SNAPSHOTTED before this feature existed (tests/fixtures/native-
  # alphazero-pre-cause-tag-checkpoint.pt - byte-for-byte the semantic-smoke fixture as it
  # existed at git commit 58f688bd4f9b, one commit before this feature's own work started, so
  # its archive genuinely has no "replay_tags" key and its semantic manifest genuinely has no
  # "replay_cause_balance_cap" key - unlike the shared semantic-smoke fixture above, which is
  # rebuilt fresh by the CURRENT binary every suite run and therefore always has both). Proves
  # the actual missing-key load path (load_checkpoint()/ReplayBuffer::load() default every
  # inherited sample's tags to 0/unknown, no error, no crash), not just the "key present but 0"
  # path the fork_legacy test above exercises. Same "parent iteration 1 -> --iterations 2 for
  # exactly one new iteration" accounting as above (identical parent checkpoint, copied before
  # any KL-105 Phase 3 code existed).
  add_test(NAME test_native_alphazero_cause_balance_true_legacy_fork
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-true-legacy
      --fresh --fork-from ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/native-alphazero-pre-cause-tag-checkpoint.pt
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 100 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --replay-cause-balance-cap 0.25)
  set_tests_properties(test_native_alphazero_cause_balance_true_legacy_fork PROPERTIES
    TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_cause_balance_true_legacy_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_cause_balance_true_legacy_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-cause-balance-true-legacy)
    set_tests_properties(test_native_alphazero_cause_balance_true_legacy_check PROPERTIES
      DEPENDS test_native_alphazero_cause_balance_true_legacy_fork TIMEOUT 20)
  endif()

  # v7 Stage 0 item 0.1+0.3 (docs/experiment-memory/14-v7-from-scratch-design.md): temperature_
  # final/temperature_anneal semantic-field machinery. Same fresh -> resume(inherit) ->
  # resume(explicit override) shape as the cause-balance chain above, in one dedicated run-dir:
  # fresh sets --temperature-anneal --temperature-final 0.25 --temperature-steps 4; the resume
  # passes NO temperature flags at all (must inherit silently); the third invocation passes only
  # --temperature-final 0.1 (must log a SEMANTIC FORK line, while temperature_anneal/
  # temperature_steps keep inheriting). --eval-interval 1 on all three doubles this fixture as
  # the drift-canary (item 0.5) fixture below - every metrics.jsonl row gets a "gates" object.
  add_test(NAME test_native_alphazero_temperature_anneal_fresh
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-temperature-anneal-smoke
      --fresh --iterations 1 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --temperature-anneal --temperature-final 0.25 --temperature-steps 4)
  set_tests_properties(test_native_alphazero_temperature_anneal_fresh PROPERTIES TIMEOUT 60)
  # No temperature flags at all - both fields must inherit silently from the fresh run's manifest.
  add_test(NAME test_native_alphazero_temperature_anneal_resume
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-temperature-anneal-smoke
      --iterations 2 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_temperature_anneal_resume PROPERTIES
    DEPENDS test_native_alphazero_temperature_anneal_fresh TIMEOUT 60)
  # Only --temperature-final diverges from the inherited manifest value (0.25 -> 0.1) - must log
  # a SEMANTIC FORK line for temperature_final specifically, not temperature_anneal/steps.
  add_test(NAME test_native_alphazero_temperature_anneal_override
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-temperature-anneal-smoke
      --iterations 3 --games 2 --simulations 2
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --temperature-final 0.1)
  set_tests_properties(test_native_alphazero_temperature_anneal_override PROPERTIES
    DEPENDS test_native_alphazero_temperature_anneal_resume TIMEOUT 60)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_temperature_anneal_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_temperature_anneal_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-temperature-anneal-smoke)
    set_tests_properties(test_native_alphazero_temperature_anneal_check PROPERTIES
      DEPENDS test_native_alphazero_temperature_anneal_override TIMEOUT 20)

    # v7 Stage 0 item 0.5: the in-training drift canary (six tactical gates, search mode, run
    # in-process every eval interval - see run_gate_canary()/append_metrics() in trainer.cpp).
    # Reuses the temperature-anneal fixture above rather than a dedicated one - all three of its
    # train invocations pass --eval-interval 1, so every resulting metrics.jsonl row already
    # carries a "gates" object.
    add_test(NAME test_native_alphazero_drift_canary_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_drift_canary_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-temperature-anneal-smoke)
    set_tests_properties(test_native_alphazero_drift_canary_check PROPERTIES
      DEPENDS test_native_alphazero_temperature_anneal_override TIMEOUT 20)
  endif()

  # v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md): forced_playouts_k
  # semantic-field machinery. Same fresh -> resume(inherit) -> resume(explicit override) shape as
  # the temperature-anneal chain above, in its own dedicated run-dir: fresh sets
  # --forced-playouts-k 2; the resume passes NO forced-playouts flag at all (must inherit
  # silently); the third invocation passes only --forced-playouts-k 3 (must log a SEMANTIC FORK
  # line). --eval-interval 1 on all three doubles this fixture as the forced-playouts telemetry
  # fixture below (every metrics.jsonl row gets a well-formed "forced_playouts" object).
  # --simulations 32 (vs this file's usual --simulations 2) is the ONE deliberate deviation from
  # the temperature-anneal chain's flag shape: forcing's floor invariant (root_floor_violations()
  # in trainer.cpp) needs a large enough total-visit budget for the sqrt(k*P*N) floors of ALL
  # competing noised actions to become simultaneously satisfiable (their sum grows like sqrt(N),
  # the budget grows like N, so any fixed k/branching-factor combination eventually crosses over -
  # at --simulations 2 the search never leaves that early transient, so the invariant check would
  # be measuring an artifact of too-small a budget, not real forcing behavior).
  add_test(NAME test_native_alphazero_forced_playouts_fresh
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-forced-playouts-smoke
      --fresh --iterations 1 --games 2 --simulations 32
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --forced-playouts-k 2)
  set_tests_properties(test_native_alphazero_forced_playouts_fresh PROPERTIES TIMEOUT 90)
  # No forced-playouts flag at all - must inherit forced_playouts_k=2 silently from the manifest.
  add_test(NAME test_native_alphazero_forced_playouts_resume
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-forced-playouts-smoke
      --iterations 2 --games 2 --simulations 32
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress)
  set_tests_properties(test_native_alphazero_forced_playouts_resume PROPERTIES
    DEPENDS test_native_alphazero_forced_playouts_fresh TIMEOUT 90)
  # Only --forced-playouts-k diverges from the inherited manifest value (2 -> 3) - must log a
  # SEMANTIC FORK line.
  add_test(NAME test_native_alphazero_forced_playouts_override
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
      $<TARGET_FILE:bomber_alphazero_native> train
      --run-dir ${CMAKE_BINARY_DIR}/native-alphazero-forced-playouts-smoke
      --iterations 3 --games 2 --simulations 32
      --train-steps 1 --batch-size 4 --replay-capacity 128
      --channels 16 --blocks 1 --teacher-games 1 --teacher-iterations 1
      --eval-interval 1 --eval-games 1 --eval-simulations 2
      --promotion-games 1 --promotion-simulations 2
      --random-score-floor 0 --heuristic-score-floor 0
      --heuristic-regression-margin 1
      --snapshot-interval 1 --max-steps 8 --no-progress
      --forced-playouts-k 3)
  set_tests_properties(test_native_alphazero_forced_playouts_override PROPERTIES
    DEPENDS test_native_alphazero_forced_playouts_resume TIMEOUT 90)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_forced_playouts_semantic_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_forced_playouts_semantic_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-forced-playouts-smoke)
    set_tests_properties(test_native_alphazero_forced_playouts_semantic_check PROPERTIES
      DEPENDS test_native_alphazero_forced_playouts_override TIMEOUT 20)

    # Forcing-floor + pruning telemetry properties (v7 Stage 0 item 0.2 test plan items 3-4):
    # reuses the SAME fixture chain above (all three invocations pass forced_playouts_k>0 and
    # --eval-interval 1) rather than a dedicated one - same reuse pattern as the drift-canary
    # check reusing the temperature-anneal fixture.
    add_test(NAME test_native_alphazero_forced_playouts_metrics_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_forced_playouts_metrics_check.py
        ${CMAKE_BINARY_DIR}/native-alphazero-forced-playouts-smoke)
    set_tests_properties(test_native_alphazero_forced_playouts_metrics_check PROPERTIES
      DEPENDS test_native_alphazero_forced_playouts_override TIMEOUT 20)
  endif()

  # KL-101 Part D regression: a second `train` process must be refused while a first is still
  # running, even on a different run-dir - the actual failure mode that crashed training twice
  # earlier in this project's history. Self-contained (manages its own background process),
  # not part of the chained smoke/resume/gate/fork pattern above.
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_native_alphazero_lock_check
      COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${AI_BOMBER_TORCH_LIB_DIR}$<SEMICOLON>$ENV{PATH}"
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_native_alphazero_lock_check.py
        $<TARGET_FILE:bomber_alphazero_native>
        ${CMAKE_BINARY_DIR}/native-alphazero-lock-a
        ${CMAKE_BINARY_DIR}/native-alphazero-lock-b)
    set_tests_properties(test_native_alphazero_lock_check PROPERTIES TIMEOUT 60)
  endif()

  # KL-110 hygiene: select_replay_indices() (src/training/native/replay_sampling.h) is the
  # pure, Torch-free extraction of ReplayBuffer::batch()'s index-selection logic - deliberately
  # kept out of ReplayBuffer itself so this test can exercise the actual sampling algorithm (RNG
  # draws + pool-A boosting math) without linking LibTorch/CUDA. Plain assert()-based
  # executable, same convention as the dependency-free C tests above (this whole file keeps
  # NDEBUG off directory-wide). Wired here rather than the dependency-free "build" tree purely
  # for build-graph convenience alongside the rest of the native-alphazero test surface - the
  # header itself has no native-alphazero/LibTorch dependency, only this include path.
  add_executable(test_replay_sampling test_replay_sampling.cpp)
  target_include_directories(test_replay_sampling PRIVATE ${CMAKE_SOURCE_DIR}/src)
  add_test(NAME test_replay_sampling COMMAND test_replay_sampling)

  # v7 Stage 0 item 0.2: prune_policy_target_visits() (src/training/native/
  # policy_target_pruning.h) is the pure, Torch-free extraction of the KataGo
  # policy-target-pruning arithmetic - same rationale/wiring as test_replay_sampling above
  # (header has no native-alphazero/LibTorch dependency; wired here purely for build-graph
  # convenience alongside the rest of this test surface).
  add_executable(test_policy_target_pruning test_policy_target_pruning.cpp)
  target_include_directories(test_policy_target_pruning PRIVATE ${CMAKE_SOURCE_DIR}/src)
  add_test(NAME test_policy_target_pruning COMMAND test_policy_target_pruning)
endif()
