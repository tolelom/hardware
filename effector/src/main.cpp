#include <Audio.h>
#include <OpenAudio_ArduinoLibrary.h>

#include "codec_control.h"
#include "effector_processor.h"
#include "serial_console.h"

namespace {
constexpr int kBlockSamples = 32;
}  // namespace

AudioSettings_F32 audioSettings(AUDIO_SAMPLE_RATE_EXACT, kBlockSamples);
AudioInputI2S_F32 audioInput(audioSettings);
EffectorProcessor processor(audioSettings);
AudioOutputI2S_F32 audioOutput(audioSettings);
AudioConnection_F32 inputToProcessor(audioInput, 0, processor, 0);
AudioConnection_F32 processorToLeft(processor, 0, audioOutput, 0);
AudioConnection_F32 processorToRight(processor, 0, audioOutput, 1);

EffectorCodec codec;
EffectorParameters parameters;
SerialConsole console(processor, parameters);

void setup() {
    Serial.begin(115200);
    AudioMemory(8);
    AudioMemory_F32(24, audioSettings);

    codec.enable();
    codec.inputSelect(AUDIO_INPUT_LINEIN);
    codec.lineInLevel(0);
    codec.lineOutLevel(17);  // 2.53Vpp, 설계서 4.2절
    codec.adcHighPassFilterDisable();
    codec.disableAdcZeroCross();

    AudioNoInterrupts();
    processor.setParameters(parameters);
    AudioInterrupts();
    Serial.println("effector stage 1 ready. type status");
}

void loop() {
    console.poll();
    int step = 0;
    AudioNoInterrupts();
    const bool changed = processor.takePgaStepChange(step);
    AudioInterrupts();
    if (changed) {
        codec.lineInLevel(static_cast<uint8_t>(step));
    }
}
