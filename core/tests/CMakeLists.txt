# Unit tests for simcore, using doctest (fetched in core/cmake/Dependencies.cmake).

add_executable(simcore_tests
  version_test.cpp)
target_link_libraries(simcore_tests PRIVATE
  simcore_lib
  simcore_warnings
  doctest::doctest_with_main)
target_compile_definitions(simcore_tests PRIVATE
  SIMCORE_EXPECTED_VERSION="${PROJECT_VERSION}")

doctest_discover_tests(simcore_tests)
