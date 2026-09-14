#include "check.h"
#include "dc_blocker.h"
#include "signal_helpers.h"

TEST_CASE("DcBlocker removes a constant offset") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    float output = 0.0f;
    for (std::size_t index = 0; index < secondsToSamples(1.0); ++index) {
        output = blocker.processSample(0.5f);
    }
    CHECK_NEAR(output, 0.0, 1e-4);
}

TEST_CASE("DcBlocker keeps 1 kHz within 0.05 dB") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    std::vector<float> input = makeSine(1000.0f, 0.5f, secondsToSamples(1.0));
    std::vector<float> output(input.size());
    blocker.process(input.data(), output.data(), input.size());
    const std::size_t settled = secondsToSamples(0.5);
    CHECK_NEAR(rmsDecibels(output, settled) - rmsDecibels(input, settled), 0.0, 0.05);
}

TEST_CASE("DcBlocker attenuates 20 Hz by less than 1 dB") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    std::vector<float> input = makeSine(20.0f, 0.5f, secondsToSamples(2.0));
    std::vector<float> output(input.size());
    blocker.process(input.data(), output.data(), input.size());
    const std::size_t settled = secondsToSamples(1.0);
    const double changeDecibels = rmsDecibels(output, settled) - rmsDecibels(input, settled);
    CHECK(changeDecibels > -1.0);
    CHECK(changeDecibels < 0.0);
}
