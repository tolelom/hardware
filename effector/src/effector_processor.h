#pragma once

#include <OpenAudio_ArduinoLibrary.h>

#include <cstdint>

#include "compressor.h"
#include "dc_blocker.h"
#include "meter.h"
#include "pga_controller.h"

struct EffectorParameters {
    float inputDriveDb = 0.0f;
    CompressorSettings compressor;
    float blend = 1.0f;
    float outputGainDb = 0.0f;
    float unityTrimDb = 0.0f;
    bool effectEnabled = true;
    int manualPgaStep = -1;  // 0 이상이면 자동 규칙 대신 이 단계로 고정(T5 측정용)
};

// DC 제거, PGA 보상, 컴프레서, Blend, Output을 한 블록에서 처리한다(설계서 6.2절).
class EffectorProcessor : public AudioStream_F32 {
public:
    explicit EffectorProcessor(const AudioSettings_F32& settings);

    // 오디오 인터럽트를 막은 상태(AudioNoInterrupts)에서 부른다
    void setParameters(const EffectorParameters& parameters) { parameters_ = parameters; }
    void setTestTone(float frequencyHz, float amplitude);
    bool takePgaStepChange(int& step);

    float currentGainDb() const { return compressor_.currentGainDb(); }
    float lastBlockPeakDbfs() const { return lastBlockPeakDbfs_; }
    std::uint8_t meterBits() const { return meterBits_; }
    int pgaStep() const { return codecStep_; }

    void update() override;

private:
    static constexpr int kMaximumBlockSamples = 128;

    audio_block_f32_t* inputQueue_[1];
    float sampleRateHz_;
    float blockSeconds_;
    EffectorParameters parameters_;
    DcBlocker dcBlocker_;
    Compressor compressor_;
    PgaController pga_;
    Meter meter_;
    float effectMix_ = 1.0f;
    float dry_[kMaximumBlockSamples];
    float wet_[kMaximumBlockSamples];
    float testToneFrequencyHz_ = 0.0f;
    float testToneAmplitude_ = 0.0f;
    double testTonePhase_ = 0.0;
    float lastBlockPeakDbfs_ = -120.0f;
    std::uint8_t meterBits_ = 0;
    int codecStep_ = 0;
    bool codecStepChanged_ = false;
};
