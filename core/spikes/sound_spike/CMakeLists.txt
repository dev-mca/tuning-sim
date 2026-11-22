# sound_spike: engine sound experiment (plan.md iteration 3, docs/adr/0003-sound-approach.md).
# Since iteration 7 the DSP and acoustic code live in simcore (simcore_acoustic); the spike
# keeps only its interactive front end (keyboard, layout switching, recording).

add_library(sound_spike_lib STATIC
  src/io/comparison_export.cpp
  src/app/sound_bank.cpp)
target_include_directories(sound_spike_lib PUBLIC src)
target_compile_features(sound_spike_lib PUBLIC cxx_std_20)
target_link_libraries(sound_spike_lib PUBLIC simcore_lib PRIVATE simcore_warnings)

add_executable(sound_spike
  src/app/main.cpp
  src/app/audio_device.cpp
  src/app/keyboard.cpp)
target_link_libraries(sound_spike PRIVATE sound_spike_lib miniaudio simcore_warnings)

if(BUILD_TESTING)
  add_executable(sound_spike_tests
    tests/sound_bank_test.cpp)
  target_link_libraries(sound_spike_tests PRIVATE
    sound_spike_lib simcore_warnings doctest::doctest_with_main)
  doctest_discover_tests(sound_spike_tests)
endif()
