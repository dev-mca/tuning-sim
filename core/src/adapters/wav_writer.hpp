#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace simcore {

/// Encodes interleaved float samples in [-1, 1] as a 16-bit PCM WAV file (RIFF format).
[[nodiscard]] std::vector<std::uint8_t>
encodeWav16(std::span<const float> interleaved, std::uint16_t channels, std::uint32_t sampleRate);

/// Writes encodeWav16() to a file; returns false on I/O errors.
[[nodiscard]] bool writeWav16(const std::filesystem::path& path, std::span<const float> interleaved,
                              std::uint16_t channels, std::uint32_t sampleRate);

} // namespace simcore
