#include "parameter_mapping.h"

#include <algorithm>
#include <cmath>

namespace parameter_mapping {

namespace {
float logInterpolate(float from, float to, float fraction) {
    return from * std::pow(to / from, fraction);
}
}  // namespace

float clampKnob(float knob) {
    return std::clamp(knob, 0.0f, 1.0f);
}

float ratioFromKnob(float knob) {
    return logInterpolate(kMinimumRatio, kMaximumRatio, clampKnob(knob));
}

AttackRelease attackReleaseFromKnob(float knob) {
    const float fraction = clampKnob(knob);
    return {logInterpolate(kFastestAttackSeconds, kSlowestAttackSeconds, fraction),
            logInterpolate(kSlowestReleaseSeconds, kFastestReleaseSeconds, fraction)};
}

float outputGainDbFromKnob(float knob) {
    const float fraction = clampKnob(knob);
    if (fraction <= 0.5f) {
        return kMinimumOutputDb * (1.0f - fraction / 0.5f);
    }
    return kMaximumOutputDb * ((fraction - 0.5f) / 0.5f);
}

float inputDriveDbFromKnob(float knob, float maximumDriveDb) {
    return clampKnob(knob) * maximumDriveDb;
}

float decibelsToGain(float decibels) {
    return std::pow(10.0f, decibels / 20.0f);
}

}  // namespace parameter_mapping
