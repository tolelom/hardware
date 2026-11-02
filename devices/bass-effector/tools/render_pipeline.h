#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "wav_file.h"

// 스칼렛 INST 0dBFS와 페달 ADC 풀스케일(설계서 3.1절). 둘 다 풀스케일 사인 RMS의 dBu다.
constexpr float kScarlettInstrumentFullScaleDbu = 12.5f;
constexpr float kPedalInputFullScaleDbu = 17.2f;
constexpr int kRenderSampleRateHz = 48000;
constexpr int kRenderBlockSamples = 48;

float scarlettToPedalGainDb();

// T7 정의(125~8000Hz 대역, 풀스케일 사인 RMS 기준)의 다이내믹 레인지를 백색 잡음 전대역 RMS로 바꾼다.
double noiseFullBandRmsDbfs(double dynamicRangeDb);
std::vector<float> generateModuleNoise(double dynamicRangeDb, std::size_t sampleCount,
                                       std::uint32_t seed);

// 녹음에서 가장 조용한 창의 RMS를 페달 기준 다이내믹 레인지로 환산한다. 백색 잡음을 가정한다.
double recordingFloorDynamicRangeDb(const std::vector<float>& scarlettSamples,
                                    std::size_t windowSamples);

struct RenderRequest {
    std::vector<std::string> commands;
    std::optional<double> dynamicRangeDb;
    std::uint32_t seed = 1;
};

// 스칼렛 녹음을 페달 ADC 레벨로 낮추고 노이즈를 더해 프로세서에 통과시킨 뒤 스칼렛 레벨로 되돌린다.
std::optional<MonoAudio> renderRecording(const MonoAudio& recording, const RenderRequest& request,
                                         std::string& error);
