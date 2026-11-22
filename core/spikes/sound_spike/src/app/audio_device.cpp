#include "app/audio_device.hpp"

#include "acoustic/offline_render.hpp"
#include "app/sound_bank.hpp"

#include <miniaudio.h>

#include <cstddef>
#include <span>

namespace sound_spike {

struct AudioDevice::Impl {
    ma_device device{};
    bool initialised = false;
    bool started = false;
};

namespace {

// Called by miniaudio on its real-time audio thread.
void dataCallback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frameCount) {
    auto* bank = static_cast<SoundBank*>(device->pUserData);
    std::span<float> samples(static_cast<float*>(output),
                             static_cast<std::size_t>(frameCount) * kStereo);
    bank->render(samples);
}

} // namespace

AudioDevice::AudioDevice(SoundBank& bank, std::uint32_t sampleRate)
    : impl_(std::make_unique<Impl>()) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = kStereo;
    config.sampleRate = sampleRate;
    config.periodSizeInFrames = kPeriodFrames;
    config.dataCallback = dataCallback;
    config.pUserData = &bank;

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        error_ = "could not open the audio output device";
        return;
    }
    impl_->initialised = true;
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        error_ = "could not start the audio output device";
        return;
    }
    impl_->started = true;
}

AudioDevice::~AudioDevice() {
    if (impl_->initialised) {
        ma_device_uninit(&impl_->device); // stops the callback before returning
    }
}

std::string AudioDevice::description() const {
    if (!impl_->initialised) {
        return "no device";
    }
    return std::string(impl_->device.playback.name) + " (" +
           ma_get_backend_name(impl_->device.pContext->backend) + ")";
}

} // namespace sound_spike
