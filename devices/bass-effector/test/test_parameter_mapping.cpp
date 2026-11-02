#include "check.h"
#include "parameter_mapping.h"

using namespace parameter_mapping;

TEST_CASE("Output knob at noon is unity") {
    CHECK_NEAR(outputGainDbFromKnob(0.5f), 0.0, 1e-6);
    CHECK_NEAR(outputGainDbFromKnob(0.0f), kMinimumOutputDb, 1e-6);
    CHECK_NEAR(outputGainDbFromKnob(1.0f), kMaximumOutputDb, 1e-6);
    CHECK(outputGainDbFromKnob(0.4f) < outputGainDbFromKnob(0.6f));
}

TEST_CASE("Ratio knob spans 1:1 to 20:1") {
    CHECK_NEAR(ratioFromKnob(0.0f), 1.0, 1e-5);
    CHECK_NEAR(ratioFromKnob(1.0f), 20.0, 1e-4);
    CHECK(ratioFromKnob(0.3f) < ratioFromKnob(0.7f));
}

TEST_CASE("Attack and release move in opposite directions") {
    const AttackRelease fast = attackReleaseFromKnob(0.0f);
    const AttackRelease slow = attackReleaseFromKnob(1.0f);
    CHECK_NEAR(fast.attackSeconds, kFastestAttackSeconds, 1e-7);
    CHECK_NEAR(fast.releaseSeconds, kSlowestReleaseSeconds, 1e-6);
    CHECK_NEAR(slow.attackSeconds, kSlowestAttackSeconds, 1e-6);
    CHECK_NEAR(slow.releaseSeconds, kFastestReleaseSeconds, 1e-6);
}

TEST_CASE("Knob values outside 0 to 1 are clamped") {
    CHECK_NEAR(inputDriveDbFromKnob(-0.5f, 30.0f), 0.0, 1e-6);
    CHECK_NEAR(inputDriveDbFromKnob(1.5f, 30.0f), 30.0, 1e-5);
    CHECK_NEAR(decibelsToGain(-6.0206f), 0.5, 1e-4);
}
