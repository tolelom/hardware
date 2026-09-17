#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "wav_file.h"

namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

void appendText(std::vector<std::uint8_t>& bytes, const char* text) {
    bytes.insert(bytes.end(), text, text + 4);
}

void appendLittleEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value, int byteCount) {
    for (int index = 0; index < byteCount; ++index) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFF));
    }
}

struct WavSpec {
    std::uint16_t formatTag;
    std::uint16_t subFormatTag;
    std::uint16_t channels;
    std::uint32_t sampleRateHz;
    std::uint16_t bitsPerSample;
    bool oddListChunkFirst;
};

std::vector<std::uint8_t> buildWav(const WavSpec& spec, const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> body;
    body.reserve(96 + data.size());
    appendText(body, "WAVE");
    if (spec.oddListChunkFirst) {
        appendText(body, "LIST");
        appendLittleEndian(body, 3, 4);
        body.insert(body.end(), {'a', 'b', 'c', 0});
    }
    const std::uint16_t blockAlign = spec.channels * (spec.bitsPerSample / 8);
    const bool extensible = spec.formatTag == kFormatExtensible;
    appendText(body, "fmt ");
    appendLittleEndian(body, extensible ? 40 : 16, 4);
    appendLittleEndian(body, spec.formatTag, 2);
    appendLittleEndian(body, spec.channels, 2);
    appendLittleEndian(body, spec.sampleRateHz, 4);
    appendLittleEndian(body, spec.sampleRateHz * blockAlign, 4);
    appendLittleEndian(body, blockAlign, 2);
    appendLittleEndian(body, spec.bitsPerSample, 2);
    if (extensible) {
        appendLittleEndian(body, 22, 2);
        appendLittleEndian(body, spec.bitsPerSample, 2);
        appendLittleEndian(body, 0, 4);
        appendLittleEndian(body, spec.subFormatTag, 2);
        const std::uint8_t guidTail[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                                          0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
        body.insert(body.end(), guidTail, guidTail + 14);
    }
    appendText(body, "data");
    appendLittleEndian(body, static_cast<std::uint32_t>(data.size()), 4);
    body.insert(body.end(), data.begin(), data.end());

    std::vector<std::uint8_t> bytes;
    appendText(bytes, "RIFF");
    appendLittleEndian(bytes, static_cast<std::uint32_t>(body.size()), 4);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

std::uint32_t floatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

}  // namespace

TEST_CASE("WAV decodes 16-bit PCM") {
    std::vector<std::uint8_t> data;
    appendLittleEndian(data, 16384, 2);
    appendLittleEndian(data, static_cast<std::uint16_t>(-32768), 2);
    const auto bytes = buildWav({kFormatPcm, 0, 1, 48000, 16, false}, data);
    std::string error;
    const auto audio = decodeWav(bytes, error);
    CHECK(audio.has_value());
    if (!audio) return;
    CHECK(audio->sampleRateHz == 48000);
    CHECK(audio->samples.size() == 2);
    CHECK_NEAR(audio->samples[0], 0.5, 1e-6);
    CHECK_NEAR(audio->samples[1], -1.0, 1e-6);
}

TEST_CASE("WAV decodes 24-bit PCM and keeps only the first channel") {
    std::vector<std::uint8_t> data;
    appendLittleEndian(data, 0x400000, 3);             // 왼쪽 +0.5
    appendLittleEndian(data, 0x7FFFFF, 3);             // 오른쪽, 버린다
    appendLittleEndian(data, 0xC00000, 3);             // 왼쪽 -0.5
    appendLittleEndian(data, 0x000000, 3);
    const auto bytes = buildWav({kFormatPcm, 0, 2, 48000, 24, false}, data);
    std::string error;
    const auto audio = decodeWav(bytes, error);
    CHECK(audio.has_value());
    if (!audio) return;
    CHECK(audio->samples.size() == 2);
    CHECK_NEAR(audio->samples[0], 0.5, 1e-6);
    CHECK_NEAR(audio->samples[1], -0.5, 1e-6);
}

TEST_CASE("WAV decodes 32-bit PCM") {
    std::vector<std::uint8_t> data;
    appendLittleEndian(data, 0xC0000000, 4);
    const auto bytes = buildWav({kFormatPcm, 0, 1, 48000, 32, false}, data);
    std::string error;
    const auto audio = decodeWav(bytes, error);
    CHECK(audio.has_value());
    if (!audio) return;
    CHECK_NEAR(audio->samples[0], -0.5, 1e-6);
}

TEST_CASE("WAV decodes extensible float after an odd-sized LIST chunk") {
    std::vector<std::uint8_t> data;
    appendLittleEndian(data, floatBits(0.25f), 4);
    appendLittleEndian(data, floatBits(-0.75f), 4);
    const auto bytes = buildWav({kFormatExtensible, kFormatFloat, 1, 44100, 32, true}, data);
    std::string error;
    const auto audio = decodeWav(bytes, error);
    CHECK(audio.has_value());
    if (!audio) return;
    CHECK(audio->sampleRateHz == 44100);
    CHECK(audio->samples.size() == 2);
    CHECK_NEAR(audio->samples[0], 0.25, 1e-7);
    CHECK_NEAR(audio->samples[1], -0.75, 1e-7);
}

TEST_CASE("WAV rejects a non-RIFF file and an unsupported format") {
    std::string error;
    CHECK(!decodeWav({'n', 'o', 'p', 'e'}, error).has_value());
    CHECK(!error.empty());

    std::vector<std::uint8_t> data(4, 0);
    const auto bytes = buildWav({kFormatPcm, 0, 1, 48000, 8, false}, data);
    error.clear();
    CHECK(!decodeWav(bytes, error).has_value());
    CHECK(!error.empty());
}

TEST_CASE("WAV float encoding decodes back to the same samples") {
    MonoAudio original;
    original.sampleRateHz = 48000;
    original.samples = {0.0f, 0.125f, -0.999f, 1.5f};
    std::string error;
    const auto decoded = decodeWav(encodeFloatWav(original), error);
    CHECK(decoded.has_value());
    if (!decoded) return;
    CHECK(decoded->sampleRateHz == 48000);
    CHECK(decoded->samples == original.samples);
}
