#pragma once

// Minimal speed-density ECU (docs/research.md §2.2, §2.4, §3); port of lab ecu.py.
// Rev limiter (fuel cut with hysteresis) and overrun fuel cut (research.md §3.3). With the cut
// switched off the ECU keeps fuelling in overrun with optional retard and extra fuel: the
// "pops & bangs" map (research.md §3.3, §2.10a).
// Launch control (2-step, research.md §3.4): with the launch switch held and the throttle open
// the ECU retards the ignition and holds a lower speed limit by cutting the spark while it keeps
// injecting, so the unburnt mixture can bang in the exhaust.
// Concepts as in Speeduino/rusEFI, no code taken from them (GPL).

#include "domain/common/units.hpp"
#include "domain/ecu/table.hpp"
#include "domain/engine/engine_definition.hpp"

#include <string>

namespace simcore {

/// What the ECU does when the driver lifts off at speed (research.md §3.3).
struct OverrunSettings {
    bool fuelCut = true;     // false: keep fuelling in overrun (needed for pops)
    double retard_deg = 0.0; // ignition taken back from the map while fuelling in overrun
    double enrichment = 0.0; // extra fuel while fuelling in overrun, fraction (0.1 = +10 %)
};

/// Allowed ranges of the overrun settings; outside them the ECU rejects the change.
inline constexpr double kMaxOverrunRetard_deg = 60.0;
inline constexpr double kMaxOverrunEnrichment = 0.5;

/// Empty if the settings are valid, otherwise the reason.
[[nodiscard]] std::string validateOverrunSettings(const OverrunSettings& settings);

/// Typical 2-step launch speed of a naturally aspirated engine (research.md §3.4).
inline constexpr double kDefaultLaunch_rpm = 4000.0;

/// Launch control / 2-step while standing (research.md §3.4).
struct LaunchSettings {
    bool enabled = false; // stock ECUs have no launch control
    /// Speed the ECU holds by cutting the spark while the launch switch is held.
    double launchSpeed_rad_s = units::rpmToRadS(kDefaultLaunch_rpm);
    double retard_deg = 0.0; // ignition taken back from the map while launch control is active
    bool flatShift = false;  // cut the spark during an upshift at open throttle (§3.4)
};

/// Allowed ranges of the launch settings; outside them the ECU rejects the change.
inline constexpr double kMinLaunch_rpm = 2000.0;
inline constexpr double kMaxLaunch_rpm = 6500.0;
inline constexpr double kMaxLaunchRetard_deg = 30.0;

/// Empty if the settings are valid, otherwise the reason.
[[nodiscard]] std::string validateLaunchSettings(const LaunchSettings& settings);

/// The tables and settings the user tunes. Axes and units as in EngineTables.
struct Tune {
    Table2D ve;
    Table2D lambdaTarget;
    Table2D ignition;
    OverrunSettings overrun;
    LaunchSettings launch;

    [[nodiscard]] static Tune fromTables(const EngineTables& tables) {
        return {tables.ve, tables.lambdaTarget, tables.ignition, {}, {}};
    }
};

struct EcuCommand {
    double fuelMassPerCycle_kg;
    double ignition_deg;
    double lambdaTarget;
    bool fuelCut;
    bool overrunFueling = false;  // fuelling in overrun with the cut switched off: pops possible
    bool launchActive = false;    // launch control works (enabled, switch held, throttle open)
    bool sparkCut = false;        // launch control or flat shift cuts the spark, keeps injecting
    bool flatShiftActive = false; // flat shift cuts the spark during a shift
};

class Ecu {
public:
    Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s,
        OverrunCutParameters overrunCut);

    /// `throttle` (0..1) decides whether the driver lifted off (overrun fuel cut).
    [[nodiscard]] EcuCommand update(double speed_rad_s, double manifoldPressure_Pa,
                                    double intakeTemperature_K, double throttle) noexcept;

    /// The driver's launch button (later: clutch pedal while standing, iteration 10).
    void setLaunchSwitch(bool held) noexcept { launchSwitch_ = held; }
    /// The gearbox interrupts the torque for a shift (iteration 10).
    void setShiftCutRequest(bool requested) noexcept { shiftCutRequest_ = requested; }
    [[nodiscard]] bool launchSwitch() const noexcept { return launchSwitch_; }

    [[nodiscard]] const Tune& tune() const noexcept { return tune_; }
    [[nodiscard]] Tune& tune() noexcept { return tune_; } // live table editing

private:
    void updateRevLimiter(double speed_rad_s) noexcept;
    void updateOverrun(double speed_rad_s, double throttle) noexcept;
    [[nodiscard]] bool updateLaunch(double speed_rad_s, double throttle) noexcept;

    Tune tune_;
    double displacement_m3_;
    double revLimit_rad_s_;
    double revResume_rad_s_;
    double revLimitHysteresis_rad_s_;
    OverrunCutParameters overrun_;
    bool limiterCut_ = false;
    bool inOverrun_ = false;
    bool launchSwitch_ = false;
    bool launchCut_ = false;
    bool shiftCutRequest_ = false;
};

} // namespace simcore
