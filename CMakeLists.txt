cmake_minimum_required(VERSION 3.25)

project(tuning-sim
  VERSION 0.1.0
  DESCRIPTION "Engine tuning simulator"
  LANGUAGES C CXX) # C only for miniaudio

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tests are on by default; switch off with -DBUILD_TESTING=OFF.
# (Not include(CTest): that adds many CDash dashboard targets we don't need.)
option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
  enable_testing()
endif()

add_subdirectory(core)
