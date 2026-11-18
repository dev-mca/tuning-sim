#include "adapters/message_bus.hpp"

#include <utility>

namespace simcore {

bool MessageBus::submit(IncomingCommand command) {
    const std::lock_guard<std::mutex> lock(producerMutex_);
    return commands_.push(std::move(command));
}

std::optional<OutgoingMessage> MessageBus::nextOutgoing() {
    return outgoing_.pop();
}

bool MessageBus::latestState(StateSnapshot& out) {
    return state_.read(out);
}

std::optional<IncomingCommand> MessageBus::poll() {
    return commands_.pop();
}

void MessageBus::publishState(const StateSnapshot& state) {
    state_.write(state);
}

void MessageBus::send(OutgoingMessage message) {
    // The simulation must never block on a slow client: if the queue is full, drop and count.
    if (!outgoing_.push(std::move(message))) {
        droppedOutgoing_.fetch_add(1);
    }
}

} // namespace simcore
