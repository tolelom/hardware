#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct MonoAudio {
    int sampleRateHz = 0;
    std::vector<float> samples;
};

// 16/24/32비트 PCM, 32비트 float, WAVE_FORMAT_EXTENSIBLE을 읽는다. 여러 채널이면 첫 채널만 쓴다.
// 실패하면 nullopt를 돌려주고 error에 이유를 적는다.
std::optional<MonoAudio> decodeWav(const std::vector<std::uint8_t>& bytes, std::string& error);

// 32비트 float 모노 WAV로 쓴다.
std::vector<std::uint8_t> encodeFloatWav(const MonoAudio& audio);
