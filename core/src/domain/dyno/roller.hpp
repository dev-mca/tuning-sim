#pragma once

// Roller dyno run ("Durchzug"): full throttle in one gear, wheel power over engine speed; port
// of lab roller.py. The car starts locked in gear a little below the first sample speed and
// accelerates freely on the road-load rollers (research.md §8.4). The roller measures the drive
// force at the tyres; wheel power = F v. Divided by the overall ratio the wheel torque becomes
// an engine-equivalent torque, lower than the engine's own by the drivetrain losses and the
// torque that accelerates the engine's inertia.

#include "domain/drivetrain/car_simulation.hpp"
#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_definition.hpp"

#include <optional>
#include <vector>

namespace simcore {

/// The run starts this far below the first sample so that the manifold has settled.
inline constexpr double kRollerRunUp_rpm = 300.0;
inline constexpr double kRollerSampleStep_rpm = 100.0;
inline constexpr double kRollerMaxRunTime_s = 60.0;

struct RollerSample {
    double speed_rad_s; // engine speed
    double vehicleSpeed_m_s;
    double wheelForce_N;
    double wheelTorque_Nm;
    double engineTorque_Nm; // brake torque of the engine model at the same instant
    double time_s;
    // Engine values at the same instant, for the dyno result in the UI:
    double manifoldPressure_Pa;
    double lambdaActual;
    double ignition_deg;
    double exhaustTemperature_K;
    double airMassFlow_kg_s;

    [[nodiscard]] double wheelPower_W() const noexcept { return wheelForce_N * vehicleSpeed_m_s; }
};

struct RollerParameters {
    int gear = 4;
    double speedFrom_rpm = 1500.0;
    double speedTo_rpm = 6800.0;
    double sampleStep_rpm = kRollerSampleStep_rpm;
};

/// A roller run on a car, one time step at a time (the service runs it in real time).
class RollerRun {
public:
    /// Puts the car in gear at the start speed. Throws std::invalid_argument for an invalid
    /// gear or speed range.
    RollerRun(const RollerParameters& parameters, CarSimulation& car);

    /// Full throttle for one step; samples when the engine passes the next sample speed.
    CarState step(CarSimulation& car);

    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] double progress() const noexcept; // 0..1
    [[nodiscard]] double currentRpm() const noexcept { return rpm_; }
    [[nodiscard]] int gear() const noexcept { return parameters_.gear; }
    [[nodiscard]] const std::vector<RollerSample>& samples() const noexcept { return samples_; }

private:
    RollerParameters parameters_;
    std::vector<double> sampleRpm_;
    std::vector<RollerSample> samples_;
    long steps_ = 0;
    long maxSteps_;
    double rpm_;
};

/// Throws std::invalid_argument for an invalid gear or speed range.
[[nodiscard]] std::vector<RollerSample> runRoller(const EngineDefinition& definition,
                                                  const RollerParameters& parameters,
                                                  std::optional<Tune> tune = std::nullopt);

} // namespace simcore
