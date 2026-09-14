#include <algorithm>
#include <cmath>
#include <vector>

#include "check.h"
#include "compressor.h"
#include "reference/openaudio_compressor.h"
#include "signal_helpers.h"

namespace {

double averageGainDb(Compressor& compressor, const std::vector<float>& input,
                     std::size_t firstIndex) {
    std::vector<float> output(1);
    double sum = 0.0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        compressor.process(&input[index], output.data(), 1);
        if (index >= firstIndex) {
            sum += compressor.currentGainDb();
        }
    }
    return sum / static_cast<double>(input.size() - firstIndex);
}

double expectedStaticGainDb(float sineAmplitude, float thresholdDbfs, float ratio) {
    const double levelDb = 20.0 * std::log10(sineAmplitude) - 10.0 * std::log10(2.0);
    return std::min(0.0, (levelDb - thresholdDbfs) * (1.0 / ratio - 1.0));
}

std::vector<float> makeLevelSteps() {
    std::vector<float> samples;
    const float amplitudes[] = {0.01f, 0.5f, 0.05f, 0.9f, 0.02f};
    for (float amplitude : amplitudes) {
        std::vector<float> segment = makeSine(1000.0f, amplitude, secondsToSamples(0.5));
        samples.insert(samples.end(), segment.begin(), segment.end());
    }
    return samples;
}

}  // namespace

TEST_CASE("Compressor leaves signal below threshold untouched") {
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({-20.0f, 4.0f, 0.005f, 0.2f, 0.0f});
    std::vector<float> input = makeSine(1000.0f, 0.01f, secondsToSamples(1.0));
    CHECK_NEAR(averageGainDb(compressor, input, secondsToSamples(0.5)), 0.0, 0.01);
}

TEST_CASE("Compressor static gain matches the ratio above threshold") {
    Compressor compressor(kTestSampleRateHz);
    // 어택이 빠르면 게인이 레벨 리플의 봉우리를 따라가 평균이 정적 곡선보다 눌린다.
    // 정적 곡선만 보려고 어택을 50ms로 둔다.
    compressor.configure({-20.0f, 4.0f, 0.05f, 0.2f, 0.0f});
    std::vector<float> input = makeSine(1000.0f, 0.5f, secondsToSamples(3.0));
    CHECK_NEAR(averageGainDb(compressor, input, secondsToSamples(2.0)),
               expectedStaticGainDb(0.5f, -20.0f, 4.0f), 0.1);
}

TEST_CASE("Compressor gain tracks the OpenAudio reference within 0.1 dB (T9)") {
    const float thresholdDbfs = -24.0f;
    const float ratio = 6.0f;
    const float attackSeconds = 0.003f;
    const float releaseSeconds = 0.25f;
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({thresholdDbfs, ratio, attackSeconds, releaseSeconds, 0.0f});
    OpenAudioCompressorReference reference(kTestSampleRateHz, thresholdDbfs, ratio,
                                           attackSeconds, releaseSeconds);
    std::vector<float> input = makeLevelSteps();
    float output = 0.0f;
    double largestDifferenceDb = 0.0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        compressor.process(&input[index], &output, 1);
        reference.processSample(input[index]);
        // 참조는 레벨 전력을 1.0(0dBFS)에서 시작해 첫 1초 동안 초기 과도가 남는다
        if (index >= secondsToSamples(1.0)) {
            largestDifferenceDb =
                std::max(largestDifferenceDb,
                                  static_cast<double>(
                             std::fabs(compressor.currentGainDb() - reference.currentGainDb())));
        }
    }
    CHECK(largestDifferenceDb <= 0.1);
}

TEST_CASE("Compressor attack time matches the reference within 10 percent (T9)") {
    const float attackSeconds = 0.01f;
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({-30.0f, 8.0f, attackSeconds, 0.3f, 0.0f});
    OpenAudioCompressorReference reference(kTestSampleRateHz, -30.0f, 8.0f, attackSeconds, 0.3f);
    std::vector<float> quiet = makeSine(1000.0f, 0.005f, secondsToSamples(1.0));
    std::vector<float> loud = makeSine(1000.0f, 0.8f, secondsToSamples(0.5));
    float output = 0.0f;
    for (float sample : quiet) {
        compressor.process(&sample, &output, 1);
        reference.processSample(sample);
    }
    const double finalGainDb = expectedStaticGainDb(0.8f, -30.0f, 8.0f);
    const double crossingDb = 0.632 * finalGainDb;
    long compressorCrossing = -1;
    long referenceCrossing = -1;
    for (std::size_t index = 0; index < loud.size(); ++index) {
        compressor.process(&loud[index], &output, 1);
        reference.processSample(loud[index]);
        if (compressorCrossing < 0 && compressor.currentGainDb() <= crossingDb) {
            compressorCrossing = static_cast<long>(index);
        }
        if (referenceCrossing < 0 && reference.currentGainDb() <= crossingDb) {
            referenceCrossing = static_cast<long>(index);
        }
    }
    CHECK(compressorCrossing > 0);
    CHECK(referenceCrossing > 0);
    CHECK_NEAR(static_cast<double>(compressorCrossing), static_cast<double>(referenceCrossing),
               0.1 * referenceCrossing);
}

TEST_CASE("Sidechain high-pass reduces compression of low notes only") {
    CompressorSettings withoutFilter{-20.0f, 4.0f, 0.005f, 0.2f, 0.0f};
    CompressorSettings withFilter = withoutFilter;
    withFilter.sidechainHighPassHz = 150.0f;

    Compressor lowWithout(kTestSampleRateHz);
    lowWithout.configure(withoutFilter);
    Compressor lowWith(kTestSampleRateHz);
    lowWith.configure(withFilter);
    std::vector<float> lowNote = makeSine(40.0f, 0.5f, secondsToSamples(2.0));
    const double lowGainWithout = averageGainDb(lowWithout, lowNote, secondsToSamples(1.5));
    const double lowGainWith = averageGainDb(lowWith, lowNote, secondsToSamples(1.5));
    CHECK(lowGainWithout < -6.0);
    CHECK(lowGainWith > -1.0);

    Compressor highWithout(kTestSampleRateHz);
    highWithout.configure(withoutFilter);
    Compressor highWith(kTestSampleRateHz);
    highWith.configure(withFilter);
    std::vector<float> highNote = makeSine(1000.0f, 0.5f, secondsToSamples(2.0));
    CHECK_NEAR(averageGainDb(highWith, highNote, secondsToSamples(1.5)),
               averageGainDb(highWithout, highNote, secondsToSamples(1.5)), 0.1);
}
