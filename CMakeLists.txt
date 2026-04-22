cmake_minimum_required(VERSION 3.24)

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

set(OCB_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "Версия сборки OCB Studio")
set(OCB_BUILD_GIT_SHA "unknown" CACHE STRING "Git SHA сборки OCB Studio")
set(OCB_BUILD_DATE "unknown" CACHE STRING "Дата сборки OCB Studio")

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

option(OCB_BUILD_TESTS "Собирать тесты OCB Studio" ON)
option(OCB_BUILD_UI "Собирать интерфейс Qt Widgets" ON)

add_subdirectory(tools)
add_subdirectory(core)

if(OCB_BUILD_UI)
  add_subdirectory(ui)
  add_subdirectory(app)
endif()

if(OCB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
