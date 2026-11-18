#pragma once

// Lock-free "latest value" exchange between one writer and one reader (docs/plan.md §2.2).
//
// Three slots: the writer fills its back slot and swaps it with the shared middle slot; the
// reader swaps its front slot with the middle slot when a new value is there. Nobody ever
// waits, and the reader always gets the newest complete value (intermediate values may be
// skipped, which is fine for 1 kHz simulation state read at 60 Hz).

#include <array>
#include <atomic>
#include <cstdint>

namespace simcore {

template <typename T>
class TripleBuffer {
public:
    /// Writer side: publishes a new value.
    void write(const T& value) {
        slots_[back_] = value;
        const std::uint8_t previous = middle_.exchange(
            static_cast<std::uint8_t>(back_ | kFreshFlag), std::memory_order_acq_rel);
        back_ = previous & kIndexMask;
    }

    /// Reader side: true if a value newer than the last read was available; `out` then holds it.
    bool read(T& out) {
        if ((middle_.load(std::memory_order_acquire) & kFreshFlag) == 0) {
            return false;
        }
        const std::uint8_t previous = middle_.exchange(front_, std::memory_order_acq_rel);
        front_ = previous & kIndexMask;
        out = slots_[front_];
        return true;
    }

private:
    static constexpr std::uint8_t kFreshFlag = 0x4; // "middle holds an unread value"
    static constexpr std::uint8_t kIndexMask = 0x3;

    std::array<T, 3> slots_{};
    std::uint8_t back_ = 0;               // writer only
    std::uint8_t front_ = 1;              // reader only
    std::atomic<std::uint8_t> middle_{2}; // shared: slot index plus fresh flag
};

} // namespace simcore
