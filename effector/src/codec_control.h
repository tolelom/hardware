#pragma once

#include <Audio.h>

// SGTL5000 레지스터 쓰기(modify)가 protected라 상속해서 쓴다.
class EffectorCodec : public AudioControlSGTL5000 {
public:
    // CHIP_ANA_CTRL(0x0024) bit 1 EN_ZCD_ADC. 라이브러리 enable()이 켜 두므로 끈다(설계서 6.2절)
    void disableAdcZeroCross() { modify(kChipAnalogControl, 0, kAdcZeroCrossBit); }

private:
    static constexpr unsigned int kChipAnalogControl = 0x0024;
    static constexpr unsigned int kAdcZeroCrossBit = 1u << 1;
};
