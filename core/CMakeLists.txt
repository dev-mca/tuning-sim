# simcore: headless simulation process (see docs/adr/0001-architecture.md)

# Throwaway experiments (docs/plan.md §4). Built by default so that CI keeps them compiling.
option(SIMCORE_BUILD_SPIKES "Build the experiments in core/spikes" ON)

include(cmake/Dependencies.cmake)

# --- Shared compiler warnings -------------------------------------------------
# An INTERFACE target carries no code, only usage requirements. Every target that
# links it gets the same warning flags (similar to a shared parent POM in Maven).
add_library(simcore_warnings INTERFACE)
if(MSVC)
  target_compile_options(simcore_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(simcore_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

# --- Core library -------------------------------------------------------------
# All logic lives here so that the executable and the tests share it.
add_library(simcore_lib STATIC
  src/app/version.cpp)
target_include_directories(simcore_lib PUBLIC include)
target_compile_features(simcore_lib PUBLIC cxx_std_20)
target_compile_definitions(simcore_lib PRIVATE SIMCORE_VERSION="${PROJECT_VERSION}")
target_link_libraries(simcore_lib PRIVATE simcore_warnings)

# --- Executable ---------------------------------------------------------------
add_executable(simcore
  src/app/main.cpp)
target_link_libraries(simcore PRIVATE simcore_lib simcore_warnings)

# --- Tests --------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# --- Spikes -------------------------------------------------------------------
if(SIMCORE_BUILD_SPIKES)
  add_subdirectory(spikes)
endif()
