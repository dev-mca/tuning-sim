#pragma once

#include <memory>
#include <optional>

namespace sound_spike {

/// Reads single key presses without Enter. Puts the terminal into raw mode while alive
/// and restores it in the destructor (RAII, like try-with-resources in Java).
class Keyboard {
public:
    Keyboard();
    ~Keyboard();
    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;

    /// Waits at most `timeoutMs` for a key; returns nothing on timeout.
    [[nodiscard]] std::optional<char> poll(int timeoutMs);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace sound_spike
