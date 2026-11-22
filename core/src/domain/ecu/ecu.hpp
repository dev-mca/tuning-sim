#pragma once

// Minimal speed-density ECU (docs/research.md §2.2, §2.4, §3); port of lab ecu.py.
// Rev limiter (fuel cut with hysteresis) and overrun fuel cut (research.md §3.3). With the cut
// switched off the ECU keeps fuelling in overrun with optional retard and extra fuel: the
// "pops & bangs" map (research.md §3.3, §2.10a).
// Concepts as in Speeduino/rusEFI, no code taken from them (GPL).

#include "domain/ecu/table.hpp"
#include "domain/engine/engine_definition.hpp"

namespace simcore {

/// What the ECU does when the driver lifts off at speed (research.md §3.3).
struct OverrunSettings {
    bool fuelCut = true;     // false: keep fuelling in overrun (needed for pops)
    double retard_deg = 0.0; // ignition taken back from the map while fuelling in overrun
    double enrichment = 0.0; // extra fuel while fuelling in overrun, fraction (0.1 = +10 %)
};

/// The tables and settings the user tunes. Axes and units as in EngineTables.
struct Tune {
    Table2D ve;
    Table2D lambdaTarget;
    Table2D ignition;
    OverrunSettings overrun;

    [[nodiscard]] static Tune fromTables(const EngineTables& tables) {
        return {tables.ve, tables.lambdaTarget, tables.ignition, {}};
    }
};

struct EcuCommand {
    double fuelMassPerCycle_kg;
    double ignition_deg;
    double lambdaTarget;
    bool fuelCut;
    bool overrunFueling = false; // fuelling in overrun with the cut switched off: pops possible
};

class Ecu {
public:
    Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s,
        OverrunCutParameters overrunCut);

    /// `throttle` (0..1) decides whether the driver lifted off (overrun fuel cut).
    [[nodiscard]] EcuCommand update(double speed_rad_s, double manifoldPressure_Pa,
                                    double intakeTemperature_K, double throttle) noexcept;

    [[nodiscard]] const Tune& tune() const noexcept { return tune_; }
    [[nodiscard]] Tune& tune() noexcept { return tune_; } // live table editing

private:
    void updateRevLimiter(double speed_rad_s) noexcept;
    void updateOverrun(double speed_rad_s, double throttle) noexcept;

    Tune tune_;
    double displacement_m3_;
    double revLimit_rad_s_;
    double revResume_rad_s_;
    OverrunCutParameters overrun_;
    bool limiterCut_ = false;
    bool inOverrun_ = false;
};

} // namespace simcore
