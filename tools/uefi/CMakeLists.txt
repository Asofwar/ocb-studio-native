add_library(ocb_tools_uefi STATIC
  third_party/lzma/LzmaDec.c
  src/UefiExtractor.cpp
)

target_include_directories(ocb_tools_uefi
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lzma
)

target_compile_features(ocb_tools_uefi PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(ocb_tools_uefi PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_tools_uefi PRIVATE -Wall -Wextra -Wpedantic)
endif()
