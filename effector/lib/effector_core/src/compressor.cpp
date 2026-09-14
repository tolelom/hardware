#include "compressor.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumLevelPower = 1.0e-13f;
constexpr float kMinimumLevelTimeSeconds = 0.002f;
}  // namespace

float Compressor::HighPass::process(float input) {
    const float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    return output;
}

Compressor::Compressor(float sampleRateHz) : sampleRateHz_(sampleRateHz) {
    configure(CompressorSettings{});
}

void Compressor::configure(const CompressorSettings& settings) {
    settings_ = settings;
    attackCoefficient_ = std::exp(-1.0f / (settings.attackSeconds * sampleRateHz_));
    releaseCoefficient_ = std::exp(-1.0f / (settings.releaseSeconds * sampleRateHz_));
    const float levelSeconds = std::max(
        kMinimumLevelTimeSeconds, std::min(settings.attackSeconds, settings.releaseSeconds) / 5.0f);
    levelCoefficient_ = std::exp(-1.0f / (levelSeconds * sampleRateHz_));

    sidechainFilterEnabled_ = settings.sidechainHighPassHz > 0.0f;
    if (sidechainFilterEnabled_) {
        // RBJ 2차 하이패스, Q = 1/sqrt(2)
        const double omega = 2.0 * M_PI * settings.sidechainHighPassHz / sampleRateHz_;
        const double alpha = std::sin(omega) / (2.0 * M_SQRT1_2);
        const double cosine = std::cos(omega);
        const double a0 = 1.0 + alpha;
        sidechainFilter_.b0 = static_cast<float>((1.0 + cosine) / 2.0 / a0);
        sidechainFilter_.b1 = static_cast<float>(-(1.0 + cosine) / a0);
        sidechainFilter_.b2 = sidechainFilter_.b0;
        sidechainFilter_.a1 = static_cast<float>(-2.0 * cosine / a0);
        sidechainFilter_.a2 = static_cast<float>((1.0 - alpha) / a0);
    }
}

void Compressor::reset() {
    levelPower_ = kMinimumLevelPower;
    gainDb_ = 0.0f;
    sidechainFilter_.x1 = sidechainFilter_.x2 = sidechainFilter_.y1 = sidechainFilter_.y2 = 0.0f;
}

void Compressor::process(const float* input, float* output, std::size_t count) {
    const float slope = 1.0f / settings_.ratio - 1.0f;
    for (std::size_t index = 0; index < count; ++index) {
        const float sidechain =
            sidechainFilterEnabled_ ? sidechainFilter_.process(input[index]) : input[index];
        levelPower_ = levelCoefficient_ * levelPower_ +
                      (1.0f - levelCoefficient_) * sidechain * sidechain;
        const float levelDb = 10.0f * std::log10(std::max(levelPower_, kMinimumLevelPower));
        const float targetGainDb = std::min(0.0f, (levelDb - settings_.thresholdDbfs) * slope);
        const float coefficient =
            targetGainDb < gainDb_ ? attackCoefficient_ : releaseCoefficient_;
        gainDb_ = coefficient * gainDb_ + (1.0f - coefficient) * targetGainDb;
        output[index] = input[index] * std::pow(10.0f, gainDb_ / 20.0f);
    }
}
