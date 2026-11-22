#pragma once

// Glue between the engine simulation and the acoustic library: which pipes the engine has,
// and what the current operating point means for the sound. Pure functions, tested offline.

#include "acoustic/acoustic_params.hpp"
#include "acoustic/layout.hpp"
#include "domain/engine/engine_definition.hpp"
#include "domain/engine/engine_simulation.hpp"

namespace simcore {

/// Exhaust topology of the engine for the waveguide model (research.md §6.3).
/// The returned layout refers to definition.name; keep the definition alive.
[[nodiscard]] acoustic::LayoutSpec exhaustLayout(const EngineDefinition& definition);

/// Acoustic parameters of one simulation step (research.md §6.2 step 3).
[[nodiscard]] acoustic::AcousticParams acousticParams(const SimulationState& state,
                                                      const EngineDefinition& definition) noexcept;

/// Stereo position of the group outlets when the groups are not merged (e.g. V8 banks).
inline constexpr float kOutletPanSpread = 0.6F;

/// research.md §2.10a: below light-off the mixture hardly ignites in the exhaust, above
/// "fully hot" every late-burning charge can bang.
inline constexpr double kPopLightOff_K = 753.15; // 480 degC
inline constexpr double kPopFullyHot_K = 903.15; // 630 degC
/// Share of eligible exhaust events that really bang: keeps it a burble with bangs, not a
/// continuous rattle (roughly 10 per second at 6000 1/min with a strong pops tune).
inline constexpr double kAfterburnEventShare = 0.07;

/// 0 below light-off, 1 when fully hot, linear in between.
[[nodiscard]] double catalystHotness(double catalystTemperature_K) noexcept;

} // namespace simcore
