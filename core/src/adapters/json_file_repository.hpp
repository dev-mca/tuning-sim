#pragma once

// Loads shared/engines/<id>.json and the referenced table files (docs/plan.md §5),
// converting everything to SI. Implements the IEngineRepository port.

#include "ports/ports.hpp"

#include <filesystem>

namespace simcore {

class JsonFileRepository final : public IEngineRepository {
public:
    /// sharedDir = the repository's shared/ folder (engines/ and tables/ inside).
    explicit JsonFileRepository(std::filesystem::path sharedDir);

    [[nodiscard]] LoadedEngine load(const std::string& engineId) const override;

private:
    std::filesystem::path sharedDir_;
};

} // namespace simcore
