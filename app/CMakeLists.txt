find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ocb_studio
  main.cpp
  AppController.cpp
)

target_include_directories(ocb_studio
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ocb_studio
  PRIVATE
    Qt6::Widgets
    ocb_core
    ocb_ui
    ocb_tools_ifr
)

target_compile_features(ocb_studio PRIVATE cxx_std_20)

set_target_properties(ocb_studio PROPERTIES
  AUTOMOC ON
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
)

if(MSVC)
  target_compile_options(ocb_studio PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_studio PRIVATE -Wall -Wextra -Wpedantic)
endif()
