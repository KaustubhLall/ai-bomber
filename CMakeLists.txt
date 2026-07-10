cmake_minimum_required(VERSION 3.16)
project(ai_bomber C)
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE AI_BOMBER_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT AI_BOMBER_GIT_SHA)
  set(AI_BOMBER_GIT_SHA "unknown")
else()
  execute_process(COMMAND git status --porcelain --untracked-files=all
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE AI_BOMBER_GIT_STATUS
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(AI_BOMBER_GIT_STATUS)
    string(APPEND AI_BOMBER_GIT_SHA "-dirty")
  endif()
endif()
add_compile_definitions(AI_BOMBER_GIT_SHA="${AI_BOMBER_GIT_SHA}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

option(AI_BOMBER_BUILD_VIZ "Build raylib visualizer" ON)
option(AI_BOMBER_BUILD_TESTS "Build tests" ON)
option(AI_BOMBER_BUILD_NATIVE_ALPHAZERO
       "Build the standalone C++/LibTorch AlphaZero trainer" OFF)
if(AI_BOMBER_BUILD_NATIVE_ALPHAZERO)
    enable_language(CXX)
endif()
set(AI_BOMBER_TORCH_ROOT "" CACHE PATH
    "Path to a LibTorch or Python torch package root")

add_subdirectory(src)

if(AI_BOMBER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
