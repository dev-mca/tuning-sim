#include "domain/drivetrain/drivetrain.hpp"

#include "domain/common/constants.hpp"
#include "domain/engine/engine_model.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace simcore {

double roadLoad_N(const DrivetrainParameters& parameters, double speed_m_s) noexcept {
    if (speed_m_s <= 0.0) {
        return 0.0;
    }
    const double airDensity =
        airDensity_kg_m3(constants::kAmbientPressure_Pa, constants::kAmbientTemperature_K);
    const double aero = 0.5 * airDensity * parameters.dragArea_m2 * speed_m_s * speed_m_s;
    const double rolling =
        parameters.rollingResistance * parameters.vehicleMass_kg * constants::kStandardGravity_m_s2;
    return aero + rolling;
}

Drivetrain::Drivetrain(DrivetrainParameters parameters, double engineInertia_kgm2)
    : parameters_(std::move(parameters)), engineInertia_kgm2_(engineInertia_kgm2),
      vehicleMass_kg_(parameters_.vehicleMass_kg +
                      parameters_.wheelInertia_kgm2 /
                          (parameters_.wheelRadius_m * parameters_.wheelRadius_m)) {}

bool Drivetrain::shift(int gear) noexcept {
    if (gear < kNeutral || gear > parameters_.gearCount() || gear == gear_) {
        return false;
    }
    if (shiftPhase_ != ShiftPhase::None) {
        return false;
    }
    targetGear_ = gear;
    shiftPhase_ = ShiftPhase::Interrupt;
    shiftElapsed_s_ = 0.0;
    return true;
}

void Drivetrain::setNeutral() noexcept {
    gear_ = kNeutral;
    targetGear_ = kNeutral;
    shiftPhase_ = ShiftPhase::None;
    clutchState_ = ClutchState::Open;
    clutchTorque_Nm_ = 0.0;
}

void Drivetrain::placeInGear(int gear, double engineSpeed_rad_s) {
    gear_ = gear;
    targetGear_ = gear;
    shiftPhase_ = ShiftPhase::None;
    pedalPressed_ = false;
    engagement_ = 1.0;
    clutchState_ = ClutchState::Locked;
    speed_m_s_ = engineSpeed_rad_s * parameters_.wheelRadius_m / ratio();
}

void Drivetrain::updateControls(double dt) noexcept {
    if (shiftPhase_ == ShiftPhase::Interrupt) {
        shiftElapsed_s_ += dt;
        if (shiftElapsed_s_ >= parameters_.shiftTime_s) {
            gear_ = targetGear_;
            shiftPhase_ = ShiftPhase::Engage;
            shiftElapsed_s_ = 0.0;
        }
    }
    if (pedalPressed_ || shiftPhase_ == ShiftPhase::Interrupt) {
        engagement_ = 0.0; // pressing the pedal opens the clutch at once
        return;
    }
    const double closingTime = shiftPhase_ == ShiftPhase::Engage ? parameters_.shiftEngageTime_s
                                                                 : parameters_.clutchReleaseTime_s;
    engagement_ = std::min(1.0, engagement_ + dt / closingTime);
    if (shiftPhase_ == ShiftPhase::Engage && engagement_ >= 1.0) {
        shiftPhase_ = ShiftPhase::None;
    }
}

double Drivetrain::clutchTorque(double engineTorque_Nm, double engineSpeed_rad_s) noexcept {
    if (decoupled()) {
        clutchState_ = ClutchState::Open;
        clutchTorque_Nm_ = 0.0;
        return 0.0;
    }
    const double capacity = engagement_ * parameters_.clutchMaxTorque_Nm;
    if (clutchState_ == ClutchState::Locked) {
        const double locked = lockedClutchTorque(engineTorque_Nm);
        if (std::abs(locked) <= capacity) {
            clutchTorque_Nm_ = locked;
            return locked;
        }
        clutchState_ = ClutchState::Slipping; // too much torque: the clutch breaks loose
    }
    double slip = engineSpeed_rad_s - inputSpeed_rad_s();
    if (clutchState_ == ClutchState::Open) {
        clutchState_ = ClutchState::Slipping;
    }
    if (slip == 0.0) {
        // Same speed on both sides (e.g. both standing): lock if the clutch can hold it.
        const double locked = lockedClutchTorque(engineTorque_Nm);
        if (std::abs(locked) <= capacity) {
            clutchState_ = ClutchState::Locked;
            clutchTorque_Nm_ = locked;
            return locked;
        }
        slip = locked; // the sign of the needed torque decides the direction
    }
    clutchTorque_Nm_ = slip > 0.0 ? capacity : -capacity;
    return clutchTorque_Nm_;
}

