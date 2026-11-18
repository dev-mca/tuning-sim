#pragma once

// IClock adapter on std::chrono::steady_clock (monotonic: never jumps with the wall clock).

#include "ports/ports.hpp"

#include <chrono>

namespace simcore {

class SteadyClock final : public IClock {
public:
    [[nodiscard]] double now_s() const override {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }
};

} // namespace simcore
