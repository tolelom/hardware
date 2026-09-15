#pragma once

#include <cstddef>

#include "effector_processor.h"

// 콘솔이 플랫폼에 요구하는 것. 펌웨어는 USB CDC와 인터럽트 보호로, 테스트는 기록으로 구현한다.
class ConsoleHost {
public:
    virtual ~ConsoleHost() = default;
    virtual void write(const char* text) = 0;
    virtual void applyParameters(const EffectorParameters& parameters) = 0;
    virtual void setTestTone(float frequencyHz, float amplitude) = 0;
    virtual void resetToBootloader() = 0;
};

// 1단계에는 노브가 없어 시리얼 명령으로 파라미터를 넣는다. 줄 단위로 해석한다.
class SerialConsole {
public:
    SerialConsole(EffectorProcessor& processor, EffectorParameters& parameters, ConsoleHost& host,
                  float sampleRateHz);
    void feed(const char* bytes, std::size_t count);

private:
    void handleLine(const char* line);
    void printStatus();

    EffectorProcessor& processor_;
    EffectorParameters& parameters_;
    ConsoleHost& host_;
    float sampleRateHz_;
    char buffer_[64];
    std::size_t bufferLength_ = 0;
};
