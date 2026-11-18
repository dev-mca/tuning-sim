# simcore: headless simulation process (see docs/adr/0001-architecture.md)

add_executable(simcore
  src/app/main.cpp)

target_compile_definitions(simcore PRIVATE
  SIMCORE_VERSION="${PROJECT_VERSION}")

# High warning level on every compiler; warnings are bugs waiting to happen.
if(MSVC)
  target_compile_options(simcore PRIVATE /W4 /permissive-)
else()
  target_compile_options(simcore PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()