std::optional<double> Drivetrain::integrate(double dt, double engineSpeed_rad_s) noexcept {
    if (clutchState_ == ClutchState::Locked) {
        // Kinematic coupling: the car follows the crank exactly (no drift).
        speed_m_s_ = engineSpeed_rad_s * parameters_.wheelRadius_m / ratio();
        return std::nullopt;
    }
    const double driveForce = wheelForce(clutchTorque_Nm_);
    const double acceleration =
        (driveForce - roadLoad_N(parameters_, speed_m_s_)) / vehicleMass_kg_;
    speed_m_s_ = std::max(0.0, speed_m_s_ + dt * acceleration);
    if (clutchState_ != ClutchState::Slipping) {
        return std::nullopt;
    }
    // The clutch torque points in the direction of the slip at the start of the step.
    const double slip = engineSpeed_rad_s - inputSpeed_rad_s();
    if (std::abs(slip) < kLockSlip_rad_s || (slip > 0.0) != (clutchTorque_Nm_ > 0.0)) {
        return lock(engineSpeed_rad_s);
    }
    return std::nullopt;
}

DrivetrainState Drivetrain::state() const noexcept {
    const double force = wheelForce(clutchTorque_Nm_);
    return {gear_,
            engagement_,
            clutchState_,
            shifting(),
            speed_m_s_,
            clutchTorque_Nm_,
            force * parameters_.wheelRadius_m,
            force};
}

double Drivetrain::inputSpeed_rad_s() const {
    // omega_in = v / r * i (§8.1)
    return speed_m_s_ / parameters_.wheelRadius_m * ratio();
}

double Drivetrain::wheelForce(double clutchTorque_Nm) const {
    if (decoupled()) {
        return 0.0;
    }
    return clutchTorque_Nm * ratio() * parameters_.efficiency / parameters_.wheelRadius_m;
}

double Drivetrain::lockedClutchTorque(double engineTorque_Nm) const {
    // m_v dv/dt = T_c i eta / r - F_res,  T_c = T_e - J_e (i/r) dv/dt
    // -> dv/dt = (T_e i eta / r - F_res) / (m_v + J_e i^2 eta / r^2)   (§8.2 with m_eff)
    const double i = ratio();
    const double radius = parameters_.wheelRadius_m;
    const double efficiency = parameters_.efficiency;
    const double effectiveMass =
        vehicleMass_kg_ + engineInertia_kgm2_ * i * i * efficiency / (radius * radius);
    const double acceleration =
        (engineTorque_Nm * i * efficiency / radius - roadLoad_N(parameters_, speed_m_s_)) /
        effectiveMass;
    return engineTorque_Nm - engineInertia_kgm2_ * i / radius * acceleration;
}

double Drivetrain::lock(double engineSpeed_rad_s) {
    // Momentum-conserving common speed; car inertia at the gearbox input: m_v r^2 / i^2.
    const double i = ratio();
    const double radius = parameters_.wheelRadius_m;
    const double inputInertia = vehicleMass_kg_ * radius * radius / (i * i);
    const double commonSpeed =
        (engineInertia_kgm2_ * engineSpeed_rad_s + inputInertia * inputSpeed_rad_s()) /
        (engineInertia_kgm2_ + inputInertia);
    clutchState_ = ClutchState::Locked;
    speed_m_s_ = commonSpeed * radius / i;
    return commonSpeed;
}

} // namespace simcore
