# Opt-in instrumentation for project-owned regression coverage.
option(OPENKAL_ENABLE_COVERAGE "Instrument GCC builds for optimization-scope coverage" OFF)

if(OPENKAL_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "OPENKAL_ENABLE_COVERAGE currently requires GCC and matching gcov")
  endif()
  if(NOT OPENKAL_BUILD_TESTS)
    message(FATAL_ERROR "OPENKAL_ENABLE_COVERAGE requires OPENKAL_BUILD_TESTS=ON")
  endif()
  find_program(OPENKAL_GCOV_EXECUTABLE gcov REQUIRED)
  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
endif()
