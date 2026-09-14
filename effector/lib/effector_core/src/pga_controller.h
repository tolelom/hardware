#pragma once

#include <vector>

// 코덱 아날로그 입력 게인(PGA) 단계를 정한다.
// - 올리기·내리기 모두 입력이 조용한 구간에서만 원하는 단계로 옮긴다
// - ADC 클립이면 즉시 한 단계 내리고, 노브가 움직이기 전까지 그 단계를 상한으로 둔다
// - 보상 게인은 alignmentBlocks 블록 전의 단계로 계산해 아날로그 변경이 ADC 출력에
//   나타나는 시점과 맞춘다
class PgaController {
public:
    static constexpr float kStepDb = 1.5f;

    struct Config {
        int maxStep;
        float quietLevelDbfs;
        int quietHoldBlocks;
        float clipLevelDbfs;
        int alignmentBlocks;
    };

    explicit PgaController(const Config& config);

    void processBlock(float desiredTotalGainDb, float blockPeakDbfs);
    int pgaStep() const { return step_; }
    bool takeStepChange();
    float compensationDb() const;

private:
    void setStep(int step);

    Config config_;
    int step_ = 0;
    int ceilingStep_;
    int lastDesiredStep_ = -1;
    int quietBlocks_ = 0;
    bool stepChanged_ = false;
    std::vector<int> stepHistory_;
    std::size_t historyIndex_ = 0;
};
