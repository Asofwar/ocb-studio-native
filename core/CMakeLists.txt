add_library(ocb_core STATIC
  src/BuiltinFields.cpp
  src/BiosAnalysisService.cpp
  src/ChecksumCompensator.cpp
  src/FieldCatalog.cpp
  src/FieldValidation.cpp
  src/IfrFieldMapper.cpp
  src/MetadataDetection.cpp
  src/OcbProfile.cpp
  src/Preset.cpp
)

target_include_directories(ocb_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ocb_core PUBLIC cxx_std_20)

target_link_libraries(ocb_core
  PUBLIC
    ocb_tools_ifr
    ocb_tools_uefi
)

if(MSVC)
  target_compile_options(ocb_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
