include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(UNIX AND NOT APPLE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7
)

FetchContent_MakeAvailable(glfw imgui)

add_library(ocb_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ocb_imgui
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(ocb_imgui
  PUBLIC
    glfw
)

if(WIN32)
  target_link_libraries(ocb_imgui PUBLIC opengl32)
elseif(APPLE)
  find_library(OpenGL_LIBRARY OpenGL REQUIRED)
  target_link_libraries(ocb_imgui PUBLIC ${OpenGL_LIBRARY})
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(ocb_imgui PUBLIC OpenGL::GL)
endif()

add_executable(ocb_studio WIN32
  main.cpp
  AppController.cpp
)

target_include_directories(ocb_studio
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ocb_studio
  PRIVATE
    ocb_build_info
    ocb_core
    ocb_imgui
    ocb_tools_ifr
    ocb_tools_uefi
)

if(WIN32)
  target_link_libraries(ocb_studio PRIVATE comdlg32)
endif()

target_compile_features(ocb_studio PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(ocb_imgui PRIVATE /W0)
  target_compile_options(ocb_studio PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_imgui PRIVATE -w)
  target_compile_options(ocb_studio PRIVATE -Wall -Wextra -Wpedantic)
endif()
