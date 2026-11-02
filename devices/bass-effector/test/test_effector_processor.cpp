#include <cmath>
#include <vector>

#include "check.h"
#include "effector_processor.h"
#include "signal_helpers.h"

namespace {

constexpr std::size_t kBlockSamples = 48;

EffectorProcessor::Settings testSettings() {
    return {kTestSampleRateHz, static_cast<int>(kBlockSamples)};
}

std::vector<float> runBlocks(EffectorProcessor& processor, const std::vector<float>& input) {
    std::vector<float> output(input.size());
    for (std::size_t start = 0; start + kBlockSamples <= input.size(); start += kBlockSamples) {
        processor.process(input.data() + start, output.data() + start, kBlockSamples);
    }
    return output;
}

}  // namespace

TEST_CASE("Processor passes the signal at unity with the effect off") {
    EffectorProcessor processor(testSettings());
    EffectorParameters parameters;
    parameters.effectEnabled = false;
    processor.setParameters(parameters);
    const std::vector<float> input = makeSine(1000.0f, 0.1f, secondsToSamples(1.0));
    const std::vector<float> output = runBlocks(processor, input);
    const std::size_t settled = secondsToSamples(0.5);
    CHECK_NEAR(rmsDecibels(output, settled), rmsDecibels(input, settled), 0.05);
}

TEST_CASE("Processor applies the unity trim to the output") {
    EffectorProcessor processor(testSettings());
    EffectorParameters parameters;
    parameters.effectEnabled = false;
    parameters.unityTrimDb = -6.0f;
    processor.setParameters(parameters);
    const std::vector<float> input = makeSine(1000.0f, 0.1f, secondsToSamples(1.0));
    const std::vector<float> output = runBlocks(processor, input);
    const std::size_t settled = secondsToSamples(0.5);
    CHECK_NEAR(rmsDecibels(output, settled), rmsDecibels(input, settled) - 6.0, 0.05);
}

TEST_CASE("Processor test tone replaces the input and reports the block peak") {
    EffectorProcessor processor(testSettings());
    EffectorParameters parameters;
    parameters.effectEnabled = false;
    processor.setParameters(parameters);
    processor.setTestTone(1000.0f, 0.1f);
    const std::vector<float> silence(secondsToSamples(0.5), 0.0f);
    const std::vector<float> output = runBlocks(processor, silence);
    CHECK_NEAR(rmsDecibels(output, secondsToSamples(0.25)), 20.0 * std::log10(0.1 / std::sqrt(2.0)), 0.1);
    CHECK_NEAR(processor.lastBlockPeakDbfs(), -20.0f, 0.5f);
}

TEST_CASE("Processor compresses with the effect on and blend fully wet") {
    EffectorProcessor processor(testSettings());
    EffectorParameters parameters;
    parameters.compressor.thresholdDbfs = -40.0f;
    parameters.compressor.ratio = 4.0f;
    parameters.blend = 1.0f;
    processor.setParameters(parameters);
    const float amplitude = std::sqrt(2.0f) * 0.1f;  // -20dBFS RMS
    const std::vector<float> input = makeSine(1000.0f, amplitude, secondsToSamples(2.0));
    const std::vector<float> output = runBlocks(processor, input);
    CHECK_NEAR(processor.currentGainDb(), -15.0f, 0.5f);
    CHECK_NEAR(rmsDecibels(output, secondsToSamples(1.5)), -35.0, 0.5);
    CHECK(processor.meterBits() != 0);
}

TEST_CASE("Processor keeps the dry level when blend is zero") {
    EffectorProcessor processor(testSettings());
    EffectorParameters parameters;
    parameters.compressor.thresholdDbfs = -40.0f;
    parameters.blend = 0.0f;
    processor.setParameters(parameters);
    const std::vector<float> input = makeSine(1000.0f, 0.1f, secondsToSamples(1.0));
    const std::vector<float> output = runBlocks(processor, input);
    const std::size_t settled = secondsToSamples(0.5);
    CHECK_NEAR(rmsDecibels(output, settled), rmsDecibels(input, settled), 0.05);
}
