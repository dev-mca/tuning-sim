#include "io/wav_writer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string_view>

namespace sound_spike {

namespace {

constexpr std::uint16_t kBitsPerSample = 16;
constexpr std::uint16_t kBytesPerSample = kBitsPerSample / 8;
constexpr std::uint16_t kPcmFormat = 1;
constexpr std::uint32_t kFmtChunkSize = 16;
constexpr std::uint32_t kHeaderSizeAfterRiffSize = 36; // "WAVE" + fmt chunk + data header
constexpr float kInt16Max = 32767.0F;

void appendTag(std::vector<std::uint8_t>& out, std::string_view tag) {
    out.insert(out.end(), tag.begin(), tag.end());
}

// WAV is little-endian, independent of the machine: write byte by byte.
void appendLe16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendLe32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

} // namespace

std::vector<std::uint8_t> encodeWav16(std::span<const float> interleaved, std::uint16_t channels,
                                      std::uint32_t sampleRate) {
    const auto dataSize = static_cast<std::uint32_t>(interleaved.size() * kBytesPerSample);
    const auto blockAlign = static_cast<std::uint16_t>(channels * kBytesPerSample);

    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSizeAfterRiffSize + 8 + dataSize);
    appendTag(out, "RIFF");
    appendLe32(out, kHeaderSizeAfterRiffSize + dataSize);
    appendTag(out, "WAVE");
    appendTag(out, "fmt ");
    appendLe32(out, kFmtChunkSize);
    appendLe16(out, kPcmFormat);
    appendLe16(out, channels);
    appendLe32(out, sampleRate);
    appendLe32(out, sampleRate * blockAlign); // byte rate
    appendLe16(out, blockAlign);
    appendLe16(out, kBitsPerSample);
    appendTag(out, "data");
    appendLe32(out, dataSize);
    for (const float sample : interleaved) {
        const float scaled = std::round(std::clamp(sample, -1.0F, 1.0F) * kInt16Max);
        appendLe16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(scaled)));
    }
    return out;
}

bool writeWav16(const std::filesystem::path& path, std::span<const float> interleaved,
                std::uint16_t channels, std::uint32_t sampleRate) {
    const std::vector<std::uint8_t> bytes = encodeWav16(interleaved, channels, sampleRate);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

} // namespace sound_spike
