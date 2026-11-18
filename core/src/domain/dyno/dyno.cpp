#include "domain/dyno/dyno.hpp"

#include "domain/common/units.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace simcore {

namespace {

constexpr double kSampleTolerance_rpm = 1.0e-9;
constexpr double kHalf = 0.5;

} // namespace

SpeedController::SpeedController(double inertia_kgm2, double bandwidth_rad_s) noexcept
    : kp_(inertia_kgm2 * bandwidth_rad_s), ki_(kp_ * bandwidth_rad_s / 4.0) {}

double SpeedController::update(double speed_rad_s, double setpoint_rad_s, double dt) noexcept {
    const double error = speed_rad_s - setpoint_rad_s;
    const double unclamped = kp_ * error + integral_Nm_ + ki_ * error * dt;
    const double output = std::min(std::max(unclamped, 0.0), kMaxBrakeTorque_Nm);
    // Anti-windup: only integrate while the output is not saturated.
    if (output == unclamped) {
        integral_Nm_ += ki_ * error * dt;
    }
    return output;
}

LowPassFilter::LowPassFilter(double timeConstant_s, double dt) noexcept
    : alpha_(dt / (timeConstant_s + dt)) {}

double LowPassFilter::update(double sample) noexcept {
    if (!value_) {
        value_ = sample;
    } else {
        *value_ += alpha_ * (sample - *value_);
    }
    return *value_;
}

SweepRun::SweepRun(const SweepParameters& parameters, double inertia_kgm2, double timeStep_s)
    : parameters_(parameters), inertia_kgm2_(inertia_kgm2), dt_(timeStep_s),
      controller_(inertia_kgm2), torqueFilter_(kMeasurementFilterTimeConstant_s, timeStep_s),
      settleSteps_(std::lround(parameters.settleTime_s / timeStep_s)),
      rampSteps_(std::lround((parameters.speedTo_rpm - parameters.speedFrom_rpm) /
                             parameters.rate_rpm_s / timeStep_s)),
      setpoint_rpm_(parameters.speedFrom_rpm) {
    if (!(parameters.speedTo_rpm > parameters.speedFrom_rpm) || !(parameters.rate_rpm_s > 0.0)) {
        throw std::invalid_argument("sweep needs speedTo > speedFrom and rate > 0");
    }
    // Like numpy.arange(from, to + step/2, step): from + i * step.
    for (long i = 0;; ++i) {
        const double setpoint =
            parameters.speedFrom_rpm + static_cast<double>(i) * parameters.sampleStep_rpm;
        if (!(setpoint < parameters.speedTo_rpm + kHalf * parameters.sampleStep_rpm)) {
            break;
        }
        sampleSetpoints_rpm_.push_back(setpoint);
    }
    samples_.reserve(sampleSetpoints_rpm_.size());
}

double SweepRun::progress() const noexcept {
    const auto total = static_cast<double>(settleSteps_ + rampSteps_ + 1);
    return std::min(1.0, static_cast<double>(stepIndex_) / total);
}

SimulationState SweepRun::step(EngineSimulation& simulation) {
    if (finished()) {
        throw std::logic_error("SweepRun::step called after the sweep finished");
    }
    if (!previousSpeed_rad_s_) {
        previousSpeed_rad_s_ = simulation.speed_rad_s();
    }
    const double rampTime_s = static_cast<double>(std::max(0L, stepIndex_ - settleSteps_)) * dt_;
    setpoint_rpm_ = std::min(parameters_.speedFrom_rpm + parameters_.rate_rpm_s * rampTime_s,
                             parameters_.speedTo_rpm);
    const SimulationState state = simulation.step(parameters_.throttle, load_Nm_);
    const double acceleration = (state.speed_rad_s - *previousSpeed_rad_s_) / dt_;
    previousSpeed_rad_s_ = state.speed_rad_s;
    const double measured = torqueFilter_.update(load_Nm_ + inertia_kgm2_ * acceleration);
    load_Nm_ = controller_.update(state.speed_rad_s, units::rpmToRadS(setpoint_rpm_), dt_);

    const bool isRamping = stepIndex_ >= settleSteps_;
    if (isRamping && nextSample_ < sampleSetpoints_rpm_.size() &&
        setpoint_rpm_ >= sampleSetpoints_rpm_[nextSample_] - kSampleTolerance_rpm) {
        const EngineOutputs& engine = state.engine;
        samples_.push_back({units::rpmToRadS(sampleSetpoints_rpm_[nextSample_]), state.speed_rad_s,
                            measured, engine.brakeTorque_Nm, state.manifoldPressure_Pa,
                            engine.lambdaActual, state.command.ignition_deg,
                            engine.airMassFlow_kg_s, engine.exhaustTemperature_K});
        ++nextSample_;
    }
    ++stepIndex_;
    return state;
}

std::vector<SweepSample> runSweep(const EngineDefinition& definition,
                                  const SweepParameters& parameters, std::optional<Tune> tune) {
    EngineSimulation simulation(definition, std::move(tune), nullptr,
                                units::rpmToRadS(parameters.speedFrom_rpm));
    SweepRun run(parameters, definition.inertia_kgm2, simulation.timeStep_s());
    while (!run.finished()) {
        run.step(simulation);
    }
    return run.samples();
}

} // namespace simcore
