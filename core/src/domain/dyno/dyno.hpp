#pragma once

// Engine dyno: absorbing brake with a speed controller (docs/research.md §2.12).
// Port of lab dyno.py. The brake can only take torque (eddy current), never drive the engine.
// Measured torque = brake torque + J d(omega)/dt, low-pass filtered (tau = 50 ms).

#include "domain/engine/engine_simulation.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace simcore {

inline constexpr double kControlBandwidth_rad_s = 20.0;
inline constexpr double kMaxBrakeTorque_Nm = 1000.0;
inline constexpr double kMeasurementFilterTimeConstant_s = 0.05;
inline constexpr double kDefaultSettleTime_s = 3.0;
inline constexpr double kDefaultSweepRate_rpm_s = 300.0;
inline constexpr double kDefaultSampleStep_rpm = 100.0;

/// PI controller with anti-windup (research.md §9). Kp = J w_c, Ki = Kp w_c / 4.
class SpeedController {
public:
    explicit SpeedController(double inertia_kgm2,
                             double bandwidth_rad_s = kControlBandwidth_rad_s) noexcept;

    /// Brake torque for the current speed error; positive error (too fast) = more brake.
    double update(double speed_rad_s, double setpoint_rad_s, double dt) noexcept;

private:
    double kp_;
    double ki_;
    double integral_Nm_ = 0.0;
};

/// First-order low-pass y += alpha (x - y), alpha = dt / (tau + dt) (research.md §9).
class LowPassFilter {
public:
    LowPassFilter(double timeConstant_s, double dt) noexcept;
    double update(double sample) noexcept;

private:
    double alpha_;
    std::optional<double> value_;
};

struct SweepParameters {
    double speedFrom_rpm;
    double speedTo_rpm;
    double rate_rpm_s = kDefaultSweepRate_rpm_s;
    double throttle = 1.0;
    double sampleStep_rpm = kDefaultSampleStep_rpm;
    double settleTime_s = kDefaultSettleTime_s;
};

/// One sample of a sweep, taken when the setpoint passes a multiple of the sample step (SI).
struct SweepSample {
    double setpoint_rad_s;
    double speed_rad_s;
    double measuredTorque_Nm; // what the dyno measures
    double engineTorque_Nm;   // brake torque of the model, no measurement effects
    double manifoldPressure_Pa;
    double lambdaActual;
    double ignition_deg;
    double airMassFlow_kg_s;
    double exhaustTemperature_K;

    [[nodiscard]] double power_W() const noexcept { return measuredTorque_Nm * speed_rad_s; }
};

/// A running sweep: settle at the start speed, then ramp the setpoint (research.md §2.12).
/// step() advances the given simulation by one time step, so the same code serves the fast
/// CLI sweep and the real-time sweep requested over the WebSocket.
class SweepRun {
public:
    /// Throws std::invalid_argument unless speedTo > speedFrom and rate > 0.
    SweepRun(const SweepParameters& parameters, double inertia_kgm2, double timeStep_s);

    /// Returns the simulation state of this step (also during settling).
    SimulationState step(EngineSimulation& simulation);

    [[nodiscard]] bool finished() const noexcept { return stepIndex_ > settleSteps_ + rampSteps_; }
    [[nodiscard]] double progress() const noexcept; // 0..1
    [[nodiscard]] double throttle() const noexcept { return parameters_.throttle; }
    [[nodiscard]] double setpoint_rpm() const noexcept { return setpoint_rpm_; }
    [[nodiscard]] const std::vector<SweepSample>& samples() const noexcept { return samples_; }

private:
    SweepParameters parameters_;
    double inertia_kgm2_;
    double dt_;
    SpeedController controller_;
    LowPassFilter torqueFilter_;
    std::vector<double> sampleSetpoints_rpm_;
    long settleSteps_;
    long rampSteps_;
    long stepIndex_ = 0;
    std::size_t nextSample_ = 0;
    double load_Nm_ = 0.0;
    std::optional<double> previousSpeed_rad_s_;
    double setpoint_rpm_;
    std::vector<SweepSample> samples_;
};

/// Complete sweep on a fresh simulation starting at speedFrom (CLI, golden file).
[[nodiscard]] std::vector<SweepSample> runSweep(const EngineDefinition& definition,
                                                const SweepParameters& parameters,
                                                std::optional<Tune> tune = std::nullopt);

} // namespace simcore
