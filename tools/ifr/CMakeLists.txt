add_library(ocb_tools_ifr STATIC
  src/IfrTextParser.cpp
  src/NativeIfrExtractor.cpp
  upstream-universal/UEFI.cpp
)

target_include_directories(ocb_tools_ifr
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/upstream-universal
)

target_compile_features(ocb_tools_ifr PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(ocb_tools_ifr PRIVATE /W4 /permissive-)
  set_source_files_properties(upstream-universal/UEFI.cpp PROPERTIES COMPILE_OPTIONS /W0)
else()
  target_compile_options(ocb_tools_ifr PRIVATE -Wall -Wextra -Wpedantic)
  set_source_files_properties(upstream-universal/UEFI.cpp PROPERTIES COMPILE_OPTIONS -w)
endif()
