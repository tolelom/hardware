#include "effector_processor.h"

#include <algorithm>
#include <cmath>

#include "parameter_mapping.h"

namespace {

constexpr float kDcBlockerCutoffHz = 5.0f;
constexpr int kPgaMaximumStepBeforeMeasurement = 6;  // +9dB. M1 결과로 조정
constexpr float kQuietLevelDbfs = -50.0f;              // T13에서 조정
constexpr float kQuietHoldSeconds = 0.02f;             // T13에서 조정
constexpr float kClipLevelDbfs = -0.1f;
constexpr int kPgaAlignmentBlocks = 2;                 // T13에서 측정해 조정
constexpr float kEffectRampSeconds = 0.02f;
constexpr float kClipBlinkSeconds = 0.4f;
constexpr float kClipBlinkHalfPeriodSeconds = 0.05f;

int secondsToBlocks(float seconds, float blockSeconds) {
    return std::max(1, static_cast<int>(std::ceil(seconds / blockSeconds)));
}

}  // namespace

EffectorProcessor::EffectorProcessor(const AudioSettings_F32& settings)
    : AudioStream_F32(1, inputQueue_),
      sampleRateHz_(settings.sample_rate_Hz),
      blockSeconds_(settings.audio_block_samples / settings.sample_rate_Hz),
      dcBlocker_(settings.sample_rate_Hz, kDcBlockerCutoffHz),
      compressor_(settings.sample_rate_Hz),
      pga_({kPgaMaximumStepBeforeMeasurement, kQuietLevelDbfs,
            secondsToBlocks(kQuietHoldSeconds, blockSeconds_), kClipLevelDbfs,
            kPgaAlignmentBlocks}),
      meter_({{1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 14.0f, 20.0f},
              kClipLevelDbfs,
              secondsToBlocks(kClipBlinkSeconds, blockSeconds_),
              secondsToBlocks(kClipBlinkHalfPeriodSeconds, blockSeconds_)}) {}

void EffectorProcessor::setTestTone(float frequencyHz, float amplitude) {
    testToneFrequencyHz_ = frequencyHz;
    testToneAmplitude_ = amplitude;
}

bool EffectorProcessor::takePgaStepChange(int& step) {
    if (!codecStepChanged_) {
        return false;
    }
    codecStepChanged_ = false;
    step = codecStep_;
    return true;
}

void EffectorProcessor::update() {
    audio_block_f32_t* block = receiveWritable_f32();
    if (block == nullptr) {
        return;
    }
    const int length = std::min(block->length, kMaximumBlockSamples);
    float* samples = block->data;

    if (testToneAmplitude_ > 0.0f) {
        const double phaseStep = 2.0 * M_PI * testToneFrequencyHz_ / sampleRateHz_;
        for (int index = 0; index < length; ++index) {
            samples[index] = testToneAmplitude_ * static_cast<float>(std::sin(testTonePhase_));
            testTonePhase_ = std::fmod(testTonePhase_ + phaseStep, 2.0 * M_PI);
        }
    }

    float peak = 0.0f;
    for (int index = 0; index < length; ++index) {
        peak = std::max(peak, std::fabs(samples[index]));
    }
    lastBlockPeakDbfs_ = 20.0f * std::log10(std::max(peak, 1.0e-6f));

    pga_.processBlock(parameters_.inputDriveDb, lastBlockPeakDbfs_);
    const bool manualPga = parameters_.manualPgaStep >= 0;
    const int step = manualPga ? parameters_.manualPgaStep : pga_.pgaStep();
    if (step != codecStep_) {
        codecStep_ = step;
        codecStepChanged_ = true;
    }
    const float compensationDb =
        manualPga ? -PgaController::kStepDb * static_cast<float>(step) : pga_.compensationDb();
    compressor_.configure(parameters_.compressor);

    const float compensationGain = parameter_mapping::decibelsToGain(compensationDb);
    const float preGain = parameter_mapping::decibelsToGain(parameters_.inputDriveDb);
    dcBlocker_.process(samples, samples, static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
        dry_[index] = samples[index] * compensationGain;
        wet_[index] = dry_[index] * preGain;
    }
    compressor_.process(wet_, wet_, static_cast<std::size_t>(length));

    const float targetMix = parameters_.effectEnabled ? 1.0f : 0.0f;
    const float rampStep = blockSeconds_ / kEffectRampSeconds;
    effectMix_ = targetMix > effectMix_ ? std::min(targetMix, effectMix_ + rampStep)
                                        : std::max(targetMix, effectMix_ - rampStep);
    const float wetWeight = parameters_.blend * effectMix_;
    const float wetRestore = 1.0f / preGain;
    const float outputGain = parameter_mapping::decibelsToGain(
        effectMix_ * parameters_.outputGainDb + parameters_.unityTrimDb);
    for (int index = 0; index < length; ++index) {
        samples[index] = outputGain * ((1.0f - wetWeight) * dry_[index] +
                                       wetWeight * wet_[index] * wetRestore);
    }

    meterBits_ = meter_.processBlock(-compressor_.currentGainDb(), lastBlockPeakDbfs_);
    transmit(block);
    release(block);
}
