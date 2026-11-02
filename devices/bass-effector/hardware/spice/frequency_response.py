"""아날로그 체인 저역·고역 응답 합산. 회로 설계 12절의 표를 만든다.

1차 구간의 곱이라 SPICE 없이 계산한다. 코덱 ADC HPF는 핀 설정이 미공개라 1Hz와 12Hz 두 경우를 본다.
    python frequency_response.py
"""
import math


def high_pass(frequency_hz: float, cutoff_hz: float) -> complex:
    s = 1j * frequency_hz / cutoff_hz
    return s / (1 + s)


def low_pass(frequency_hz: float, cutoff_hz: float) -> complex:
    return 1 / (1 + 1j * frequency_hz / cutoff_hz)


def parallel(a: float, b: float) -> float:
    return a * b / (a + b)


def feedback_shelf(frequency_hz: float) -> complex:
    """출력단 비반전. R7 47k, R8 10k, C4 10uF. 게인 = 1 + (R7/R8) * jwt/(1+jwt), 5.7로 정규화."""
    tau = 10e3 * 10e-6
    z = 1j * 2 * math.pi * frequency_hz * tau
    return (1 + 4.7 * z / (1 + z)) / 5.7


CODEC_INPUT_OHM = 5000.0
PAD_LOAD_OHM = 3300 + parallel(820, CODEC_INPUT_OHM)
PAD_OUTPUT_OHM = parallel(3300, parallel(820, CODEC_INPUT_OHM))

STAGES = {
    "C1 R2+R3": lambda f: high_pass(f, 1 / (2 * math.pi * 1.81e6 * 100e-9)),
    "C2 패드": lambda f: high_pass(f, 1 / (2 * math.pi * PAD_LOAD_OHM * 47e-6)),
    "C23 종단": lambda f: low_pass(f, 1 / (2 * math.pi * PAD_OUTPUT_OHM * 4.7e-9)),
    "DSP DC 5Hz": lambda f: high_pass(f, 5.0),
    "C3 R6": lambda f: high_pass(f, 1 / (2 * math.pi * 100e3 * 1e-6)),
    "C4 셸프": feedback_shelf,
    "C5 부하": lambda f: high_pass(f, 1 / (2 * math.pi * parallel(100e3, 10e3) * 10e-6)),
}


def decibels(value: complex) -> float:
    return 20 * math.log10(abs(value))


def main() -> None:
    for codec_hpf_hz in (1.0, 12.0):
        print(f"코덱 HPF {codec_hpf_hz:g}Hz")
        print("f(Hz) " + " ".join(f"{name:>10}" for name in STAGES) + "   코덱 HPF    합계 dB")
        for frequency_hz in (20, 31, 41, 100, 1000, 10000, 20000):
            parts = [decibels(stage(frequency_hz)) for stage in STAGES.values()]
            codec = decibels(high_pass(frequency_hz, codec_hpf_hz))
            print(f"{frequency_hz:5d} " + " ".join(f"{part:10.2f}" for part in parts)
                  + f"   {codec:8.2f}   {sum(parts) + codec:8.2f}")


if __name__ == "__main__":
    main()
