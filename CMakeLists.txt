cmake_minimum_required(VERSION 3.24)

project(OCBStudioNative
  VERSION 0.1.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OCB_BUILD_TESTS "Build OCB Studio tests" ON)
option(OCB_BUILD_UI "Build Qt Widgets UI" ON)

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
