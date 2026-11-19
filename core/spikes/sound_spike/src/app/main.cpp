// sound_spike: throwaway experiment for physically based engine sound (plan.md iteration 3).
//
//   sound_spike                  interactive playback (keys below)
//   sound_spike --export <dir>   render the 10 s comparison scenario of every layout to WAV

#include "acoustic/layout.hpp"
#include "acoustic/offline_render.hpp"
#include "adapters/wav_writer.hpp"
#include "app/audio_device.hpp"
#include "app/keyboard.hpp"
#include "app/sound_bank.hpp"
#include "io/comparison_export.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using namespace sound_spike;

constexpr double kMinSpeed_rpm = 700.0;
constexpr double kMaxSpeed_rpm = 8000.0;
constexpr double kSpeedStep_rpm = 250.0;
constexpr double kBigSpeedStep_rpm = 1000.0;
constexpr float kLoadStep = 0.1F;
constexpr double kRecordDuration_s = 10.0;
constexpr int kPollIntervalMs = 50;

void printHelp() {
    std::cout << "sound_spike - engine sound experiment (tuning-sim iteration 3)\n\n"
                 "  sound_spike                 interactive playback\n"
                 "  sound_spike --export <dir>  write the 10 s comparison WAVs and exit\n\n"
                 "keys: w/s speed +/-250   W/S speed +/-1000   d/a load +/-0.1\n"
                 "      1 I4   2 I6   3 V8 crossplane   4 V8 flatplane\n"
                 "      r record 10 s to recording_<layout>.wav   q quit\n";
}

int exportWavs(const std::filesystem::path& directory) {
    std::cout << "rendering " << kAllLayouts.size() << " layouts, " << kComparisonDuration_s
              << " s each ...\n";
    const auto files = exportComparisonWavs(directory);
    for (const auto& file : files) {
        std::cout << "  wrote " << file.string() << '\n';
    }
    return files.size() == kAllLayouts.size() ? 0 : 1;
}

int runInteractive() {
    SoundBank bank(static_cast<float>(kSampleRate), kRecordDuration_s);
    AudioDevice device(bank, kSampleRate);
    if (!device.ok()) {
        std::cerr << "error: " << device.error() << '\n';
        return 1;
    }
    printHelp();
    std::cout << "\naudio: " << device.description() << "\n\n";

    double speed = 900.0;
    float load = 0.15F;
    LayoutId layout = LayoutId::Inline6;
    bool recording = false;
    Keyboard keyboard;
    for (;;) {
        if (const auto key = keyboard.poll(kPollIntervalMs)) {
            switch (*key) {
            case 'w':
                speed += kSpeedStep_rpm;
                break;
            case 's':
                speed -= kSpeedStep_rpm;
                break;
            case 'W':
                speed += kBigSpeedStep_rpm;
                break;
            case 'S':
                speed -= kBigSpeedStep_rpm;
                break;
            case 'd':
                load += kLoadStep;
                break;
            case 'a':
                load -= kLoadStep;
                break;
            case '1':
                layout = LayoutId::Inline4;
                break;
            case '2':
                layout = LayoutId::Inline6;
                break;
            case '3':
                layout = LayoutId::V8Crossplane;
                break;
            case '4':
                layout = LayoutId::V8Flatplane;
                break;
            case 'r':
                if (!recording) {
                    bank.startRecording();
                    recording = true;
                }
                break;
            case 'q':
                std::cout << '\n';
                return 0;
            default:
                break;
            }
            speed = std::clamp(speed, kMinSpeed_rpm, kMaxSpeed_rpm);
            load = std::clamp(load, 0.0F, 1.0F);
            bank.setTargetSpeed(speed);
            bank.setTargetLoad(load);
            bank.selectLayout(layout);
        }
        if (recording && bank.recordingFinished()) {
            recording = false;
            const std::string file = "recording_" + std::string(layoutKey(layout)) + ".wav";
            const bool ok = writeWav16(file, bank.recording(), kStereo, kSampleRate);
            std::cout << '\n' << (ok ? "wrote " : "could not write ") << file << '\n';
        }
        std::printf("\r%-34s target %5.0f 1/min  now %5.0f 1/min  load %3.0f %%  %s   ",
                    std::string(makeLayout(layout).name).c_str(), speed, bank.currentSpeedRpm(),
                    static_cast<double>(load) * 100.0, recording ? "[REC]" : "     ");
        std::fflush(stdout);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2) {
        const std::string_view option = argv[1];
        if (option == "--export" && argc >= 3) {
            return exportWavs(argv[2]);
        }
        printHelp();
        return option == "--help" ? 0 : 1;
    }
    return runInteractive();
}
