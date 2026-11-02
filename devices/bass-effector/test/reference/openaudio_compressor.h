#pragma once

// 대조군: OpenAudio_ArduinoLibrary AudioEffectCompressor_F32 (Chip Audette, MIT)의
// 레벨 추정·목표 게인·평활 식을 샘플 단위로 옮긴 것. HPF와 프리게인은 뺐다.

#include <algorithm>
#include <cmath>
#include <cstddef>

class OpenAudioCompressorReference {
public:
    OpenAudioCompressorReference(float sampleRateHz, float thresholdDbfs, float ratio,
                                 float attackSeconds, float releaseSeconds)
        : thresholdDbfs_(thresholdDbfs), ratio_(std::max(0.001f, ratio)) {
        attackConst_ = std::exp(-1.0f / (attackSeconds * sampleRateHz));
        releaseConst_ = std::exp(-1.0f / (releaseSeconds * sampleRateHz));
        const float levelSeconds =
            std::max(0.002f, std::min(attackSeconds, releaseSeconds) / 5.0f);
        levelConst_ = std::exp(-1.0f / (levelSeconds * sampleRateHz));
    }

    float processSample(float input) {
        float power = levelConst_ * previousLevelPower_ + (1.0f - levelConst_) * input * input;
        previousLevelPower_ = power;
        const float levelDb = 10.0f * log10Approx(power);
        float targetGainDb = (levelDb - thresholdDbfs_) / ratio_ - (levelDb - thresholdDbfs_);
        if (targetGainDb > 0.0f) {
            targetGainDb = 0.0f;
        }
        if (targetGainDb < previousGainDb_) {
            previousGainDb_ = attackConst_ * previousGainDb_ + (1.0f - attackConst_) * targetGainDb;
        } else {
            previousGainDb_ =
                releaseConst_ * previousGainDb_ + (1.0f - releaseConst_) * targetGainDb;
        }
        if (previousLevelPower_ < 1.0e-13f) {
            previousLevelPower_ = 1.0e-13f;
        }
        return input * std::exp(2.302585092994f * previousGainDb_ / 20.0f);
    }

    float currentGainDb() const { return previousGainDb_; }

private:
    static float log2Approx(float value) {
        int exponent = 0;
        const float fraction = std::frexp(std::fabs(value), &exponent);
        float result = 1.23149591368684f;
        result *= fraction;
        result += -4.11852516267426f;
        result *= fraction;
        result += 6.02197014179219f;
        result *= fraction;
        result += -3.13396450166353f;
        result += static_cast<float>(exponent);
        return result;
    }

    static float log10Approx(float value) { return log2Approx(value) * 0.3010299956639812f; }

    float thresholdDbfs_;
    float ratio_;
    float attackConst_;
    float releaseConst_;
    float levelConst_;
    float previousLevelPower_ = 1.0f;
    float previousGainDb_ = 0.0f;
};
