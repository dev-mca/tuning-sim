#pragma once

// Minimal speed-density ECU (docs/research.md §2.2, §2.4, §3); port of lab ecu.py.
// Concepts as in Speeduino/rusEFI, no code taken from them (GPL).

#include "domain/ecu/table.hpp"
#include "domain/engine/engine_definition.hpp"

namespace simcore {

/// The tables the user tunes. Axes and units as in EngineTables.
struct Tune {
    Table2D ve;
    Table2D lambdaTarget;
    Table2D ignition;

    [[nodiscard]] static Tune fromTables(const EngineTables& tables) {
        return {tables.ve, tables.lambdaTarget, tables.ignition};
    }
};

struct EcuCommand {
    double fuelMassPerCycle_kg;
    double ignition_deg;
    double lambdaTarget;
    bool fuelCut;
};

class Ecu {
public:
    Ecu(Tune tune, double displacement_m3, double revLimit_rad_s, double revLimitHysteresis_rad_s);

    [[nodiscard]] EcuCommand update(double speed_rad_s, double manifoldPressure_Pa,
                                    double intakeTemperature_K) noexcept;

    [[nodiscard]] const Tune& tune() const noexcept { return tune_; }
    [[nodiscard]] Tune& tune() noexcept { return tune_; } // live table editing

private:
    void updateRevLimiter(double speed_rad_s) noexcept;

    Tune tune_;
    double displacement_m3_;
    double revLimit_rad_s_;
    double revResume_rad_s_;
    bool fuelCut_ = false;
};

} // namespace simcore
