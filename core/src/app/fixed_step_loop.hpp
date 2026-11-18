#pragma once

// Fixed time step with accumulator ("Fix Your Timestep", Glenn Fiedler; docs/plan.md §2.3).
// Wall-clock time is collected in an accumulator; the simulation advances in exact steps of
// dt. Pure logic (no clock, no sleeping), so it is unit-tested with arbitrary elapsed times.

#include <algorithm>
#include <cmath>

namespace simcore {

class FixedStepAccumulator {
public:
    /// maxBacklog_s caps the debt after a stall (debugger, sleep) instead of catching up for
    /// minutes ("spiral of death").
    FixedStepAccumulator(double timeStep_s, double maxBacklog_s) noexcept
        : timeStep_s_(timeStep_s), maxBacklog_s_(maxBacklog_s) {}

    /// Adds elapsed wall-clock time and returns how many steps are due now.
    int addElapsed(double elapsed_s) noexcept {
        accumulator_s_ = std::min(accumulator_s_ + std::max(elapsed_s, 0.0), maxBacklog_s_);
        // Divide instead of subtracting dt in a loop: repeated subtraction accumulates rounding
        // errors (0.1 s / 1 ms would give 99 steps). The tolerance absorbs the last bit.
        const auto steps =
            static_cast<int>(std::floor(accumulator_s_ / timeStep_s_ + kRoundingTolerance));
        accumulator_s_ = std::max(0.0, accumulator_s_ - steps * timeStep_s_);
        return steps;
    }

private:
    static constexpr double kRoundingTolerance = 1.0e-9; // in steps

    double timeStep_s_;
    double maxBacklog_s_;
    double accumulator_s_ = 0.0;
};

} // namespace simcore
