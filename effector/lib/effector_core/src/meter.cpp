#include "meter.h"

Meter::Meter(const Config& config) : config_(config) {}

std::uint8_t Meter::processBlock(float gainReductionDb, float blockPeakDbfs) {
    if (blockPeakDbfs >= config_.clipLevelDbfs) {
        if (clipBlocksRemaining_ == 0) {
            clipElapsedBlocks_ = 0;
        }
        clipBlocksRemaining_ = config_.clipHoldBlocks;
    }
    if (clipBlocksRemaining_ > 0) {
        const bool lit = (clipElapsedBlocks_ / config_.blinkHalfPeriodBlocks) % 2 == 0;
        ++clipElapsedBlocks_;
        --clipBlocksRemaining_;
        return lit ? 0xFF : 0x00;
    }
    std::uint8_t bits = 0;
    for (std::size_t led = 0; led < config_.thresholdsDb.size(); ++led) {
        if (gainReductionDb >= config_.thresholdsDb[led]) {
            bits |= static_cast<std::uint8_t>(1u << led);
        }
    }
    return bits;
}
