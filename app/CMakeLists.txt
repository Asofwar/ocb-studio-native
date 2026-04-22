find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(OCB_REQUIRE_STATIC_QT)
  if(DEFINED QT_FEATURE_static AND NOT QT_FEATURE_static)
    message(FATAL_ERROR
      "OCB_REQUIRE_STATIC_QT=ON requires a static Qt build. "
      "The Qt found by CMAKE_PREFIX_PATH is dynamic, so ocb_studio would still need Qt DLLs."
    )
  endif()

  get_target_property(_ocb_qt_core_type Qt6::Core TYPE)
  if(_ocb_qt_core_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR
      "OCB_REQUIRE_STATIC_QT=ON requires Qt6::Core to be a static library."
    )
  endif()
elseif(MSVC AND OCB_STATIC_MSVC_RUNTIME AND DEFINED QT_FEATURE_static AND NOT QT_FEATURE_static)
  message(WARNING
    "OCB_STATIC_MSVC_RUNTIME=ON links project targets with /MT, but the Qt package is dynamic. "
    "This can still require Qt and CRT DLLs through Qt; use OCB_REQUIRE_STATIC_QT=ON with a static Qt build "
    "for a true single-exe Windows build."
  )
endif()

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
    ocb_build_info
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

if(OCB_REQUIRE_STATIC_QT AND COMMAND qt_import_plugins)
  set(_ocb_qt_static_plugins)
  foreach(_ocb_qt_plugin IN ITEMS
    Qt6::QWindowsIntegrationPlugin
    Qt6::QWindowsVistaStylePlugin
  )
    if(TARGET ${_ocb_qt_plugin})
      list(APPEND _ocb_qt_static_plugins ${_ocb_qt_plugin})
    endif()
  endforeach()

  if(_ocb_qt_static_plugins)
    qt_import_plugins(ocb_studio INCLUDE ${_ocb_qt_static_plugins})
  endif()
endif()

if(MSVC)
  target_compile_options(ocb_studio PRIVATE /W4 /permissive-)
else()
  target_compile_options(ocb_studio PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32 AND NOT OCB_REQUIRE_STATIC_QT)
  get_target_property(_ocb_qmake_location Qt6::qmake IMPORTED_LOCATION)
  if(_ocb_qmake_location)
    get_filename_component(_ocb_qt_bin_dir "${_ocb_qmake_location}" DIRECTORY)
  endif()

  find_program(OCB_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS ${_ocb_qt_bin_dir}
    DOC "Path to Qt windeployqt executable"
  )

  if(OCB_WINDEPLOYQT_EXECUTABLE)
    set(OCB_PORTABLE_DIR "${CMAKE_BINARY_DIR}/dist/OCB Studio Native" CACHE PATH "Portable Windows package output directory")
    set(_ocb_portable_exe "${OCB_PORTABLE_DIR}/ocb_studio.exe")
    set(_ocb_portable_zip "${CMAKE_BINARY_DIR}/dist/OCB-Studio-Native-$<CONFIG>.zip")
    set(_ocb_copy_msvc_runtime_command)

    if(MSVC)
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_ocb_msvc_redist_arch x64)
      else()
        set(_ocb_msvc_redist_arch x86)
      endif()

      get_filename_component(_ocb_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
      get_filename_component(_ocb_msvc_tools_arch_dir "${_ocb_msvc_bin_dir}" DIRECTORY)
      get_filename_component(_ocb_msvc_tools_bin_dir "${_ocb_msvc_tools_arch_dir}" DIRECTORY)
      get_filename_component(_ocb_msvc_tools_version_dir "${_ocb_msvc_tools_bin_dir}" DIRECTORY)
      get_filename_component(_ocb_msvc_tools_dir "${_ocb_msvc_tools_version_dir}" DIRECTORY)
      get_filename_component(_ocb_msvc_tools_root_dir "${_ocb_msvc_tools_dir}" DIRECTORY)
      get_filename_component(_ocb_msvc_vc_dir "${_ocb_msvc_tools_root_dir}" DIRECTORY)
      file(GLOB _ocb_msvc_runtime_dlls
        "${_ocb_msvc_vc_dir}/Redist/MSVC/*/${_ocb_msvc_redist_arch}/Microsoft.VC*.CRT/*.dll"
      )

      if(_ocb_msvc_runtime_dlls)
        set(_ocb_copy_msvc_runtime_command
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_ocb_msvc_runtime_dlls} "${OCB_PORTABLE_DIR}"
        )
      else()
        message(WARNING
          "MSVC runtime redistributable DLLs were not found under ${_ocb_msvc_vc_dir}/Redist. "
          "The portable package may require the Visual C++ Redistributable to be installed."
        )
      endif()
    endif()

    add_custom_target(ocb_studio_portable
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${OCB_PORTABLE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OCB_PORTABLE_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ocb_studio>" "${_ocb_portable_exe}"
      COMMAND "${OCB_WINDEPLOYQT_EXECUTABLE}"
        "$<IF:$<CONFIG:Debug>,--debug,--release>"
        --compiler-runtime
        --dir "${OCB_PORTABLE_DIR}"
        "${_ocb_portable_exe}"
      ${_ocb_copy_msvc_runtime_command}
      DEPENDS ocb_studio
      BYPRODUCTS "${_ocb_portable_exe}"
      COMMENT "Creating portable Windows package in ${OCB_PORTABLE_DIR}"
      VERBATIM
    )

    add_custom_target(ocb_studio_portable_zip
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
      COMMAND ${CMAKE_COMMAND} -E tar cf "${_ocb_portable_zip}" --format=zip -- "OCB Studio Native"
      DEPENDS ocb_studio_portable
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
      BYPRODUCTS "${_ocb_portable_zip}"
      COMMENT "Creating portable Windows zip package ${_ocb_portable_zip}"
      VERBATIM
    )
  else()
    message(WARNING
      "windeployqt was not found. The ocb_studio_portable target will not be available."
    )
  endif()
endif()
