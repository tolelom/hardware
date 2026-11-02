#pragma once

#include <cstddef>

// 1차 DC 제거 필터. y[n] = x[n] - x[n-1] + pole * y[n-1]
class DcBlocker {
public:
    DcBlocker(float sampleRateHz, float cutoffHz);

    void reset();
    float processSample(float input);
    void process(const float* input, float* output, std::size_t count);

private:
    float pole_;
    float previousInput_ = 0.0f;
    float previousOutput_ = 0.0f;
};
