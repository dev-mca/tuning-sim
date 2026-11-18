# sound_spike: engine sound experiment (plan.md iteration 3, docs/adr/0003-sound-approach.md).
# Library with all DSP (testable without audio hardware) + interactive executable + tests.

add_library(sound_spike_lib STATIC
  src/dsp/delay_line.cpp
  src/dsp/fir_filter.cpp
  src/acoustic/layout.cpp
  src/acoustic/pulse_source.cpp
  src/acoustic/exhaust_network.cpp
  src/acoustic/engine_sound.cpp
  src/io/wav_writer.cpp
  src/io/offline_render.cpp
  src/app/sound_bank.cpp)
target_include_directories(sound_spike_lib PUBLIC src)
target_compile_features(sound_spike_lib PUBLIC cxx_std_20)
target_link_libraries(sound_spike_lib PRIVATE simcore_warnings)

add_executable(sound_spike
  src/app/main.cpp
  src/app/audio_device.cpp
  src/app/keyboard.cpp)
target_link_libraries(sound_spike PRIVATE sound_spike_lib miniaudio simcore_warnings)

if(BUILD_TESTING)
  add_executable(sound_spike_tests
    tests/dsp_test.cpp
    tests/pulse_source_test.cpp
    tests/exhaust_network_test.cpp
    tests/engine_sound_test.cpp)
  target_link_libraries(sound_spike_tests PRIVATE
    sound_spike_lib simcore_warnings doctest::doctest_with_main)
  doctest_discover_tests(sound_spike_tests)
endif()
