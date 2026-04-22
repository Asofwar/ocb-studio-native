add_executable(ocb_core_tests
  OcbCoreTests.cpp
)

target_link_libraries(ocb_core_tests
  PRIVATE
    ocb_core
    ocb_tools_ifr
    ocb_tools_uefi
)

target_compile_definitions(ocb_core_tests
  PRIVATE
    OCB_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/.."
)

if(MSVC)
  target_compile_options(ocb_core_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ocb_core_tests COMMAND ocb_core_tests)
