#include "serial_console.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "parameter_mapping.h"

namespace {
constexpr float kMaximumInputDriveDb = 30.0f;  // T16 청음으로 조정
constexpr int kMinimumPgaStep = -1;  // 자동
constexpr int kMaximumPgaStep = 15;  // SGTL5000 lineInLevel 범위
constexpr float kMinimumSidechainHighPassHz = 0.0f;  // 0 = 끔
constexpr float kMaximumSidechainHighPassFraction = 0.45f;  // 나이퀴스트보다 조금 아래
}  // namespace

SerialConsole::SerialConsole(EffectorProcessor& processor, EffectorParameters& parameters)
    : processor_(processor), parameters_(parameters) {}

void SerialConsole::poll() {
    while (Serial.available() > 0) {
        const int character = Serial.read();
        if (character == '\n' || character == '\r') {
            if (bufferLength_ > 0) {
                buffer_[bufferLength_] = '\0';
                handleLine(buffer_);
                bufferLength_ = 0;
            }
        } else if (bufferLength_ < static_cast<int>(sizeof(buffer_)) - 1) {
            buffer_[bufferLength_++] = static_cast<char>(character);
        }
    }
}

void SerialConsole::handleLine(const char* line) {
    using namespace parameter_mapping;
    char command[16] = {0};
    float first = 0.0f;
    float second = 0.0f;
    const int fields = std::sscanf(line, "%15s %f %f", command, &first, &second);
    if (fields < 1) {
        return;
    }
    if (std::strcmp(command, "input") == 0 && fields >= 2) {
        parameters_.inputDriveDb = inputDriveDbFromKnob(first, kMaximumInputDriveDb);
    } else if (std::strcmp(command, "ratio") == 0 && fields >= 2) {
        parameters_.compressor.ratio = ratioFromKnob(first);
    } else if (std::strcmp(command, "attrel") == 0 && fields >= 2) {
        const AttackRelease times = attackReleaseFromKnob(first);
        parameters_.compressor.attackSeconds = times.attackSeconds;
        parameters_.compressor.releaseSeconds = times.releaseSeconds;
    } else if (std::strcmp(command, "blend") == 0 && fields >= 2) {
        parameters_.blend = clampKnob(first);
    } else if (std::strcmp(command, "output") == 0 && fields >= 2) {
        parameters_.outputGainDb = outputGainDbFromKnob(first);
    } else if (std::strcmp(command, "threshold") == 0 && fields >= 2) {
        parameters_.compressor.thresholdDbfs = first;
    } else if (std::strcmp(command, "sidechain") == 0 && fields >= 2) {
        const float maximumSidechainHighPassHz =
            kMaximumSidechainHighPassFraction * AUDIO_SAMPLE_RATE_EXACT;
        parameters_.compressor.sidechainHighPassHz =
            std::clamp(first, kMinimumSidechainHighPassHz, maximumSidechainHighPassHz);
    } else if (std::strcmp(command, "trim") == 0 && fields >= 2) {
        parameters_.unityTrimDb = first;
    } else if (std::strcmp(command, "pga") == 0 && fields >= 2) {
        parameters_.manualPgaStep =
            std::clamp(static_cast<int>(first), kMinimumPgaStep, kMaximumPgaStep);
    } else if (std::strcmp(command, "effect") == 0 && fields >= 2) {
        parameters_.effectEnabled = first > 0.5f;
    } else if (std::strcmp(command, "tone") == 0) {
        AudioNoInterrupts();
        processor_.setTestTone(first, fields >= 3 ? decibelsToGain(second) : 0.0f);
        AudioInterrupts();
    } else if (std::strcmp(command, "status") == 0) {
        printStatus();
        return;
    } else {
        Serial.println("commands: input|ratio|attrel|blend|output <0..1>, threshold <dBFS>, "
                       "sidechain <Hz>, trim <dB>, pga <step|-1>, effect <0|1>, tone <Hz> <dBFS>, tone 0, status");
        return;
    }
    applyParameters();
    printStatus();
}

void SerialConsole::applyParameters() {
    AudioNoInterrupts();
    processor_.setParameters(parameters_);
    AudioInterrupts();
}

void SerialConsole::printStatus() {
    AudioNoInterrupts();
    const float gainDb = processor_.currentGainDb();
    const float peakDbfs = processor_.lastBlockPeakDbfs();
    const int pgaStep = processor_.pgaStep();
    const unsigned meterBits = processor_.meterBits();
    AudioInterrupts();
    char message[160];
    std::snprintf(message, sizeof message,
                  "gainDb=%.2f peakDbfs=%.2f pgaStep=%d meter=0x%02X drive=%.1f ratio=%.2f",
                  gainDb, peakDbfs, pgaStep, meterBits, parameters_.inputDriveDb,
                  parameters_.compressor.ratio);
    Serial.println(message);
}
