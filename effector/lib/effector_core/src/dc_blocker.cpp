#include "dc_blocker.h"

#include <cmath>

DcBlocker::DcBlocker(float sampleRateHz, float cutoffHz)
    : pole_(static_cast<float>(std::exp(-2.0 * M_PI * cutoffHz / sampleRateHz))) {}

void DcBlocker::reset() {
    previousInput_ = 0.0f;
    previousOutput_ = 0.0f;
}

float DcBlocker::processSample(float input) {
    const float output = input - previousInput_ + pole_ * previousOutput_;
    previousInput_ = input;
    previousOutput_ = output;
    return output;
}

void DcBlocker::process(const float* input, float* output, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = processSample(input[index]);
    }
}
