cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(OCBStudioNative
  VERSION 0.1.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8)
endif()

option(OCB_STATIC_MSVC_RUNTIME "Link the MSVC runtime statically with /MT" ON)

if(MSVC AND OCB_STATIC_MSVC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(OCB_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "OCB Studio build version")
set(OCB_BUILD_GIT_SHA "unknown" CACHE STRING "OCB Studio build Git SHA")
set(OCB_BUILD_DATE "unknown" CACHE STRING "OCB Studio build date")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInfo.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/ocb/BuildInfo.hpp
  @ONLY
)

add_library(ocb_build_info INTERFACE)
target_include_directories(ocb_build_info
  INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/include
)

option(OCB_BUILD_TESTS "Build OCB Studio tests" ON)
option(OCB_BUILD_APP "Build OCB Studio command line tool" ON)

add_subdirectory(tools)
add_subdirectory(core)

if(OCB_BUILD_APP)
  add_subdirectory(app)
endif()

if(OCB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
