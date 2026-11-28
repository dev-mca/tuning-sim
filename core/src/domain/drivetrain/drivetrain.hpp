#pragma once

// Clutch, gearbox and the car on a road-load roller dyno (research.md §8, iteration 10); port
// of lab drivetrain.py, formula by formula in the same floating-point order.
//
// The clutch is a state machine (§8.3): open (no torque), slipping (capacity
// T_cap = engagement * T_max in the direction of the slip, engine and car integrated
// separately) or locked (one inertia, the clutch passes what that needs within T_cap).
// A step is split around the engine: clutchTorque() before the crank integration,
// integrate() after it.

#include "domain/engine/engine_definition.hpp"

#include <optional>

namespace simcore {

inline constexpr int kNeutral = 0;
/// Slip below which a slipping clutch may lock (also when the slip changes sign in a step).
inline constexpr double kLockSlip_rad_s = 1.0;

enum class ClutchState { Open, Slipping, Locked };

struct DrivetrainState {
    int gear = kNeutral;
    double clutchEngagement = 0.0; // 0 = open .. 1 = closed
    ClutchState clutchState = ClutchState::Open;
    bool shifting = false; // torque interruption of a shift
    double vehicleSpeed_m_s = 0.0;
    double clutchTorque_Nm = 0.0; // from the engine; negative = engine braking
    double wheelTorque_Nm = 0.0;  // both driven wheels: T_clutch i eta (§8.1)
    double wheelForce_N = 0.0;    // drive force at the tyres = what the roller dyno measures
};

/// F_air + F_roll at speed v >= 0 (research.md §8.2); the rollers simulate both.
[[nodiscard]] double roadLoad_N(const DrivetrainParameters& parameters, double speed_m_s) noexcept;

class Drivetrain {
public:
    Drivetrain(DrivetrainParameters parameters, double engineInertia_kgm2);

    // --- driver inputs ---
    void setClutchPedal(bool pressed) noexcept { pedalPressed_ = pressed; }
    /// Starts a shift to `gear` (0 = neutral); false if invalid or already shifting.
    bool shift(int gear) noexcept;
    /// Takes the gear out at once (engine dyno sweep: the brake sits on the crank).
    void setNeutral() noexcept;
    /// Puts the car in `gear` with a closed clutch at the speed that matches the engine.
    void placeInGear(int gear, double engineSpeed_rad_s);

    // --- queries ---
    [[nodiscard]] int gear() const noexcept { return gear_; }
    [[nodiscard]] bool shifting() const noexcept { return shiftPhase_ == ShiftPhase::Interrupt; }
    [[nodiscard]] bool clutchPedalPressed() const noexcept { return pedalPressed_; }
    [[nodiscard]] double vehicleSpeed_m_s() const noexcept { return speed_m_s_; }
    /// True when no torque can pass: neutral or clutch fully open.
    [[nodiscard]] bool decoupled() const noexcept {
        return gear_ == kNeutral || engagement_ <= 0.0;
    }
    /// Clutch pedal pressed with a standing car: arms the 2-step (research.md §3.4).
    [[nodiscard]] bool launchArmed() const noexcept {
        return pedalPressed_ && speed_m_s_ < parameters_.launchMaxSpeed_m_s;
    }
    [[nodiscard]] const DrivetrainParameters& parameters() const noexcept { return parameters_; }

    // --- one time step ---
    /// Shift phases and clutch engagement; call once per step before clutchTorque().
    void updateControls(double dt) noexcept;
    /// Load torque on the crank for this step (research.md §8.3).
    double clutchTorque(double engineTorque_Nm, double engineSpeed_rad_s) noexcept;
    /// Moves the car on; returns a new engine speed when the clutch has just locked.
    std::optional<double> integrate(double dt, double engineSpeed_rad_s) noexcept;

    [[nodiscard]] DrivetrainState state() const noexcept;

private:
    enum class ShiftPhase { None, Interrupt, Engage };

    [[nodiscard]] double ratio() const { return parameters_.overallRatio(gear_); }
    [[nodiscard]] double inputSpeed_rad_s() const;
    [[nodiscard]] double wheelForce(double clutchTorque_Nm) const;
    [[nodiscard]] double lockedClutchTorque(double engineTorque_Nm) const;
    double lock(double engineSpeed_rad_s);

    DrivetrainParameters parameters_;
    double engineInertia_kgm2_;
    double vehicleMass_kg_; // car plus wheels as an equivalent mass at the tyres (§8.2)
    int gear_ = kNeutral;
    int targetGear_ = kNeutral;
    bool pedalPressed_ = false;
    double engagement_ = 1.0;
    ClutchState clutchState_ = ClutchState::Open;
    ShiftPhase shiftPhase_ = ShiftPhase::None;
    double shiftElapsed_s_ = 0.0;
    double speed_m_s_ = 0.0;
    double clutchTorque_Nm_ = 0.0;
};

} // namespace simcore
