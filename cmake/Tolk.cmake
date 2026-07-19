set(TOLK_ROOT "${CMAKE_SOURCE_DIR}/third_party/tolk")
set(TOLK_SOURCE_DIR "${TOLK_ROOT}/src")

if(NOT EXISTS "${TOLK_SOURCE_DIR}/Tolk.cpp")
  message(FATAL_ERROR
    "Tolk sources are missing. Run: git submodule update --init --recursive")
endif()

set(TOLK_SAPI_SOURCE "${TOLK_SOURCE_DIR}/ScreenReaderDriverSAPI.cpp")

add_library(tolk SHARED
  "${TOLK_SOURCE_DIR}/Tolk.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverJAWS.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverNVDA.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverSA.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverSNova.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverWE.cpp"
  "${TOLK_SOURCE_DIR}/ScreenReaderDriverZT.cpp"
  "${TOLK_SAPI_SOURCE}"
  "${TOLK_SOURCE_DIR}/fsapi.c"
  "${TOLK_SOURCE_DIR}/wineyes.c"
  "${TOLK_SOURCE_DIR}/zt.c"
  "${TOLK_SOURCE_DIR}/Tolk.rc"
)

target_include_directories(tolk PRIVATE "${TOLK_SOURCE_DIR}")
target_compile_definitions(tolk PRIVATE _EXPORTING UNICODE _UNICODE)
target_link_libraries(tolk PRIVATE user32 ole32 oleaut32)

if(MINGW)
  set_property(SOURCE "${TOLK_SAPI_SOURCE}" APPEND PROPERTY
    COMPILE_DEFINITIONS INITGUID)
  set_property(SOURCE "${TOLK_SAPI_SOURCE}" APPEND PROPERTY
    COMPILE_OPTIONS -include windows.h)
  target_link_options(tolk PRIVATE -static-libgcc -static-libstdc++)
endif()

set_target_properties(tolk PROPERTIES
  OUTPUT_NAME "Tolk"
  PREFIX ""
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET_IS_64BIT)
  set(TOLK_CONTROLLER_SOURCE "${TOLK_ROOT}/libs/x64/nvdaControllerClient64.dll")
else()
  set(TOLK_CONTROLLER_SOURCE "${TOLK_ROOT}/libs/x86/nvdaControllerClient32.dll")
endif()

get_filename_component(TOLK_CONTROLLER_NAME "${TOLK_CONTROLLER_SOURCE}" NAME)

add_custom_target(tolk_runtime_files ALL
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/licenses/tolk"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${TOLK_CONTROLLER_SOURCE}"
    "${CMAKE_BINARY_DIR}/${TOLK_CONTROLLER_NAME}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${TOLK_ROOT}/LICENSE.txt"
    "${CMAKE_BINARY_DIR}/licenses/tolk/LICENSE.txt"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${TOLK_ROOT}/LICENSE-NVDA.txt"
    "${CMAKE_BINARY_DIR}/licenses/tolk/LICENSE-NVDA.txt"
  DEPENDS
    "${TOLK_CONTROLLER_SOURCE}"
    "${TOLK_ROOT}/LICENSE.txt"
    "${TOLK_ROOT}/LICENSE-NVDA.txt"
  VERBATIM
)
