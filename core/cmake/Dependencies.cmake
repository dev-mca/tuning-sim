# Third-party dependencies, fetched at configure time with pinned versions and checksums.
# Versions and licences are recorded in docs/setup.md and THIRD_PARTY_NOTICES.md.

include(FetchContent)

# doctest: unit test framework (MIT), see docs/adr/0002-cpp-test-framework.md.
if(BUILD_TESTING)
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
    URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
    SYSTEM # treat doctest headers as system headers: their warnings are not ours
  )
  set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(doctest)
  # Provides doctest_discover_tests(): every TEST_CASE becomes its own CTest test.
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
endif()

# nlohmann/json: JSON for engine/table files and the WebSocket protocol (MIT).
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

# IXWebSocket: WebSocket server (BSD-3-Clause), see docs/adr/0004-websocket-library.md.
# Only localhost is used: no TLS, no compression (no OpenSSL/zlib needed).
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ixwebsocket
  URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v12.1.0.tar.gz
  URL_HASH SHA256=55f3a5d3a1a2c5f19d737c24e6aced28d1200f8f5deb45ecd8e5d46f765b5288
  SYSTEM
)
FetchContent_MakeAvailable(ixwebsocket)

if(SIMCORE_BUILD_SPIKES)
  # miniaudio: audio playback (Public Domain / MIT-0). Single source file; we build it as our
  # own small library instead of using its CMake project (fewer options, no optional codecs).
  # SOURCE_SUBDIR points to a folder without CMakeLists.txt, so FetchContent only downloads.
  FetchContent_Declare(miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz
    URL_HASH SHA256=838ba55a21c9142adf7b76bdc4351e5cd8b7ae128f46b25ec25f94ae9969c600
    SOURCE_SUBDIR download-only
  )
  FetchContent_MakeAvailable(miniaudio)

  find_package(Threads REQUIRED)
  add_library(miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
  target_include_directories(miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
  # Decoders/encoders and the node graph are not needed: smaller and faster to build.
  target_compile_definitions(miniaudio PUBLIC
    MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH
    MA_NO_ENGINE)
  target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
  if(UNIX AND NOT APPLE)
    target_link_libraries(miniaudio PUBLIC m)
  endif()
endif()
