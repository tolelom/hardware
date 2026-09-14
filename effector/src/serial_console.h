#pragma once

#include <Arduino.h>

#include "effector_processor.h"

// 1단계에는 노브가 없어 시리얼 명령으로 파라미터를 넣는다.
class SerialConsole {
public:
    SerialConsole(EffectorProcessor& processor, EffectorParameters& parameters);
    void poll();

private:
    void handleLine(const char* line);
    void applyParameters();
    void printStatus();

    EffectorProcessor& processor_;
    EffectorParameters& parameters_;
    char buffer_[64];
    int bufferLength_ = 0;
};
