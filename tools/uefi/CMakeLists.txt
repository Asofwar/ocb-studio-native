set(UEFITOOL_UPSTREAM ${CMAKE_CURRENT_SOURCE_DIR}/upstream)

add_library(ocb_uefitool_engine STATIC
  ${UEFITOOL_UPSTREAM}/common/guiddatabase.cpp
  ${UEFITOOL_UPSTREAM}/common/types.cpp
  ${UEFITOOL_UPSTREAM}/common/filesystem.cpp
  ${UEFITOOL_UPSTREAM}/common/descriptor.cpp
  ${UEFITOOL_UPSTREAM}/common/ffs.cpp
  ${UEFITOOL_UPSTREAM}/common/nvram.cpp
  ${UEFITOOL_UPSTREAM}/common/nvramparser.cpp
  ${UEFITOOL_UPSTREAM}/common/meparser.cpp
  ${UEFITOOL_UPSTREAM}/common/ffsparser.cpp
  ${UEFITOOL_UPSTREAM}/common/amd_microcode.cpp
  ${UEFITOOL_UPSTREAM}/common/fitparser.cpp
  ${UEFITOOL_UPSTREAM}/common/ffsreport.cpp
  ${UEFITOOL_UPSTREAM}/common/peimage.cpp
  ${UEFITOOL_UPSTREAM}/common/treeitem.cpp
  ${UEFITOOL_UPSTREAM}/common/treemodel.cpp
  ${UEFITOOL_UPSTREAM}/common/utility.cpp
  ${UEFITOOL_UPSTREAM}/common/brotli/common/constants.c
  ${UEFITOOL_UPSTREAM}/common/brotli/common/context.c
  ${UEFITOOL_UPSTREAM}/common/brotli/common/dictionary.c
  ${UEFITOOL_UPSTREAM}/common/brotli/common/platform.c
  ${UEFITOOL_UPSTREAM}/common/brotli/common/shared_dictionary.c
  ${UEFITOOL_UPSTREAM}/common/brotli/common/transform.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/bit_reader.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/decode.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/huffman.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/prefix.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/state.c
  ${UEFITOOL_UPSTREAM}/common/brotli/dec/static_init.c
  ${UEFITOOL_UPSTREAM}/common/LZMA/LzmaDecompress.c
  ${UEFITOOL_UPSTREAM}/common/LZMA/SDK/C/Bra.c
  ${UEFITOOL_UPSTREAM}/common/LZMA/SDK/C/Bra86.c
  ${UEFITOOL_UPSTREAM}/common/LZMA/SDK/C/CpuArch.c
  ${UEFITOOL_UPSTREAM}/common/LZMA/SDK/C/LzmaDec.c
  ${UEFITOOL_UPSTREAM}/common/Tiano/EfiTianoDecompress.c
  ${UEFITOOL_UPSTREAM}/common/ustring.cpp
  ${UEFITOOL_UPSTREAM}/common/bstrlib/bstrlib.c
  ${UEFITOOL_UPSTREAM}/common/bstrlib/bstrwrap.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/ami_nvar.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/apple_sysf.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/dell_dvar.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/edk2_vss.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/edk2_vss2.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/edk2_ftw.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/insyde_fdc.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/insyde_fdm.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/ms_slic_marker.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/ms_slic_pubkey.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/phoenix_flm.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/phoenix_evsa.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/intel_acbp_v1.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/intel_acbp_v2.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/intel_keym_v1.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/intel_keym_v2.cpp
  ${UEFITOOL_UPSTREAM}/common/generated/intel_acm.cpp
  ${UEFITOOL_UPSTREAM}/common/kaitai/kaitaistream.cpp
  ${UEFITOOL_UPSTREAM}/common/digest/sha1.c
  ${UEFITOOL_UPSTREAM}/common/digest/sha256.c
  ${UEFITOOL_UPSTREAM}/common/digest/sha512.c
  ${UEFITOOL_UPSTREAM}/common/digest/sm3.c
  ${UEFITOOL_UPSTREAM}/common/zlib/adler32.c
  ${UEFITOOL_UPSTREAM}/common/zlib/compress.c
  ${UEFITOOL_UPSTREAM}/common/zlib/crc32.c
  ${UEFITOOL_UPSTREAM}/common/zlib/deflate.c
  ${UEFITOOL_UPSTREAM}/common/zlib/gzclose.c
  ${UEFITOOL_UPSTREAM}/common/zlib/gzlib.c
  ${UEFITOOL_UPSTREAM}/common/zlib/gzread.c
  ${UEFITOOL_UPSTREAM}/common/zlib/gzwrite.c
  ${UEFITOOL_UPSTREAM}/common/zlib/inflate.c
  ${UEFITOOL_UPSTREAM}/common/zlib/infback.c
  ${UEFITOOL_UPSTREAM}/common/zlib/inftrees.c
  ${UEFITOOL_UPSTREAM}/common/zlib/inffast.c
  ${UEFITOOL_UPSTREAM}/common/zlib/trees.c
  ${UEFITOOL_UPSTREAM}/common/zlib/uncompr.c
  ${UEFITOOL_UPSTREAM}/common/zlib/zutil.c
)

target_include_directories(ocb_uefitool_engine
  PUBLIC
    ${UEFITOOL_UPSTREAM}
    ${UEFITOOL_UPSTREAM}/common
    ${UEFITOOL_UPSTREAM}/common/brotli
    ${UEFITOOL_UPSTREAM}/common/LZMA
    ${UEFITOOL_UPSTREAM}/common/LZMA/SDK/C
    ${UEFITOOL_UPSTREAM}/common/Tiano
    ${UEFITOOL_UPSTREAM}/common/bstrlib
    ${UEFITOOL_UPSTREAM}/common/kaitai
    ${UEFITOOL_UPSTREAM}/common/digest
    ${UEFITOOL_UPSTREAM}/common/zlib
)

target_compile_definitions(ocb_uefitool_engine
  PUBLIC
    U_ENABLE_NVRAM_PARSING_SUPPORT
    U_ENABLE_ME_PARSING_SUPPORT
    U_ENABLE_FIT_PARSING_SUPPORT
    U_ENABLE_GUID_DATABASE_SUPPORT
)

set_target_properties(ocb_uefitool_engine PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_library(ocb_tools_uefi STATIC
  src/UefiExtractor.cpp
)

target_include_directories(ocb_tools_uefi
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${UEFITOOL_UPSTREAM}
    ${UEFITOOL_UPSTREAM}/common
)

target_link_libraries(ocb_tools_uefi
  PRIVATE
    ocb_uefitool_engine
)

target_compile_features(ocb_tools_uefi PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(ocb_uefitool_engine PRIVATE /W0)
  target_compile_options(ocb_tools_uefi PRIVATE /W0 /permissive-)
else()
  target_compile_options(ocb_uefitool_engine PRIVATE -w)
  target_compile_options(ocb_tools_uefi PRIVATE -w)
endif()
