// 실기기 T9 대조값: 같은 설정의 네이티브 컴프레서에 1kHz 사인을 3초 넣고 마지막 1초 평균 게인을 출력한다.
// 사용: static_gain <thresholdDbfs> <ratio> <attackSeconds> <releaseSeconds> <toneDbfs>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "compressor.h"

int main(int argumentCount, char** arguments) {
    if (argumentCount != 6) {
        std::fprintf(stderr, "usage: static_gain threshold ratio attack release toneDbfs\n");
        return 2;
    }
    const float sampleRateHz = 44100.0f;
    CompressorSettings settings;
    settings.thresholdDbfs = std::strtof(arguments[1], nullptr);
    settings.ratio = std::strtof(arguments[2], nullptr);
    settings.attackSeconds = std::strtof(arguments[3], nullptr);
    settings.releaseSeconds = std::strtof(arguments[4], nullptr);
    const float amplitude = std::pow(10.0f, std::strtof(arguments[5], nullptr) / 20.0f);

    Compressor compressor(sampleRateHz);
    compressor.configure(settings);
    const std::size_t totalSamples = static_cast<std::size_t>(3.0f * sampleRateHz);
    const std::size_t averageFrom = static_cast<std::size_t>(2.0f * sampleRateHz);
    const double phaseStep = 2.0 * M_PI * 1000.0 / sampleRateHz;
    double sum = 0.0;
    float output = 0.0f;
    for (std::size_t index = 0; index < totalSamples; ++index) {
        const float input = amplitude * static_cast<float>(std::sin(phaseStep * index));
        compressor.process(&input, &output, 1);
        if (index >= averageFrom) {
            sum += compressor.currentGainDb();
        }
    }
    std::printf("%.3f\n", sum / static_cast<double>(totalSamples - averageFrom));
    return 0;
}
