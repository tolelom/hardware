#include "serial_console.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "parameter_mapping.h"

namespace {
bool isSpace(char character) {
    return character == ' ' || character == '\t';
}

const char* skipSpaces(const char* text) {
    while (isSpace(*text)) {
        ++text;
    }
    return text;
}

constexpr float kMaximumInputDriveDb = 30.0f;  // T16 청음으로 조정
constexpr float kMinimumSidechainHighPassHz = 0.0f;  // 0 = 끔
constexpr float kMaximumSidechainHighPassFraction = 0.45f;  // 나이퀴스트보다 조금 아래
}  // namespace

SerialConsole::SerialConsole(EffectorProcessor& processor, EffectorParameters& parameters,
                             ConsoleHost& host, float sampleRateHz)
    : processor_(processor), parameters_(parameters), host_(host), sampleRateHz_(sampleRateHz) {}

void SerialConsole::feed(const char* bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const char character = bytes[index];
        if (character == '\n' || character == '\r') {
            if (bufferLength_ > 0) {
                buffer_[bufferLength_] = '\0';
                handleLine(buffer_);
                bufferLength_ = 0;
            }
        } else if (bufferLength_ < sizeof(buffer_) - 1) {
            buffer_[bufferLength_++] = character;
        }
    }
}

void SerialConsole::handleLine(const char* line) {
    using namespace parameter_mapping;
    // sscanf 대신 strtof로 읽는다. newlib-nano에서 %f를 쓰려면 _scanf_float가 FLASH를 약 9KB 먹는다
    char command[16] = {0};
    const char* cursor = skipSpaces(line);
    std::size_t commandLength = 0;
    while (cursor[commandLength] != '\0' && !isSpace(cursor[commandLength]) &&
           commandLength < sizeof(command) - 1) {
        command[commandLength] = cursor[commandLength];
        ++commandLength;
    }
    if (commandLength == 0) {
        return;
    }
    cursor += commandLength;
    int fields = 1;
    float numbers[2] = {0.0f, 0.0f};
    for (float& number : numbers) {
        char* end = nullptr;
        const float value = std::strtof(cursor, &end);
        if (end == cursor) {
            break;
        }
        number = value;
        cursor = end;
        ++fields;
    }
    const float first = numbers[0];
    const float second = numbers[1];
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
        const float maximumSidechainHighPassHz = kMaximumSidechainHighPassFraction * sampleRateHz_;
        parameters_.compressor.sidechainHighPassHz =
            std::clamp(first, kMinimumSidechainHighPassHz, maximumSidechainHighPassHz);
    } else if (std::strcmp(command, "trim") == 0 && fields >= 2) {
        parameters_.unityTrimDb = first;
    } else if (std::strcmp(command, "effect") == 0 && fields >= 2) {
        parameters_.effectEnabled = first > 0.5f;
    } else if (std::strcmp(command, "tone") == 0) {
        host_.setTestTone(first, fields >= 3 ? decibelsToGain(second) : 0.0f);
    } else if (std::strcmp(command, "dfu") == 0) {
        host_.write("entering DFU bootloader\r\n");
        host_.resetToBootloader();
        return;
    } else if (std::strcmp(command, "status") == 0) {
        printStatus();
        return;
    } else {
        host_.write("commands: input|ratio|attrel|blend|output <0..1>, threshold <dBFS>, "
                    "sidechain <Hz>, trim <dB>, effect <0|1>, tone <Hz> <dBFS>, tone 0, status, dfu\r\n");
        return;
    }
    host_.applyParameters(parameters_);
    printStatus();
}

void SerialConsole::printStatus() {
    char message[160];
    std::snprintf(message, sizeof message,
                  "gainDb=%.2f peakDbfs=%.2f meter=0x%02X drive=%.1f ratio=%.2f\r\n",
                  processor_.currentGainDb(), processor_.lastBlockPeakDbfs(),
                  static_cast<unsigned>(processor_.meterBits()), parameters_.inputDriveDb,
                  parameters_.compressor.ratio);
    host_.write(message);
}
