// 스칼렛 INST로 녹음한 DI WAV를 코어에 통과시켜 청음용 WAV를 만든다(1단계 계획 Task 10).
// 사용: offline_render <입력.wav> <출력.wav> [--dynamic-range <dB>] [--seed <n>] [--command "<콘솔 명령>"]...
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "render_pipeline.h"
#include "wav_file.h"

namespace {

constexpr double kFloorWindowSeconds = 0.5;

int printUsage() {
    std::fprintf(stderr, "usage: offline_render input.wav output.wav [--dynamic-range dB] [--seed n] "
                         "[--command \"console command\"]...\n");
    return 2;
}

bool readFile(const char* path, std::vector<std::uint8_t>& bytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool writeFile(const char* path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

}  // namespace

int main(int argumentCount, char** arguments) {
    if (argumentCount < 3) {
        return printUsage();
    }
    const char* inputPath = arguments[1];
    const char* outputPath = arguments[2];
    RenderRequest request;
    for (int index = 3; index < argumentCount; ++index) {
        const bool hasValue = index + 1 < argumentCount;
        if (std::strcmp(arguments[index], "--dynamic-range") == 0 && hasValue) {
            request.dynamicRangeDb = std::strtod(arguments[++index], nullptr);
        } else if (std::strcmp(arguments[index], "--seed") == 0 && hasValue) {
            request.seed = static_cast<std::uint32_t>(std::strtoul(arguments[++index], nullptr, 10));
        } else if (std::strcmp(arguments[index], "--command") == 0 && hasValue) {
            request.commands.emplace_back(arguments[++index]);
        } else {
            return printUsage();
        }
    }

    std::vector<std::uint8_t> bytes;
    if (!readFile(inputPath, bytes)) {
        std::fprintf(stderr, "cannot read %s\n", inputPath);
        return 1;
    }
    std::string error;
    const auto recording = decodeWav(bytes, error);
    if (!recording) {
        std::fprintf(stderr, "%s: %s\n", inputPath, error.c_str());
        return 1;
    }
    const auto rendered = renderRecording(*recording, request, error);
    if (!rendered) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!writeFile(outputPath, encodeFloatWav(*rendered))) {
        std::fprintf(stderr, "cannot write %s\n", outputPath);
        return 1;
    }

    const std::size_t floorWindow =
        static_cast<std::size_t>(kFloorWindowSeconds * recording->sampleRateHz);
    float peak = 0.0f;
    for (const float sample : rendered->samples) {
        peak = std::max(peak, std::fabs(sample));
    }
    std::printf("recordingFloorDynamicRangeDb=%.1f outputPeakDbfs=%.2f seconds=%.2f\n",
                recordingFloorDynamicRangeDb(recording->samples, floorWindow),
                20.0 * std::log10(std::max(peak, 1.0e-9f)),
                static_cast<double>(rendered->samples.size()) / rendered->sampleRateHz);
    return 0;
}
