#include "render_pipeline.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "effector_processor.h"
#include "parameter_mapping.h"
#include "serial_console.h"

namespace {

constexpr double kFullScaleSineRmsDbfs = -3.0103;
constexpr double kBandLowHz = 125.0;
constexpr double kBandHighHz = 8000.0;

// 오프라인 렌더에서는 콘솔 출력을 버리고, 파라미터를 적용했는지만 센다.
class RenderHost : public ConsoleHost {
public:
    explicit RenderHost(EffectorProcessor& processor) : processor_(processor) {}

    void write(const char*) override {}
    void applyParameters(const EffectorParameters& parameters) override {
        processor_.setParameters(parameters);
        ++applyCount;
    }
    void setTestTone(float frequencyHz, float amplitude) override {
        processor_.setTestTone(frequencyHz, amplitude);
        ++applyCount;
    }
    void resetToBootloader() override {}

    int applyCount = 0;

private:
    EffectorProcessor& processor_;
};

double bandToFullBandDb() {
    const double nyquistHz = kRenderSampleRateHz / 2.0;
    return 10.0 * std::log10(nyquistHz / (kBandHighHz - kBandLowHz));
}

}  // namespace

float scarlettToPedalGainDb() {
    return kScarlettInstrumentFullScaleDbu - kPedalInputFullScaleDbu;
}

double noiseFullBandRmsDbfs(double dynamicRangeDb) {
    return kFullScaleSineRmsDbfs - dynamicRangeDb + bandToFullBandDb();
}

std::vector<float> generateModuleNoise(double dynamicRangeDb, std::size_t sampleCount,
                                       std::uint32_t seed) {
    const double standardDeviation = std::pow(10.0, noiseFullBandRmsDbfs(dynamicRangeDb) / 20.0);
    std::mt19937 generator(seed);
    std::normal_distribution<double> distribution(0.0, standardDeviation);
    std::vector<float> noise(sampleCount);
    for (float& sample : noise) {
        sample = static_cast<float>(distribution(generator));
    }
    return noise;
}

double recordingFloorDynamicRangeDb(const std::vector<float>& scarlettSamples,
                                    std::size_t windowSamples) {
    const std::size_t window = std::max<std::size_t>(1, std::min(windowSamples, scarlettSamples.size()));
    double quietestMeanSquare = -1.0;
    for (std::size_t start = 0; start + window <= scarlettSamples.size(); start += window) {
        double sumOfSquares = 0.0;
        for (std::size_t index = start; index < start + window; ++index) {
            sumOfSquares += static_cast<double>(scarlettSamples[index]) * scarlettSamples[index];
        }
        const double meanSquare = sumOfSquares / static_cast<double>(window);
        if (quietestMeanSquare < 0.0 || meanSquare < quietestMeanSquare) {
            quietestMeanSquare = meanSquare;
        }
    }
    const double floorDbfs =
        10.0 * std::log10(std::max(quietestMeanSquare, 1.0e-30)) + scarlettToPedalGainDb();
    return kFullScaleSineRmsDbfs + bandToFullBandDb() - floorDbfs;
}

std::optional<MonoAudio> renderRecording(const MonoAudio& recording, const RenderRequest& request,
                                         std::string& error) {
    if (recording.sampleRateHz != kRenderSampleRateHz) {
        error = "sample rate must be " + std::to_string(kRenderSampleRateHz) + "Hz, got " +
                std::to_string(recording.sampleRateHz) + "Hz";
        return std::nullopt;
    }

    EffectorProcessor processor({static_cast<float>(kRenderSampleRateHz), kRenderBlockSamples});
    EffectorParameters parameters;
    processor.setParameters(parameters);
    RenderHost host(processor);
    SerialConsole console(processor, parameters, host, static_cast<float>(kRenderSampleRateHz));
    for (const std::string& command : request.commands) {
        const int applyCountBefore = host.applyCount;
        const std::string line = command + "\n";
        console.feed(line.data(), line.size());
        if (host.applyCount == applyCountBefore) {
            error = "command did not change parameters: " + command;
            return std::nullopt;
        }
    }

    const std::size_t count = recording.samples.size();
    const float toPedal = parameter_mapping::decibelsToGain(scarlettToPedalGainDb());
    std::vector<float> input(count);
    for (std::size_t index = 0; index < count; ++index) {
        input[index] = recording.samples[index] * toPedal;
    }
    if (request.dynamicRangeDb) {
        const std::vector<float> noise = generateModuleNoise(*request.dynamicRangeDb, count, request.seed);
        for (std::size_t index = 0; index < count; ++index) {
            input[index] += noise[index];
        }
    }

    MonoAudio rendered;
    rendered.sampleRateHz = kRenderSampleRateHz;
    rendered.samples.resize(count);
    for (std::size_t start = 0; start < count; start += kRenderBlockSamples) {
        const std::size_t length = std::min<std::size_t>(kRenderBlockSamples, count - start);
        processor.process(input.data() + start, rendered.samples.data() + start, length);
    }
    const float toScarlett = 1.0f / toPedal;
    for (float& sample : rendered.samples) {
        sample *= toScarlett;
    }
    return rendered;
}
