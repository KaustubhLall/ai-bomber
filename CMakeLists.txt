cmake_minimum_required(VERSION 3.16)
project(ai_bomber C)
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE AI_BOMBER_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT AI_BOMBER_GIT_SHA)
  set(AI_BOMBER_GIT_SHA "unknown")
endif()
add_compile_definitions(AI_BOMBER_GIT_SHA="${AI_BOMBER_GIT_SHA}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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

add_subdirectory(src)

if(AI_BOMBER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
