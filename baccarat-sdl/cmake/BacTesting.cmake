# Shared doctest main - compiled once
if(NOT TARGET bac_doctest_main)
  add_library(bac_doctest_main OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/doctest_main.cpp"
  )
  target_include_directories(bac_doctest_main PRIVATE ${doctest_SOURCE_DIR})
  set_target_properties(bac_doctest_main PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
  )
endif()

# Helper function to add a doctest executable
function(bac_add_doctest name)
  cmake_parse_arguments(
    BAC
    ""
    ""
    "SOURCES;LIBS"
    ${ARGN}
  )

  add_executable(${name} ${BAC_SOURCES})

  target_link_libraries(${name}
    PRIVATE
      doctest::doctest
      $<TARGET_OBJECTS:bac_doctest_main>
      ${BAC_LIBS}
  )

  target_include_directories(${name}
    PRIVATE
      "${doctest_SOURCE_DIR}"
  )

  set_target_properties(${name} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
  )

  bac_set_warnings(${name})

  add_test(NAME ${name} COMMAND ${name})
endfunction()

# Helper function to set compiler warnings
function(bac_set_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic
  )
endfunction()
