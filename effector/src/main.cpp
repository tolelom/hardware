// Daisy Seed3 층. 오디오 콜백이 코어를 부르고, USB CDC로 1단계 시리얼 명령을 받는다.
#include <algorithm>
#include <cstring>

#include "daisy_seed.h"
#include "effector_processor.h"
#include "serial_console.h"

using daisy::AudioHandle;
using daisy::DaisySeed;
using daisy::SaiHandle;
using daisy::System;
using daisy::UsbHandle;

namespace {

constexpr float kSampleRateHz = 48000.0f;
constexpr int kBlockSamples = 48;
constexpr std::size_t kReceiveBufferSize = 256;

DaisySeed hardware;
EffectorProcessor processor({kSampleRateHz, kBlockSamples});
EffectorParameters parameters;

// USB 수신 인터럽트가 쓰고 메인 루프가 읽는 링 버퍼. 생산자·소비자가 하나씩이라 잠금이 없다.
volatile char receiveBuffer[kReceiveBufferSize];
volatile std::size_t receiveWriteIndex = 0;
volatile std::size_t receiveReadIndex = 0;

void onUsbReceive(uint8_t* buffer, uint32_t* length) {
    for (uint32_t index = 0; index < *length; ++index) {
        const std::size_t next = (receiveWriteIndex + 1) % kReceiveBufferSize;
        if (next == receiveReadIndex) {
            return;
        }
        receiveBuffer[receiveWriteIndex] = static_cast<char>(buffer[index]);
        receiveWriteIndex = next;
    }
}

class DaisyConsoleHost : public ConsoleHost {
public:
    void write(const char* text) override {
        hardware.usb_handle.TransmitInternal(reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
                                             std::strlen(text));
    }
    void applyParameters(const EffectorParameters& next) override {
        __disable_irq();
        processor.setParameters(next);
        __enable_irq();
    }
    void setTestTone(float frequencyHz, float amplitude) override {
        __disable_irq();
        processor.setTestTone(frequencyHz, amplitude);
        __enable_irq();
    }
    void resetToBootloader() override {
        System::Delay(50);
        System::ResetToBootloader();
    }
};

DaisyConsoleHost host;
SerialConsole console(processor, parameters, host, kSampleRateHz);

void audioCallback(AudioHandle::InputBuffer input, AudioHandle::OutputBuffer output, size_t size) {
    processor.process(input[0], output[0], size);
    std::copy(output[0], output[0] + size, output[1]);
}

}  // namespace

int main() {
    hardware.Init();
    hardware.SetAudioBlockSize(kBlockSamples);
    hardware.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hardware.usb_handle.Init(UsbHandle::FS_INTERNAL);
    hardware.usb_handle.SetReceiveCallback(onUsbReceive, UsbHandle::FS_INTERNAL);

    processor.setParameters(parameters);
    hardware.StartAudio(audioCallback);
    System::Delay(500);
    host.write("effector stage 1 ready. type status\r\n");

    while (true) {
        while (receiveReadIndex != receiveWriteIndex) {
            const char character = receiveBuffer[receiveReadIndex];
            receiveReadIndex = (receiveReadIndex + 1) % kReceiveBufferSize;
            console.feed(&character, 1);
        }
        System::Delay(1);
    }
}
