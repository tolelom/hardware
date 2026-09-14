#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

constexpr float kTestSampleRateHz = 44117.647f;

inline std::vector<float> makeSine(float frequencyHz, float amplitude,
                                   std::size_t sampleCount) {
    std::vector<float> samples(sampleCount);
    const double phaseStep = 2.0 * M_PI * frequencyHz / kTestSampleRateHz;
    for (std::size_t index = 0; index < sampleCount; ++index) {
        samples[index] = amplitude * static_cast<float>(std::sin(phaseStep * index));
    }
    return samples;
}

inline double rmsDecibels(const std::vector<float>& samples, std::size_t firstIndex) {
    double sumOfSquares = 0.0;
    for (std::size_t index = firstIndex; index < samples.size(); ++index) {
        sumOfSquares += static_cast<double>(samples[index]) * samples[index];
    }
    const double meanSquare = sumOfSquares / static_cast<double>(samples.size() - firstIndex);
    return 10.0 * std::log10(meanSquare);
}

inline std::size_t secondsToSamples(double seconds) {
    return static_cast<std::size_t>(seconds * kTestSampleRateHz);
}
