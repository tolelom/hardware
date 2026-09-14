#pragma once

// 노브 값(0~1)을 컴프레서 파라미터로 바꾼다. 범위 상수는 초기값이고 T16 청음으로 조정한다.
namespace parameter_mapping {

struct AttackRelease {
    float attackSeconds;
    float releaseSeconds;
};

constexpr float kMinimumRatio = 1.0f;
constexpr float kMaximumRatio = 20.0f;
constexpr float kFastestAttackSeconds = 0.0005f;
constexpr float kSlowestAttackSeconds = 0.020f;
constexpr float kSlowestReleaseSeconds = 1.0f;
constexpr float kFastestReleaseSeconds = 0.050f;
constexpr float kMinimumOutputDb = -30.0f;
constexpr float kMaximumOutputDb = 12.0f;

float clampKnob(float knob);
float ratioFromKnob(float knob);
AttackRelease attackReleaseFromKnob(float knob);
float outputGainDbFromKnob(float knob);
float inputDriveDbFromKnob(float knob, float maximumDriveDb);
float decibelsToGain(float decibels);

}  // namespace parameter_mapping
