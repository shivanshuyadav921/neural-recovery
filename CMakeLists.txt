cmake_minimum_required(VERSION 3.20)

project(neuroparse
  VERSION 0.1.0
  DESCRIPTION "Neural error recovery inside a compiler parsing pipeline"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NEUROPARSE_BUILD_TESTS "Build C++ unit tests" ON)
# GoogleTest is the intended framework. Fetch is off by default so Phase 1
# configure does not require GitHub. Enable with -DNEUROPARSE_FETCH_GTEST=ON.
option(NEUROPARSE_FETCH_GTEST "Download GoogleTest via FetchContent" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  message(FATAL_ERROR
    "GCC ${CMAKE_CXX_COMPILER_VERSION} cannot compile C++20. "
    "Use GCC 11+, Clang 12+, or MSVC 2019+. Do not use MinGW GCC 6.3.")
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(neuroparse_core STATIC
  compiler/src/version.cpp
  compiler/src/cli/cli.cpp
  compiler/src/lexer/lexer.cpp
  compiler/src/parser/parser.cpp
  compiler/src/ast/ast_printer.cpp
  compiler/src/recovery/panic_mode.cpp
  compiler/src/recovery/heuristic_recovery.cpp
  compiler/src/recovery/error_recovery_controller.cpp
  compiler/src/recovery/error_context.cpp
  compiler/src/recovery/context_extractor.cpp
  compiler/src/recovery/repair_validator.cpp
  compiler/src/recovery/repair_applier.cpp
  compiler/src/recovery/neural_recovery.cpp
  compiler/src/fuzz/invariants.cpp
)
target_include_directories(neuroparse_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/include
)
target_compile_features(neuroparse_core PUBLIC cxx_std_20)

add_executable(neuroparse compiler/src/cli/main.cpp)
target_link_libraries(neuroparse PRIVATE neuroparse_core)

if(NEUROPARSE_BUILD_TESTS)
  enable_testing()
  add_executable(neuroparse_unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_version.cpp
    tests/unit/test_cli.cpp
    tests/unit/test_lexer.cpp
    tests/unit/test_parser.cpp
    tests/unit/test_ast.cpp
    tests/unit/test_recovery.cpp
    tests/unit/test_context_extractor.cpp
    tests/unit/test_repair_validator.cpp
    tests/unit/test_repair_applier.cpp
    tests/unit/test_neural_recovery.cpp
    tests/unit/test_fuzz.cpp
  )
  target_include_directories(neuroparse_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit)
  target_link_libraries(neuroparse_unit_tests PRIVATE neuroparse_core)
  add_test(NAME neuroparse_unit_tests_all COMMAND neuroparse_unit_tests)
  add_test(NAME test_version COMMAND neuroparse_unit_tests --suite=version)
  add_test(NAME test_cli COMMAND neuroparse_unit_tests --suite=cli)
  add_test(NAME test_lexer COMMAND neuroparse_unit_tests --suite=lexer)
  add_test(NAME test_parser COMMAND neuroparse_unit_tests --suite=parser)
  add_test(NAME test_ast COMMAND neuroparse_unit_tests --suite=ast)
  add_test(NAME test_recovery COMMAND neuroparse_unit_tests --suite=recovery)
  add_test(NAME test_context_extractor COMMAND neuroparse_unit_tests --suite=context_extractor)
  add_test(NAME test_repair_validator COMMAND neuroparse_unit_tests --suite=repair_validator)
  add_test(NAME test_repair_applier COMMAND neuroparse_unit_tests --suite=repair_applier)
  add_test(NAME test_neural_recovery COMMAND neuroparse_unit_tests --suite=neural_recovery)
  add_test(NAME test_fuzz COMMAND neuroparse_unit_tests --suite=fuzz)

  if(NEUROPARSE_FETCH_GTEST)
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
    target_link_libraries(neuroparse_unit_tests PRIVATE GTest::gtest)
  endif()
endif()

install(TARGETS neuroparse)
