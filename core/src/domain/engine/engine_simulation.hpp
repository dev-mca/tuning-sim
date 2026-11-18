#pragma once

// Fixed-step engine simulation: ECU + engine model + manifold + crank dynamics.
// Port of lab simulation.py (docs/plan.md §2.3, research.md §9):
// - manifold pressure: explicit Euler with substeps (stiff near wide-open throttle),
// - crank speed: semi-implicit Euler, J d(omega)/dt = T_e - T_load (research.md §2.11).

#include "domain/ecu/ecu.hpp"
#include "domain/engine/engine_model.hpp"

#include <optional>

namespace simcore {

inline constexpr double kDefaultTimeStep_s = 1.0e-3;
/// Near wide-open throttle the manifold time constant drops below 1 ms; 10 substeps of
/// 0.1 ms keep explicit Euler stable. Must match lab simulation.py for the golden file.
inline constexpr int kManifoldSubsteps = 10;
inline constexpr double kMinManifoldPressure_Pa = 1.0e3;

struct SimulationState {
    double time_s;
    double speed_rad_s;
    double manifoldPressure_Pa;
    double throttle;
    double loadTorque_Nm;
    EcuCommand command;
    EngineOutputs engine;
};

class EngineSimulation {
public:
    /// Uses the engine's stock tune unless `tune` is given; idle speed unless a start speed is.
    explicit EngineSimulation(const EngineDefinition& definition,
                              std::optional<Tune> tune = std::nullopt,
                              const Table2D* trueVe = nullptr,
                              std::optional<double> initialSpeed_rad_s = std::nullopt,
                              double timeStep_s = kDefaultTimeStep_s);

    /// Advances by one time step with throttle 0..1 and the load (brake) torque.
    SimulationState step(double throttle, double loadTorque_Nm) noexcept;

    [[nodiscard]] double timeStep_s() const noexcept { return timeStep_s_; }
    [[nodiscard]] double speed_rad_s() const noexcept { return speed_rad_s_; }
    [[nodiscard]] const EngineDefinition& definition() const noexcept { return definition_; }
    [[nodiscard]] Ecu& ecu() noexcept { return ecu_; }
    [[nodiscard]] const Ecu& ecu() const noexcept { return ecu_; }

private:
    void integrateManifold(double throttle, double dt) noexcept;

    EngineDefinition definition_;
    EngineModel model_;
    Ecu ecu_;
    double timeStep_s_;
    double time_s_ = 0.0;
    double speed_rad_s_;
    double manifoldPressure_Pa_;
    double manifoldTemperature_K_;
};

} // namespace simcore
