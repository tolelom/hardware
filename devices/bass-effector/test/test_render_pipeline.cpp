#include <cmath>
#include <complex>
#include <random>
#include <string>
#include <vector>

#include "check.h"
#include "render_pipeline.h"
#include "signal_helpers.h"

namespace {

void fourierTransform(std::vector<std::complex<double>>& values) {
    const std::size_t count = values.size();
    for (std::size_t index = 1, reversed = 0; index < count; ++index) {
        std::size_t bit = count >> 1;
        for (; reversed & bit; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values[index], values[reversed]);
        }
    }
    for (std::size_t length = 2; length <= count; length <<= 1) {
        const double angle = -2.0 * M_PI / static_cast<double>(length);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < count; start += length) {
            std::complex<double> rotation(1.0, 0.0);
            for (std::size_t offset = 0; offset < length / 2; ++offset) {
                const std::complex<double> upper = values[start + offset];
                const std::complex<double> lower = values[start + offset + length / 2] * rotation;
                values[start + offset] = upper + lower;
                values[start + offset + length / 2] = upper - lower;
                rotation *= step;
            }
        }
    }
}

double bandRmsDbfs(const std::vector<float>& samples, double lowHz, double highHz) {
    std::vector<std::complex<double>> values(samples.begin(), samples.end());
    fourierTransform(values);
    const double count = static_cast<double>(values.size());
    const double binHz = kRenderSampleRateHz / count;
    double power = 0.0;
    for (std::size_t bin = 1; bin < values.size() / 2; ++bin) {
        const double frequencyHz = bin * binHz;
        if (frequencyHz >= lowHz && frequencyHz <= highHz) {
            power += std::norm(values[bin]);
        }
    }
    return 10.0 * std::log10(2.0 * power / (count * count));
}

MonoAudio makeRecording(std::vector<float> samples) {
    MonoAudio audio;
    audio.sampleRateHz = kRenderSampleRateHz;
    audio.samples = std::move(samples);
    return audio;
}

}  // namespace

TEST_CASE("Scarlett recording is lowered by 4.7dB to the pedal ADC scale") {
    CHECK_NEAR(scarlettToPedalGainDb(), -4.7, 1e-5);
}

TEST_CASE("Module noise has the band RMS that the T7 dynamic range defines") {
    const std::vector<float> noise = generateModuleNoise(100.0, 1u << 16, 7);
    CHECK_NEAR(bandRmsDbfs(noise, 125.0, 8000.0), -103.0103, 0.3);
    CHECK_NEAR(rmsDecibels(noise, 0), noiseFullBandRmsDbfs(100.0), 0.2);
    CHECK_NEAR(noiseFullBandRmsDbfs(100.0), -103.0103 + 10.0 * std::log10(24000.0 / 7875.0), 1e-3);
}

TEST_CASE("Module noise repeats for a seed and changes with another seed") {
    const std::vector<float> first = generateModuleNoise(90.0, 1000, 3);
    CHECK(first == generateModuleNoise(90.0, 1000, 3));
    CHECK(first != generateModuleNoise(90.0, 1000, 4));
}

TEST_CASE("Recording floor is read from the quietest window") {
    std::vector<float> samples = makeSine(100.0f, 0.3f, secondsToSamples(1.0));
    std::vector<float> quiet = generateModuleNoise(80.0, secondsToSamples(1.0), 11);
    const float toScarlett = std::pow(10.0f, -scarlettToPedalGainDb() / 20.0f);
    for (float& sample : quiet) {
        sample *= toScarlett;
    }
    samples.insert(samples.end(), quiet.begin(), quiet.end());
    CHECK_NEAR(recordingFloorDynamicRangeDb(samples, secondsToSamples(0.5)), 80.0, 0.3);
}

TEST_CASE("Render with the effect off returns the recording at its own level") {
    const std::vector<float> sine = makeSine(1000.0f, 0.2f, secondsToSamples(1.0));
    std::string error;
    const auto rendered = renderRecording(makeRecording(sine), {{"effect 0"}, std::nullopt, 1}, error);
    CHECK(rendered.has_value());
    if (!rendered) return;
    CHECK(rendered->sampleRateHz == kRenderSampleRateHz);
    CHECK(rendered->samples.size() == sine.size());
    const std::size_t settled = secondsToSamples(0.1);
    CHECK_NEAR(rmsDecibels(rendered->samples, settled), rmsDecibels(sine, settled), 0.05);
}

TEST_CASE("Render applies console commands to the processor") {
    const std::vector<float> sine = makeSine(1000.0f, 0.5f, secondsToSamples(1.0));
    std::string error;
    const auto bypassed = renderRecording(makeRecording(sine), {{"effect 0"}, std::nullopt, 1}, error);
    const auto compressed = renderRecording(
        makeRecording(sine), {{"threshold -40", "ratio 1", "attrel 0"}, std::nullopt, 1}, error);
    CHECK(bypassed.has_value() && compressed.has_value());
    if (!bypassed || !compressed) return;
    const std::size_t settled = secondsToSamples(0.5);
    CHECK(rmsDecibels(compressed->samples, settled) < rmsDecibels(bypassed->samples, settled) - 10.0);
}

TEST_CASE("Render adds module noise before the processor") {
    const std::vector<float> silence(secondsToSamples(1.0), 0.0f);
    std::string error;
    const auto rendered = renderRecording(makeRecording(silence), {{"effect 0"}, 60.0, 5}, error);
    CHECK(rendered.has_value());
    if (!rendered) return;
    const double expected = noiseFullBandRmsDbfs(60.0) - scarlettToPedalGainDb();
    CHECK_NEAR(rmsDecibels(rendered->samples, secondsToSamples(0.1)), expected, 0.3);
}

TEST_CASE("Render rejects an unknown command and a sample rate other than 48kHz") {
    const std::vector<float> sine = makeSine(1000.0f, 0.2f, 480);
    std::string error;
    CHECK(!renderRecording(makeRecording(sine), {{"pga 3"}, std::nullopt, 1}, error).has_value());
    CHECK(error.find("pga 3") != std::string::npos);

    MonoAudio other = makeRecording(sine);
    other.sampleRateHz = 44100;
    error.clear();
    CHECK(!renderRecording(other, {{}, std::nullopt, 1}, error).has_value());
    CHECK(!error.empty());
}
