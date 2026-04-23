add_library(ocb_tools_ifr STATIC
  src/IfrTextParser.cpp
  src/NativeIfrExtractor.cpp
)

target_include_directories(ocb_tools_ifr
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ocb_tools_ifr PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(ocb_tools_ifr PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_tools_ifr PRIVATE -Wall -Wextra -Wpedantic)
endif()
