#pragma once

// Lock-free single-producer/single-consumer ring buffer (docs/plan.md §2.2, learning goal).
//
// Exactly one thread calls push(), exactly one other thread calls pop(). No mutex: each side
// owns one index and only reads the other one. memory_order_release on the writer's index
// publishes the slot contents; memory_order_acquire on the reader's side makes them visible
// (similar to a volatile write/read pair in Java's memory model).

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <utility>

namespace simcore {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "one slot always stays empty to tell full from empty");

public:
    /// Producer side. Returns false (and drops nothing) if the queue is full.
    bool push(T value) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // full
        }
        slots_[head] = std::move(value);
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Consumer side. Returns nothing if the queue is empty.
    std::optional<T> pop() {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt; // empty
        }
        std::optional<T> value(std::move(slots_[tail]));
        tail_.store(increment(tail), std::memory_order_release);
        return value;
    }

private:
    static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> slots_{};
    std::atomic<std::size_t> head_{0}; // next slot to write, owned by the producer
    std::atomic<std::size_t> tail_{0}; // next slot to read, owned by the consumer
};

} // namespace simcore
