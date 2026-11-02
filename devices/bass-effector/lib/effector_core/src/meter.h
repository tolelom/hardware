#pragma once

#include <array>
#include <cstdint>

// 감쇠량 LED 8칸과 입력 클립 점멸. 반환값의 비트 i가 LED i다.
class Meter {
public:
    struct Config {
        std::array<float, 8> thresholdsDb;
        float clipLevelDbfs;
        int clipHoldBlocks;
        int blinkHalfPeriodBlocks;
    };

    explicit Meter(const Config& config);

    std::uint8_t processBlock(float gainReductionDb, float blockPeakDbfs);

private:
    Config config_;
    int clipBlocksRemaining_ = 0;
    int clipElapsedBlocks_ = 0;
};
