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
# Folders follow the hexagon (docs/plan.md §2.1): domain/ (pure logic), ports/ (interfaces),
# adapters/ (JSON files, WebSocket, clock), app/ (application service, loop, wiring).
add_library(simcore_lib STATIC
  src/app/version.cpp
  src/app/cli.cpp
  src/app/simulation_service.cpp
  src/domain/ecu/table.cpp
  src/domain/ecu/ecu.cpp
  src/domain/engine/intake.cpp
  src/domain/engine/engine_model.cpp
  src/domain/engine/engine_simulation.cpp
  src/domain/dyno/dyno.cpp
  src/adapters/table_units.cpp
  src/adapters/json_file_repository.cpp
  src/adapters/protocol.cpp
  src/adapters/message_bus.cpp
  src/adapters/sweep_csv.cpp
  src/adapters/websocket_server.cpp)
target_include_directories(simcore_lib PUBLIC include src)
target_compile_features(simcore_lib PUBLIC cxx_std_20)
target_compile_definitions(simcore_lib PRIVATE SIMCORE_VERSION="${PROJECT_VERSION}")
target_link_libraries(simcore_lib
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE simcore_warnings ixwebsocket::ixwebsocket)

# --- Executable ---------------------------------------------------------------
add_executable(simcore
  src/app/main.cpp)
target_link_libraries(simcore PRIVATE simcore_lib simcore_warnings)
# Default for --shared during development; a packaged app passes the path explicitly.
target_compile_definitions(simcore PRIVATE SIMCORE_DEFAULT_SHARED_DIR="${PROJECT_SOURCE_DIR}/shared")

# --- Tests --------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# --- Spikes -------------------------------------------------------------------
if(SIMCORE_BUILD_SPIKES)
  add_subdirectory(spikes)
endif()
