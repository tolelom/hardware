#pragma once

#include <cstddef>

struct CompressorSettings {
    float thresholdDbfs = -20.0f;
    float ratio = 4.0f;
    float attackSeconds = 0.005f;
    float releaseSeconds = 0.200f;
    float sidechainHighPassHz = 0.0f;  // 0이면 끔
};

// RMS 검출 피드포워드 컴프레서. 검출기 앞에만 사이드체인 HPF를 둔다.
// 레벨 추정과 게인 평활 식은 OpenAudio AudioEffectCompressor_F32와 같다.
class Compressor {
public:
    explicit Compressor(float sampleRateHz);

    void configure(const CompressorSettings& settings);
    void reset();
    void process(const float* input, float* output, std::size_t count);
    float currentGainDb() const { return gainDb_; }

private:
    struct HighPass {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        float process(float input);
    };

    float sampleRateHz_;
    CompressorSettings settings_;
    float attackCoefficient_ = 0.0f;
    float releaseCoefficient_ = 0.0f;
    float levelCoefficient_ = 0.0f;
    bool sidechainFilterEnabled_ = false;
    HighPass sidechainFilter_;
    float levelPower_ = 1.0e-13f;
    float gainDb_ = 0.0f;
};
