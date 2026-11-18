#pragma once

// Shared helpers for the simcore tests.

#include "adapters/json_file_repository.hpp"

#include <string>

namespace simcore::test {

/// The reference engine from the repository's shared/ folder (path injected by CMake).
inline const LoadedEngine& referenceEngine() {
    static const LoadedEngine engine = JsonFileRepository(SIMCORE_TEST_SHARED_DIR).load("i6_30_na");
    return engine;
}

inline std::string sharedDir() {
    return SIMCORE_TEST_SHARED_DIR;
}

} // namespace simcore::test
