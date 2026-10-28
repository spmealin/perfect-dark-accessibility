# Intentionally opt-in and separate from the normal game and package targets.
# Adapter fixtures still include private implementations. Keep their special
# whole-program optimization local instead of changing game compiler flags.
add_custom_target(accessibility_tests)

set(accessibility_fixture_names
  cane_result cane_path landmark glossary monitor_image interaction
  precision_mapping weapon_target visibility)

foreach(diagnostics RANGE 0 1)
  foreach(fixture IN LISTS accessibility_fixture_names)
    set(target "accessibility_${fixture}_test_diagnostics_${diagnostics}")
    add_executable(${target} EXCLUDE_FROM_ALL
      "accessibility_${fixture}_test.c" assertions_enabled.c)
    target_compile_definitions(${target} PRIVATE
      ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=${diagnostics})
    # Game configurations commonly define NDEBUG. Tests must not inherit it.
    target_compile_options(${target} PRIVATE -O2 -UNDEBUG)
    target_link_libraries(${target} PRIVATE m)
    add_dependencies(${target} pd_headers)
    add_dependencies(accessibility_tests ${target})

    if(fixture STREQUAL "cane_result")
      target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/accessibility/accessibility_cane_result.c")
    elseif(fixture STREQUAL "cane_path")
      target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/accessibility/accessibility_cane_path.c")
    elseif(fixture STREQUAL "glossary")
      target_link_libraries(${target} PRIVATE "${SDL2_LIBRARY}")
    else()
      # Discard unrelated engine functions in implementation-including fixtures.
      target_compile_options(${target} PRIVATE -fwhole-program)
    endif()

    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${target} PROPERTIES
      LABELS "accessibility;diagnostics_${diagnostics}"
      TIMEOUT 60)
  endforeach()
endforeach()
