#include "app/keyboard.hpp"

#ifdef _WIN32
#include <conio.h>

#include <chrono>
#include <thread>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace sound_spike {

#ifdef _WIN32

struct Keyboard::State {};

Keyboard::Keyboard() : state_(std::make_unique<State>()) {}
Keyboard::~Keyboard() = default;

std::optional<char> Keyboard::poll(int timeoutMs) {
    constexpr int kStepMs = 5;
    for (int waited = 0; waited <= timeoutMs; waited += kStepMs) {
        if (_kbhit() != 0) {
            return static_cast<char>(_getch());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
    }
    return std::nullopt;
}

#else

struct Keyboard::State {
    termios original{};
    bool restored = false;
};

Keyboard::Keyboard() : state_(std::make_unique<State>()) {
    if (tcgetattr(STDIN_FILENO, &state_->original) != 0) {
        state_->restored = true; // not a terminal: nothing to restore
        return;
    }
    termios raw = state_->original;
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO); // no line buffering, no echo
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

Keyboard::~Keyboard() {
    if (!state_->restored) {
        tcsetattr(STDIN_FILENO, TCSANOW, &state_->original);
    }
}

std::optional<char> Keyboard::poll(int timeoutMs) {
    constexpr int kMicrosecondsPerMillisecond = 1000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(STDIN_FILENO, &readSet);
    timeval timeout{0, static_cast<suseconds_t>(timeoutMs * kMicrosecondsPerMillisecond)};
    if (select(STDIN_FILENO + 1, &readSet, nullptr, nullptr, &timeout) > 0) {
        char key = 0;
        if (read(STDIN_FILENO, &key, 1) == 1) {
            return key;
        }
    }
    return std::nullopt;
}

#endif

} // namespace sound_spike
