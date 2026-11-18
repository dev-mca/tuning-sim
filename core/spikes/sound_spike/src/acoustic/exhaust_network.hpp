#pragma once

#include "acoustic/layout.hpp"
#include "dsp/delay_line.hpp"
#include "dsp/filters.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace sound_spike {

/// Stereo sample pair.
struct StereoSample {
    float left = 0.0F;
    float right = 0.0F;
};

/// Exhaust system as a network of digital waveguides (research.md §6.3).
///
/// Every pipe holds two delay lines: one wave travelling downstream, one upstream.
/// Pipe ends are connected to
/// - a valve (cylinder): the pulse is injected, the returning wave is partly reflected,
/// - a junction (collector): scattering junction, p_J = 2 sum(G_i p_i+) / sum(G_i), G_i = A_i,
/// - an outlet (open tailpipe end): reflection r = -lowpass, the rest is radiated (= output).
/// All buffers are allocated in the constructor; process() never allocates.
class ExhaustNetwork {
public:
    ExhaustNetwork(const LayoutSpec& layout, float sampleRate);

    void reset() noexcept;

    /// One sample: per-cylinder source pressures in, radiated stereo sound out.
    StereoSample process(std::span<const float> cylinderSources) noexcept;

    [[nodiscard]] std::size_t pipeCount() const noexcept { return pipes_.size(); }

    static constexpr float kValveReflection = 0.6F; // mostly closed valve, lossy
    static constexpr float kPipeGain = 0.985F;      // wall friction and heat loss per pass
    static constexpr float kPipeLossCutoffHz = 9000.0F;

private:
    enum class End { Upstream, Downstream };

    struct Port {
        std::size_t pipe;
        End end;
    };

    struct Pipe {
        Pipe(const PipeSpec& spec, float sampleRate);
        DelayLine downstreamWave; // travels from the upstream end to the downstream end
        DelayLine upstreamWave;   // travels back
        double delaySamples;
        float area_m2;
        OnePoleLowpass downstreamLoss;
        OnePoleLowpass upstreamLoss;
        float arrivingAtDownstream = 0.0F;
        float arrivingAtUpstream = 0.0F;
        float leavingUpstreamEnd = 0.0F;   // written into downstreamWave
        float leavingDownstreamEnd = 0.0F; // written into upstreamWave
    };

    struct Junction {
        std::vector<Port> ports;
    };

    struct Outlet {
        std::size_t pipe;
        float pan;
        OnePoleLowpass radiationLowpass;
    };

    float& arriving(const Port& port) noexcept;
    float& leaving(const Port& port) noexcept;

    std::vector<Pipe> pipes_;
    std::vector<Junction> junctions_;
    std::vector<Outlet> outlets_;
    std::size_t cylinderCount_;
};

} // namespace sound_spike
