#pragma once

// Runs SimulationService::step() in real time on its own thread (simulation thread,
// docs/plan.md §2.2), driven by an IClock and the fixed-step accumulator.

#include "app/fixed_step_loop.hpp"
#include "app/simulation_service.hpp"
#include "ports/ports.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace simcore {

class RealtimeRunner {
public:
    static constexpr double kMaxBacklog_s = 0.1;
    static constexpr auto kIdleSleep = std::chrono::milliseconds(1);

    RealtimeRunner(SimulationService& service, const IClock& clock)
        : service_(service), clock_(clock) {}
    ~RealtimeRunner() { stop(); }
    RealtimeRunner(const RealtimeRunner&) = delete;
    RealtimeRunner& operator=(const RealtimeRunner&) = delete;

    void start() {
        running_ = true;
        thread_ = std::thread([this] { run(); });
    }
    void stop() {
        if (running_.exchange(false) && thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void run() {
        FixedStepAccumulator accumulator(service_.timeStep_s(), kMaxBacklog_s);
        double last_s = clock_.now_s();
        while (running_) {
            const double now_s = clock_.now_s();
            const int steps = accumulator.addElapsed(now_s - last_s);
            last_s = now_s;
            for (int i = 0; i < steps; ++i) {
                service_.step();
            }
            std::this_thread::sleep_for(kIdleSleep);
        }
    }

    SimulationService& service_;
    const IClock& clock_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace simcore
