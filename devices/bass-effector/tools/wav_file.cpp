#include "wav_file.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

std::uint32_t readLittleEndian(const std::uint8_t* bytes, int byteCount) {
    std::uint32_t value = 0;
    for (int index = 0; index < byteCount; ++index) {
        value |= static_cast<std::uint32_t>(bytes[index]) << (8 * index);
    }
    return value;
}

bool sameTag(const std::uint8_t* bytes, const char* tag) {
    return std::memcmp(bytes, tag, 4) == 0;
}

float decodeSample(const std::uint8_t* bytes, std::uint16_t format, std::uint16_t bitsPerSample) {
    if (format == kFormatFloat) {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof value);
        return value;
    }
    const std::uint32_t raw = readLittleEndian(bytes, bitsPerSample / 8);
    const int shift = 32 - bitsPerSample;
    const std::int32_t value = static_cast<std::int32_t>(raw << shift) >> shift;
    return static_cast<float>(static_cast<double>(value) / static_cast<double>(1u << (bitsPerSample - 1)));
}

void appendText(std::vector<std::uint8_t>& bytes, const char* text) {
    bytes.insert(bytes.end(), text, text + 4);
}

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value, int byteCount) {
    for (int index = 0; index < byteCount; ++index) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF));
    }
}

}  // namespace

std::optional<MonoAudio> decodeWav(const std::vector<std::uint8_t>& bytes, std::string& error) {
    if (bytes.size() < 12 || !sameTag(bytes.data(), "RIFF") || !sameTag(bytes.data() + 8, "WAVE")) {
        error = "not a RIFF WAVE file";
        return std::nullopt;
    }

    bool formatFound = false;
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRateHz = 0;
    std::uint16_t bitsPerSample = 0;

    std::size_t position = 12;
    while (position + 8 <= bytes.size()) {
        const std::uint8_t* header = bytes.data() + position;
        const std::size_t chunkSize = readLittleEndian(header + 4, 4);
        const std::size_t bodyStart = position + 8;
        const std::size_t available = bytes.size() - bodyStart;

        if (sameTag(header, "fmt ")) {
            if (chunkSize < 16 || chunkSize > available) {
                error = "fmt chunk is truncated";
                return std::nullopt;
            }
            const std::uint8_t* body = bytes.data() + bodyStart;
            format = static_cast<std::uint16_t>(readLittleEndian(body, 2));
            channels = static_cast<std::uint16_t>(readLittleEndian(body + 2, 2));
            sampleRateHz = readLittleEndian(body + 4, 4);
            bitsPerSample = static_cast<std::uint16_t>(readLittleEndian(body + 14, 2));
            if (format == kFormatExtensible) {
                if (chunkSize < 26) {
                    error = "extensible fmt chunk is truncated";
                    return std::nullopt;
                }
                format = static_cast<std::uint16_t>(readLittleEndian(body + 24, 2));
            }
            const bool pcm = format == kFormatPcm &&
                             (bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32);
            const bool floating = format == kFormatFloat && bitsPerSample == 32;
            if (!(pcm || floating) || channels == 0) {
                error = "unsupported format " + std::to_string(format) + " with " +
                        std::to_string(bitsPerSample) + " bits";
                return std::nullopt;
            }
            formatFound = true;
        } else if (sameTag(header, "data")) {
            if (!formatFound) {
                error = "data chunk comes before fmt chunk";
                return std::nullopt;
            }
            const std::size_t frameBytes = static_cast<std::size_t>(channels) * (bitsPerSample / 8);
            const std::size_t dataBytes = std::min(chunkSize, available);
            MonoAudio audio;
            audio.sampleRateHz = static_cast<int>(sampleRateHz);
            audio.samples.reserve(dataBytes / frameBytes);
            for (std::size_t offset = 0; offset + frameBytes <= dataBytes; offset += frameBytes) {
                audio.samples.push_back(decodeSample(bytes.data() + bodyStart + offset, format, bitsPerSample));
            }
            return audio;
        }
        position = bodyStart + chunkSize + (chunkSize & 1);
    }
    error = formatFound ? "no data chunk" : "no fmt chunk";
    return std::nullopt;
}

std::vector<std::uint8_t> encodeFloatWav(const MonoAudio& audio) {
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(audio.samples.size() * sizeof(float));
    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + dataBytes);
    appendText(bytes, "RIFF");
    appendLittleEndian(bytes, 36 + dataBytes, 4);
    appendText(bytes, "WAVE");
    appendText(bytes, "fmt ");
    appendLittleEndian(bytes, 16, 4);
    appendLittleEndian(bytes, kFormatFloat, 2);
    appendLittleEndian(bytes, 1, 2);
    appendLittleEndian(bytes, static_cast<std::uint32_t>(audio.sampleRateHz), 4);
    appendLittleEndian(bytes, static_cast<std::uint32_t>(audio.sampleRateHz) * sizeof(float), 4);
    appendLittleEndian(bytes, sizeof(float), 2);
    appendLittleEndian(bytes, 32, 2);
    appendText(bytes, "data");
    appendLittleEndian(bytes, dataBytes, 4);
    for (const float sample : audio.samples) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &sample, sizeof bits);
        appendLittleEndian(bytes, bits, 4);
    }
    return bytes;
}
