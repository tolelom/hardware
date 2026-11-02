#include "effector_processor.h"

#include <algorithm>
#include <cmath>

#include "parameter_mapping.h"

namespace {

constexpr float kDcBlockerCutoffHz = 5.0f;
constexpr float kClipLevelDbfs = -0.1f;
constexpr float kEffectRampSeconds = 0.02f;
constexpr float kClipBlinkSeconds = 0.4f;
constexpr float kClipBlinkHalfPeriodSeconds = 0.05f;

int secondsToBlocks(float seconds, float blockSeconds) {
    return std::max(1, static_cast<int>(std::ceil(seconds / blockSeconds)));
}

}  // namespace

EffectorProcessor::EffectorProcessor(const Settings& settings)
    : sampleRateHz_(settings.sampleRateHz),
      blockSeconds_(static_cast<float>(settings.blockSamples) / settings.sampleRateHz),
      dcBlocker_(settings.sampleRateHz, kDcBlockerCutoffHz),
      compressor_(settings.sampleRateHz),
      meter_({{1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 14.0f, 20.0f},
              kClipLevelDbfs,
              secondsToBlocks(kClipBlinkSeconds, blockSeconds_),
              secondsToBlocks(kClipBlinkHalfPeriodSeconds, blockSeconds_)}) {}

void EffectorProcessor::setTestTone(float frequencyHz, float amplitude) {
    testToneFrequencyHz_ = frequencyHz;
    testToneAmplitude_ = amplitude;
}

void EffectorProcessor::process(const float* input, float* output, std::size_t count) {
    const std::size_t length = std::min(count, static_cast<std::size_t>(kMaximumBlockSamples));

    if (testToneAmplitude_ > 0.0f) {
        const double phaseStep = 2.0 * M_PI * testToneFrequencyHz_ / sampleRateHz_;
        for (std::size_t index = 0; index < length; ++index) {
            dry_[index] = testToneAmplitude_ * static_cast<float>(std::sin(testTonePhase_));
            testTonePhase_ = std::fmod(testTonePhase_ + phaseStep, 2.0 * M_PI);
        }
    } else {
        std::copy(input, input + length, dry_);
    }

    float peak = 0.0f;
    for (std::size_t index = 0; index < length; ++index) {
        peak = std::max(peak, std::fabs(dry_[index]));
    }
    lastBlockPeakDbfs_ = 20.0f * std::log10(std::max(peak, 1.0e-6f));

    compressor_.configure(parameters_.compressor);
    const float preGain = parameter_mapping::decibelsToGain(parameters_.inputDriveDb);
    dcBlocker_.process(dry_, dry_, length);
    for (std::size_t index = 0; index < length; ++index) {
        wet_[index] = dry_[index] * preGain;
    }
    compressor_.process(wet_, wet_, length);

    const float targetMix = parameters_.effectEnabled ? 1.0f : 0.0f;
    const float rampStep = blockSeconds_ / kEffectRampSeconds;
    effectMix_ = targetMix > effectMix_ ? std::min(targetMix, effectMix_ + rampStep)
                                        : std::max(targetMix, effectMix_ - rampStep);
    const float wetWeight = parameters_.blend * effectMix_;
    const float wetRestore = 1.0f / preGain;
    const float outputGain = parameter_mapping::decibelsToGain(
        effectMix_ * parameters_.outputGainDb + parameters_.unityTrimDb);
    for (std::size_t index = 0; index < length; ++index) {
        output[index] = outputGain * ((1.0f - wetWeight) * dry_[index] +
                                      wetWeight * wet_[index] * wetRestore);
    }
    for (std::size_t index = length; index < count; ++index) {
        output[index] = 0.0f;
    }

    meterBits_ = meter_.processBlock(-compressor_.currentGainDb(), lastBlockPeakDbfs_);
}
