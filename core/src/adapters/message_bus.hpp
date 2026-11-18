#pragma once

// Thread bridge between network threads and the simulation thread (docs/plan.md §2.2).
//
// network -> simulation: commands in a lock-free SPSC queue. WebSocket callbacks may run on
//   several threads (one per client), so producers serialise among themselves with a mutex;
//   the simulation thread (consumer) never takes a lock.
// simulation -> network: replies/events in a second SPSC queue, the latest state in a
//   triple buffer (60 Hz reader, 1 kHz writer, nobody waits).

#include "concurrency/spsc_queue.hpp"
#include "concurrency/triple_buffer.hpp"
#include "ports/ports.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>

namespace simcore {

class MessageBus final : public ICommandSource, public IStatePublisher {
public:
    static constexpr std::size_t kQueueCapacity = 1024;

    // --- network side ---
    /// Returns false if the queue is full (command dropped).
    bool submit(IncomingCommand command);
    [[nodiscard]] std::optional<OutgoingMessage> nextOutgoing();
    /// True and fills `out` if a state newer than the last call is available.
    bool latestState(StateSnapshot& out);

    // --- simulation side (ports) ---
    [[nodiscard]] std::optional<IncomingCommand> poll() override;
    void publishState(const StateSnapshot& state) override;
    void send(OutgoingMessage message) override;

    [[nodiscard]] std::size_t droppedOutgoing() const noexcept { return droppedOutgoing_.load(); }

private:
    std::mutex producerMutex_;
    SpscQueue<IncomingCommand, kQueueCapacity> commands_;
    SpscQueue<OutgoingMessage, kQueueCapacity> outgoing_;
    TripleBuffer<StateSnapshot> state_;
    std::atomic<std::size_t> droppedOutgoing_{0};
};

} // namespace simcore
