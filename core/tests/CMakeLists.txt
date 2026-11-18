# Unit tests for simcore, using doctest (see docs/adr/0002-cpp-test-framework.md).

include(FetchContent)

# Pinned release tarball with checksum: reproducible and no git needed at configure time.
# Version recorded in docs/setup.md.
FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
  SYSTEM # treat doctest headers as system headers: their warnings are not ours
)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

add_executable(simcore_tests
  version_test.cpp)
target_link_libraries(simcore_tests PRIVATE
  simcore_lib
  simcore_warnings
  doctest::doctest_with_main)
target_compile_definitions(simcore_tests PRIVATE
  SIMCORE_EXPECTED_VERSION="${PROJECT_VERSION}")

# Registers every TEST_CASE as its own CTest test, so ctest and CLion list them individually.
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(simcore_tests)
