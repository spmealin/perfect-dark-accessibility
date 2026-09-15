set(PRISM_ROOT "${CMAKE_SOURCE_DIR}/third_party/prism")
set(PRISM_RUNTIME_SOURCE "${PRISM_ROOT}/bin/prism.dll")
set(PRISM_INCLUDE_DIRECTORY "${PRISM_ROOT}/include")
set(PRISM_NOTICE_SOURCE "${PRISM_ROOT}/NOTICE")
set(PRISM_LICENSES_SOURCE "${PRISM_ROOT}/LICENSES")

if(NOT TARGET_IS_64BIT)
  message(FATAL_ERROR
    "The pinned Prism v0.18.2 Windows runtime supports x86-64 only. "
    "Use the default 64-bit Windows build for accessibility speech.")
endif()

foreach(PRISM_REQUIRED_PATH IN ITEMS
    "${PRISM_RUNTIME_SOURCE}"
    "${PRISM_INCLUDE_DIRECTORY}/prism.h"
    "${PRISM_INCLUDE_DIRECTORY}/prism_version.h"
    "${PRISM_NOTICE_SOURCE}"
    "${PRISM_LICENSES_SOURCE}")
  if(NOT EXISTS "${PRISM_REQUIRED_PATH}")
    message(FATAL_ERROR "Required Prism runtime input is missing: ${PRISM_REQUIRED_PATH}")
  endif()
endforeach()

file(SHA256 "${PRISM_RUNTIME_SOURCE}" PRISM_RUNTIME_SHA256)
if(NOT PRISM_RUNTIME_SHA256 STREQUAL
    "cb9712e11af9ebe96457dbf8f5daad4a6c359ae1f59cdf2663282b3a9cc9759c")
  message(FATAL_ERROR
    "The pinned Prism v0.18.2 runtime checksum does not match its recorded value.")
endif()

add_custom_target(prism_runtime_files ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PRISM_RUNTIME_SOURCE}"
    "${CMAKE_BINARY_DIR}/prism.dll"
  COMMAND "${CMAKE_COMMAND}" -E make_directory
    "${CMAKE_BINARY_DIR}/licenses/prism"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PRISM_NOTICE_SOURCE}"
    "${CMAKE_BINARY_DIR}/licenses/prism/NOTICE"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${PRISM_LICENSES_SOURCE}"
    "${CMAKE_BINARY_DIR}/licenses/prism/LICENSES"
  DEPENDS
    "${PRISM_RUNTIME_SOURCE}"
    "${PRISM_NOTICE_SOURCE}"
  VERBATIM
)
