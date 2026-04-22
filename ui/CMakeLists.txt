find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(ocb_ui STATIC
  src/FieldTableModel.cpp
  src/MainWindow.cpp
)

target_include_directories(ocb_ui
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
)

target_link_libraries(ocb_ui
  PUBLIC
    Qt6::Widgets
    ocb_core
)

target_compile_features(ocb_ui PUBLIC cxx_std_20)

set_target_properties(ocb_ui PROPERTIES
  AUTOMOC ON
)

if(MSVC)
  target_compile_options(ocb_ui PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
