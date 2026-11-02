#pragma once

#include <cstddef>
#include <cstdint>

#include "compressor.h"
#include "dc_blocker.h"
#include "meter.h"

struct EffectorParameters {
    float inputDriveDb = 0.0f;
    CompressorSettings compressor;
    float blend = 1.0f;
    float outputGainDb = 0.0f;
    float unityTrimDb = 0.0f;
    bool effectEnabled = true;
};

// DC 제거, 프리게인, 컴프레서, Blend, Output을 한 블록에서 처리한다(설계서 6.2절).
// 플랫폼 헤더가 없다. 오디오 콜백이 process()를 부르고, 파라미터는 콜백 밖에서
// setParameters()로 넣는다(동시 접근 보호는 호출자 몫).
class EffectorProcessor {
public:
    struct Settings {
        float sampleRateHz;
        int blockSamples;
    };

    static constexpr int kMaximumBlockSamples = 128;

    explicit EffectorProcessor(const Settings& settings);

    void setParameters(const EffectorParameters& parameters) { parameters_ = parameters; }
    void setTestTone(float frequencyHz, float amplitude);
    void process(const float* input, float* output, std::size_t count);

    float currentGainDb() const { return compressor_.currentGainDb(); }
    float lastBlockPeakDbfs() const { return lastBlockPeakDbfs_; }
    std::uint8_t meterBits() const { return meterBits_; }

private:
    float sampleRateHz_;
    float blockSeconds_;
    EffectorParameters parameters_;
    DcBlocker dcBlocker_;
    Compressor compressor_;
    Meter meter_;
    float effectMix_ = 1.0f;
    float dry_[kMaximumBlockSamples];
    float wet_[kMaximumBlockSamples];
    float testToneFrequencyHz_ = 0.0f;
    float testToneAmplitude_ = 0.0f;
    double testTonePhase_ = 0.0;
    float lastBlockPeakDbfs_ = -120.0f;
    std::uint8_t meterBits_ = 0;
};
