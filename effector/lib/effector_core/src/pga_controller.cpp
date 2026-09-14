#include "pga_controller.h"

#include <algorithm>
#include <cmath>

PgaController::PgaController(const Config& config)
    : config_(config),
      ceilingStep_(config.maxStep),
      stepHistory_(static_cast<std::size_t>(config.alignmentBlocks) + 1, 0) {}

void PgaController::processBlock(float desiredTotalGainDb, float blockPeakDbfs) {
    const int desiredStep = std::clamp(
        static_cast<int>(std::floor(desiredTotalGainDb / kStepDb + 1.0e-4f)), 0, config_.maxStep);
    if (desiredStep != lastDesiredStep_) {
        ceilingStep_ = config_.maxStep;
        lastDesiredStep_ = desiredStep;
    }

    if (blockPeakDbfs >= config_.clipLevelDbfs && step_ > 0) {
        setStep(step_ - 1);
        ceilingStep_ = step_;
        quietBlocks_ = 0;
    } else if (blockPeakDbfs < config_.quietLevelDbfs) {
        ++quietBlocks_;
        const int targetStep = std::min(desiredStep, ceilingStep_);
        if (quietBlocks_ >= config_.quietHoldBlocks && targetStep != step_) {
            setStep(targetStep);
        }
    } else {
        quietBlocks_ = 0;
    }

    historyIndex_ = (historyIndex_ + 1) % stepHistory_.size();
    stepHistory_[historyIndex_] = step_;
}

bool PgaController::takeStepChange() {
    const bool changed = stepChanged_;
    stepChanged_ = false;
    return changed;
}

float PgaController::compensationDb() const {
    // historyIndex_ 다음 칸이 가장 오래된 값, 즉 alignmentBlocks 블록 전 단계다
    const std::size_t oldestIndex = (historyIndex_ + 1) % stepHistory_.size();
    return -kStepDb * static_cast<float>(stepHistory_[oldestIndex]);
}

void PgaController::setStep(int step) {
    if (step != step_) {
        step_ = step;
        stepChanged_ = true;
    }
}
