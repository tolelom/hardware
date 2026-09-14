#include "check.h"
#include "meter.h"

namespace {

Meter::Config testConfig() {
    return {{1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 14.0f, 20.0f}, -0.1f, 8, 2};
}

}  // namespace

TEST_CASE("Meter lights LEDs up to the gain reduction") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(0.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(1.0f, -30.0f) == 0x01);
    CHECK(meter.processBlock(6.0f, -30.0f) == 0x0F);
    CHECK(meter.processBlock(25.0f, -30.0f) == 0xFF);
}

TEST_CASE("Meter blinks all LEDs for the clip hold time (T15)") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(3.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x07);
}

TEST_CASE("Meter keeps blinking while the clip continues") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0x00);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0x00);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
}
