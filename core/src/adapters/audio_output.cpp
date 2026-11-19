#include "adapters/audio_output.hpp"

#include <miniaudio.h>

#include <cstddef>
#include <span>

namespace simcore {

struct AudioOutput::Impl {
    ma_device device{};
    bool initialised = false;
};

namespace {

// Called by miniaudio on its real-time audio thread (research.md §6.8).
void dataCallback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frameCount) {
    auto* renderer = static_cast<IAudioRenderer*>(device->pUserData);
    renderer->render(
        std::span<float>(static_cast<float*>(output),
                         static_cast<std::size_t>(frameCount) * AudioOutput::kChannels));
}

} // namespace

AudioOutput::AudioOutput(IAudioRenderer& renderer, std::uint32_t sampleRate)
    : impl_(std::make_unique<Impl>()) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = kChannels;
    config.sampleRate = sampleRate;
    config.periodSizeInFrames = kPeriodFrames;
    config.dataCallback = dataCallback;
    config.pUserData = &renderer;

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        info_ = {false, "could not open the audio output device"};
        return;
    }
    impl_->initialised = true;
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        info_ = {false, "could not start the audio output device"};
        return;
    }
    info_ = {true, std::string(impl_->device.playback.name) + " (" +
                       ma_get_backend_name(impl_->device.pContext->backend) + ")"};
}

AudioOutput::~AudioOutput() {
    if (impl_->initialised) {
        ma_device_uninit(&impl_->device); // stops the callback before returning
    }
}

} // namespace simcore
