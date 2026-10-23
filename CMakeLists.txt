cmake_minimum_required(VERSION 3.14)
project(cml VERSION 2.0.0 LANGUAGES CXX
        DESCRIPTION "Custom Math Library: from-scratch numerical computing with a JSON engine, WebAssembly build and web UI")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CML_BUILD_TESTS "Build the unit tests" ON)
option(CML_BUILD_CLI "Build the cml-cli executable" ON)
option(CML_BUILD_SHARED "Build the engine as a shared library with a C ABI" OFF)
option(CML_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
set(CML_SOURCES
  src/core.cpp
  src/special.cpp
  src/json.cpp
  src/expression.cpp
  src/polynomial.cpp
  src/matrix.cpp
  src/roots.cpp
  src/optimize.cpp
  src/integrate.cpp
  src/ode.cpp
  src/interpolate.cpp
  src/stats.cpp
  src/engine.cpp
  src/c_api.cpp
)

add_library(cml STATIC ${CML_SOURCES})
add_library(cml::cml ALIAS cml)
target_include_directories(cml PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

function(cml_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    if(CML_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(CML_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    # 32-bit x86 GCC/Clang default to x87 excess precision, which breaks the
    # bit-exact algorithms (TwoProd, fmod). Force SSE2 arithmetic there.
    # (CMAKE_SYSTEM_PROCESSOR reports the host, so probe the compiler instead.)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      include(CheckCXXCompilerFlag)
      check_cxx_compiler_flag("-mfpmath=sse -msse2" CML_HAS_SSE_FPMATH)
      if(CML_HAS_SSE_FPMATH)
        target_compile_options(${target} PRIVATE -mfpmath=sse -msse2)
      endif()
    endif()
  endif()
  # The library relies on strict IEEE semantics; never allow fast-math.
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE -fno-fast-math)
  else()
    target_compile_options(${target} PRIVATE /fp:precise)
  endif()
endfunction()
cml_set_warnings(cml)

if(CML_BUILD_SHARED)
  add_library(cml_shared SHARED ${CML_SOURCES})
  target_include_directories(cml_shared PUBLIC include)
  target_compile_definitions(cml_shared PRIVATE CML_BUILD_DLL)
  set_target_properties(cml_shared PROPERTIES OUTPUT_NAME cml CXX_VISIBILITY_PRESET hidden)
  cml_set_warnings(cml_shared)
endif()

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
if(CML_BUILD_CLI AND NOT EMSCRIPTEN)
  add_executable(cml-cli apps/cli/main.cpp)
  target_link_libraries(cml-cli PRIVATE cml)
  cml_set_warnings(cml-cli)
endif()

# ---------------------------------------------------------------------------
# WebAssembly (configure with: emcmake cmake -S . -B build-wasm)
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  add_executable(cml_wasm src/wasm_entry.cpp)
  target_link_libraries(cml_wasm PRIVATE cml)
  set_target_properties(cml_wasm PROPERTIES OUTPUT_NAME "cml" SUFFIX ".js")
  target_link_options(cml_wasm PRIVATE
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s EXPORT_NAME=createCmlModule"
    "SHELL:-s ENVIRONMENT=web,worker,node"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s INITIAL_MEMORY=16MB"
    "SHELL:-s MAXIMUM_MEMORY=512MB"
    "SHELL:-s STACK_SIZE=1MB"
    "SHELL:-s FILESYSTEM=0"
    "SHELL:-s EXPORTED_FUNCTIONS=['_cml_execute','_cml_free','_cml_version','_malloc','_free']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['cwrap','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
    "-O3")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(CML_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_executable(cml-tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_json.cpp
    tests/test_expression.cpp
    tests/test_polynomial.cpp
    tests/test_matrix.cpp
    tests/test_numerics.cpp
    tests/test_engine.cpp)
  target_include_directories(cml-tests PRIVATE tests)
  target_link_libraries(cml-tests PRIVATE cml)
  cml_set_warnings(cml-tests)
  add_test(NAME cml-tests COMMAND cml-tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS cml EXPORT cmlTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/cml DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(CML_BUILD_CLI AND NOT EMSCRIPTEN)
  install(TARGETS cml-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(EXPORT cmlTargets NAMESPACE cml:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cml)
