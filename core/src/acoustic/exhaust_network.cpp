#include "acoustic/exhaust_network.hpp"

#include <cmath>
#include <numbers>

namespace simcore::acoustic {

namespace {

constexpr double kMinDelaySamples = 2.0;

float pipeArea(const PipeSpec& spec) {
    return static_cast<float>(std::numbers::pi * spec.diameter_m * spec.diameter_m / 4.0);
}

/// An open pipe end radiates high frequencies well and reflects low ones (research.md §6.3).
/// The transition lies near ka = 1, i.e. f = c / (2 pi a) with a = pipe radius.
float radiationCutoffHz(const PipeSpec& spec) {
    const double radius_m = spec.diameter_m / 2.0;
    return static_cast<float>(spec.soundSpeed_m_s / (2.0 * std::numbers::pi * radius_m));
}

/// Equal-power pan: -1 left, 0 centre, +1 right.
StereoSample pan(float value, float position) {
    const float angle = (position + 1.0F) * std::numbers::pi_v<float> / 4.0F;
    return {value * std::cos(angle), value * std::sin(angle)};
}

} // namespace

ExhaustNetwork::Pipe::Pipe(const PipeSpec& spec, float sampleRate)
    : downstreamWave(spec.length_m * sampleRate / spec.soundSpeed_m_s + kMinDelaySamples),
      upstreamWave(spec.length_m * sampleRate / spec.soundSpeed_m_s + kMinDelaySamples),
      delaySamples(std::max(kMinDelaySamples, spec.length_m * sampleRate / spec.soundSpeed_m_s)),
      area_m2(pipeArea(spec)), downstreamLoss(kPipeLossCutoffHz, sampleRate),
      upstreamLoss(kPipeLossCutoffHz, sampleRate) {}

ExhaustNetwork::ExhaustNetwork(const LayoutSpec& layout, float sampleRate)
    : cylinderCount_(layout.cylinders.size()) {
    // Pipe order: primaries (index = cylinder), group collectors, optional merged pipe.
    for (const CylinderSpec& cylinder : layout.cylinders) {
        pipes_.emplace_back(cylinder.primary, sampleRate);
    }
    const std::size_t firstCollector = pipes_.size();
    for (const GroupSpec& group : layout.groups) {
        pipes_.emplace_back(group.collector, sampleRate);
    }

    // One junction per group: its primaries flow in, its collector pipe flows out.
    junctions_.resize(layout.groups.size());
    for (std::size_t cylinder = 0; cylinder < layout.cylinders.size(); ++cylinder) {
        const auto group = static_cast<std::size_t>(layout.cylinders[cylinder].group);
        junctions_[group].ports.push_back({cylinder, End::Downstream});
    }
    for (std::size_t group = 0; group < layout.groups.size(); ++group) {
        junctions_[group].ports.push_back({firstCollector + group, End::Upstream});
    }

    if (layout.mergeGroups) {
        const std::size_t merged = pipes_.size();
        pipes_.emplace_back(layout.mergedPipe, sampleRate);
        Junction yPipe;
        for (std::size_t group = 0; group < layout.groups.size(); ++group) {
            yPipe.ports.push_back({firstCollector + group, End::Downstream});
        }
        yPipe.ports.push_back({merged, End::Upstream});
        junctions_.push_back(yPipe);
        outlets_.push_back(
            {merged, 0.0F, OnePoleLowpass(radiationCutoffHz(layout.mergedPipe), sampleRate)});
    } else {
        for (std::size_t group = 0; group < layout.groups.size(); ++group) {
            const GroupSpec& spec = layout.groups[group];
            outlets_.push_back({firstCollector + group, spec.pan,
                                OnePoleLowpass(radiationCutoffHz(spec.collector), sampleRate)});
        }
    }
}

void ExhaustNetwork::reset() noexcept {
    for (Pipe& pipe : pipes_) {
        pipe.downstreamWave.reset();
        pipe.upstreamWave.reset();
        pipe.downstreamLoss.reset();
        pipe.upstreamLoss.reset();
        pipe.arrivingAtDownstream = pipe.arrivingAtUpstream = 0.0F;
        pipe.leavingUpstreamEnd = pipe.leavingDownstreamEnd = 0.0F;
    }
    for (Outlet& outlet : outlets_) {
        outlet.radiationLowpass.reset();
    }
}

float& ExhaustNetwork::arriving(const Port& port) noexcept {
    Pipe& pipe = pipes_[port.pipe];
    return port.end == End::Downstream ? pipe.arrivingAtDownstream : pipe.arrivingAtUpstream;
}

float& ExhaustNetwork::leaving(const Port& port) noexcept {
    Pipe& pipe = pipes_[port.pipe];
    return port.end == End::Downstream ? pipe.leavingDownstreamEnd : pipe.leavingUpstreamEnd;
}

StereoSample ExhaustNetwork::process(std::span<const float> cylinderSources) noexcept {
    // 1. Waves arrive at both ends of every pipe (delayed, with losses).
    for (Pipe& pipe : pipes_) {
        pipe.arrivingAtDownstream =
            kPipeGain * pipe.downstreamLoss.process(pipe.downstreamWave.read(pipe.delaySamples));
        pipe.arrivingAtUpstream =
            kPipeGain * pipe.upstreamLoss.process(pipe.upstreamWave.read(pipe.delaySamples));
    }

    // 2. Valves: inject the cylinder pulse, partly reflect what comes back.
    for (std::size_t cylinder = 0; cylinder < cylinderCount_; ++cylinder) {
        Pipe& primary = pipes_[cylinder];
        primary.leavingUpstreamEnd =
            cylinderSources[cylinder] + kValveReflection * primary.arrivingAtUpstream;
    }

    // 3. Junctions: scattering with admittance proportional to the pipe area (§6.3).
    for (const Junction& junction : junctions_) {
        float weightedSum = 0.0F;
        float areaSum = 0.0F;
        for (const Port& port : junction.ports) {
            const float area = pipes_[port.pipe].area_m2;
            weightedSum += area * arriving(port);
            areaSum += area;
        }
        const float junctionPressure = 2.0F * weightedSum / areaSum;
        for (const Port& port : junction.ports) {
            leaving(port) = junctionPressure - arriving(port);
        }
    }

    // 4. Outlets: low frequencies reflect inverted (r = -lowpass), the rest radiates.
    StereoSample output;
    for (Outlet& outlet : outlets_) {
        Pipe& pipe = pipes_[outlet.pipe];
        const float incoming = pipe.arrivingAtDownstream;
        const float reflected = -outlet.radiationLowpass.process(incoming);
        pipe.leavingDownstreamEnd = reflected;
        const StereoSample radiated = pan(incoming + reflected, outlet.pan);
        output.left += radiated.left;
        output.right += radiated.right;
    }

    // 5. Waves leave the ends into the delay lines.
    for (Pipe& pipe : pipes_) {
        pipe.downstreamWave.write(pipe.leavingUpstreamEnd);
        pipe.upstreamWave.write(pipe.leavingDownstreamEnd);
    }
    return output;
}

} // namespace simcore::acoustic
