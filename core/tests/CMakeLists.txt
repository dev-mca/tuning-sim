# Unit and integration tests for simcore, using doctest (fetched in core/cmake/Dependencies.cmake).

add_executable(simcore_tests
  version_test.cpp
  table_test.cpp
  engine_model_test.cpp
  golden_test.cpp
  concurrency_test.cpp
  protocol_test.cpp
  simulation_service_test.cpp
  app_test.cpp
  websocket_server_test.cpp
  origin_policy_test.cpp
  table_units_test.cpp
  json_repository_test.cpp
  acoustic/dsp_test.cpp
  acoustic/pulse_source_test.cpp
  acoustic/exhaust_network_test.cpp
  acoustic/engine_sound_test.cpp
  acoustic/acoustic_params_test.cpp
  acoustic/afterburn_test.cpp
  engine_acoustics_test.cpp
  sound_acceptance_test.cpp
  json_repository_test.cpp)
target_link_libraries(simcore_tests PRIVATE
  simcore_lib
  simcore_warnings
  ixwebsocket::ixwebsocket
  doctest::doctest_with_main)
target_compile_definitions(simcore_tests PRIVATE
  SIMCORE_EXPECTED_VERSION="${PROJECT_VERSION}"
  SIMCORE_TEST_SHARED_DIR="${PROJECT_SOURCE_DIR}/shared")

doctest_discover_tests(simcore_tests)
