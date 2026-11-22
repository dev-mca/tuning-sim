#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simcore::acoustic {

/// One-pole low-pass: y += a (x - y), a = 1 - exp(-2 pi fc / fs) (research.md §9).
class OnePoleLowpass {
public:
    OnePoleLowpass() = default;
    OnePoleLowpass(float cutoffHz, float sampleRate) { setCutoff(cutoffHz, sampleRate); }

    void setCutoff(float cutoffHz, float sampleRate) noexcept {
        coefficient_ = 1.0F - std::exp(-2.0F * std::numbers::pi_v<float> * cutoffHz / sampleRate);
    }
    void reset(float value = 0.0F) noexcept { state_ = value; }
    float process(float input) noexcept {
        state_ += coefficient_ * (input - state_);
        return state_;
    }

private:
    float coefficient_ = 1.0F;
    float state_ = 0.0F;
};

/// Removes the DC offset (research.md §6.8): high-pass at a few Hz.
class DcBlocker {
public:
    DcBlocker(float cutoffHz, float sampleRate)
        : pole_(1.0F - 2.0F * std::numbers::pi_v<float> * cutoffHz / sampleRate) {}

    void reset() noexcept { previousInput_ = previousOutput_ = 0.0F; }
    float process(float input) noexcept {
        const float output = input - previousInput_ + pole_ * previousOutput_;
        previousInput_ = input;
        previousOutput_ = output;
        return output;
    }

private:
    float pole_;
    float previousInput_ = 0.0F;
    float previousOutput_ = 0.0F;
};

/// Second-order low-pass (RBJ audio EQ cookbook), Q = 1/sqrt(2) by default.
class BiquadLowpass {
public:
    void setCutoff(float cutoffHz, float sampleRate, float q = std::numbers::sqrt2_v<float> / 2) {
        const float omega = 2.0F * std::numbers::pi_v<float> * cutoffHz / sampleRate;
        const float alpha = std::sin(omega) / (2.0F * q);
        const float cosOmega = std::cos(omega);
        const float a0 = 1.0F + alpha;
        b0_ = (1.0F - cosOmega) / 2.0F / a0;
        b1_ = (1.0F - cosOmega) / a0;
        b2_ = b0_;
        a1_ = -2.0F * cosOmega / a0;
        a2_ = (1.0F - alpha) / a0;
    }
    void reset() noexcept { z1_ = z2_ = 0.0F; }
    float process(float input) noexcept { // transposed direct form II
        const float output = b0_ * input + z1_;
        z1_ = b1_ * input - a1_ * output + z2_;
        z2_ = b2_ * input - a2_ * output;
        return output;
    }

private:
    float b0_ = 1.0F, b1_ = 0.0F, b2_ = 0.0F, a1_ = 0.0F, a2_ = 0.0F;
    float z1_ = 0.0F, z2_ = 0.0F;
};

/// Moves towards a target with a maximum rate, then smooths with a one-pole filter.
/// Used for engine speed: no instant jumps (the crank has inertia), no zipper noise.
class SlewSmoother {
public:
    SlewSmoother(double maxRatePerSecond, double smoothingTimeS, double sampleRate)
        : maxStep_(maxRatePerSecond / sampleRate),
          coefficient_(1.0 - std::exp(-1.0 / (smoothingTimeS * sampleRate))) {}

    void jumpTo(double value) noexcept { limited_ = smoothed_ = value; }
    double process(double target) noexcept {
        limited_ += std::clamp(target - limited_, -maxStep_, maxStep_);
        smoothed_ += coefficient_ * (limited_ - smoothed_);
        return smoothed_;
    }
    [[nodiscard]] double value() const noexcept { return smoothed_; }

private:
    double maxStep_;
    double coefficient_;
    double limited_ = 0.0;
    double smoothed_ = 0.0;
};

/// Soft clipper instead of hard clipping (research.md §6.8).
inline float softClip(float x) noexcept {
    return std::tanh(x);
}

} // namespace simcore::acoustic
