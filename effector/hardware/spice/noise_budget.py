"""잡음 예산. 입력 경로와 출력 경로의 아날로그 잡음을 ngspice .noise로 구해 코덱 바닥과 합친다.
설계 검증 도구 설계서 4절. 결과는 회로 설계 12절.

    python3 effector/hardware/spice/noise_budget.py
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ngspice_run  # noqa: E402

SPICE_DIR = Path(__file__).resolve().parent
CODEC_FULL_SCALE_DBU = 20 * math.log10(1.0 / 0.7746)
CODEC_ROWS = [("칩 값", 119.0, 120.0), ("칩 값 -10dB", 109.0, 110.0)]
EN_VALUES = [("7n", "7nV 보수"), ("4.4n", "4.4nV 광대역")]
# 패시브 픽업 가정값. 사용자 베이스 실측 전이다(설계 검증 도구 설계서 4.2절)
PICKUP = {"r": "7k", "l": "3", "c": "600p", "load": "125k"}
Weight = Callable[[float], float]


def a_weight(frequency: float) -> float:
    f2 = frequency * frequency
    ra = (12194.0 ** 2 * f2 * f2) / ((f2 + 20.6 ** 2) * math.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2))
                                     * (f2 + 12194.0 ** 2))
    return ra * ra * 10 ** (2.0 / 10)


def band(lo: float, hi: float) -> Weight:
    return lambda frequency: 1.0 if lo <= frequency <= hi else 0.0


WEIGHTINGS: dict[str, Weight] = {"A": a_weight, "125-8000": band(125, 8000), "20-20k": band(20, 20000)}


def white_power(weight: Weight, top: float = 24000.0) -> float:
    return sum(weight(f + 0.5) for f in range(int(top)))


def white_ratio(weight: Weight, reference: Weight) -> float:
    return math.sqrt(white_power(weight) / white_power(reference))


def integrate(frequencies: list[float], density: list[float], weight: Weight, steps: int = 64) -> float:
    total = 0.0
    for i in range(len(frequencies) - 1):
        f0, f1 = frequencies[i], frequencies[i + 1]
        d0, d1 = math.log(density[i]), math.log(density[i + 1])
        for k in range(steps):
            t = (k + 0.5) / steps
            f = f0 * (f1 / f0) ** t
            d = math.exp(d0 + (d1 - d0) * t)
            total += d * d * weight(f) * f * math.log(f1 / f0) / steps
    return math.sqrt(total)


def dbu(volts: float) -> float:
    return 20 * math.log10(volts / 0.7746)


def power_sum_db(*levels: float) -> float:
    return 10 * math.log10(sum(10 ** (level / 10) for level in levels))


def t7_zone(dynamic_range_db: float) -> str:
    if dynamic_range_db >= 105.0:
        return "통과"
    if dynamic_range_db >= 98.0:
        return "회색"
    return "미달"


@dataclass
class Spectrum:
    frequencies: list[float]
    vectors: dict[str, list[float]]


def read_spectrum(path: Path) -> Spectrum:
    lines = [line.split() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    names = lines[0][1:]
    rows = [[float(value) for value in line] for line in lines[1:]]
    return Spectrum([row[0] for row in rows], {name: [row[i + 1] for row in rows] for i, name in enumerate(names)})


def noise_text(body: list[str], output: str, source: str, data_file: str, en: str) -> str:
    return "\n".join([
        "* 잡음 예산 테스트벤치 (noise_budget.py 생성)",
        ".include opamp_rr_noise.inc", ".include generated/vbias.cir", *body,
        f".param en_value={en}",
        f".noise v({output}) {source} dec 50 20 24k 1",
        ".control", "set wr_vecnames", "set wr_singlescale", "run", "setplot noise1",
        f"wrdata {data_file} all", ".endc", ".end"]) + "\n"


def input_body(condition: str) -> list[str]:
    lines = [".include generated/block_D.cir", "Vrail vrail 0 16.9", "Rcodec seed_audio_in1 0 5k noisy=0"]
    if condition == "short":
        return lines + ["Vin in_a 0 DC 0 AC 1"]
    return lines + ["Vin src 0 DC 0 AC 1", f"Rpick src p1 {PICKUP['r']}", f"Lpick p1 in_a {PICKUP['l']}",
                    f"Cpick in_a 0 {PICKUP['c']}", f"Rload in_a 0 {PICKUP['load']}"]


def output_body(source_ohm: float) -> list[str]:
    return [".include generated/block_E.cir", "Vrail vrail 0 16.9", "Vdac dac 0 DC 1.65 AC 1",
            f"Rdac dac seed_audio_out1 {max(source_ohm, 1e-3):g} noisy=0", "Rjack out_c 0 10k noisy=0"]


def run_noise(body: list[str], output: str, source: str, en: str, tag: str) -> Spectrum:
    data_file = f"_noise_{tag}.txt"
    ngspice_run.run_text(noise_text(body, output, source, data_file, en))
    path = SPICE_DIR / data_file
    try:
        return read_spectrum(path)
    finally:
        path.unlink()


def gain_at_1k(spectrum: Spectrum) -> float:
    index = min(range(len(spectrum.frequencies)), key=lambda i: abs(spectrum.frequencies[i] - 1000))
    return spectrum.vectors["onoise_spectrum"][index] / spectrum.vectors["inoise_spectrum"][index]


def input_referred(spectrum: Spectrum, weight: Weight) -> float:
    """입력 잭 기준 잡음(Vrms). 풀스케일과 같은 1kHz 게인으로 나눈다. inoise_spectrum은 주파수별 게인으로 나눠
    픽업 L·C처럼 고역 게인이 떨어지는 소스에서 고역 잡음을 부풀린다."""
    return integrate(spectrum.frequencies, spectrum.vectors["onoise_spectrum"], weight) / gain_at_1k(spectrum)


def contributions(spectrum: Spectrum, weight: Weight, count: int = 5) -> list[tuple[str, float]]:
    rows = []
    for name, values in spectrum.vectors.items():
        if name.startswith("onoise_") and name != "onoise_spectrum" and not name.endswith(("_1overf", "_thermal")):
            if max(values) > 0:
                rows.append((name.removeprefix("onoise_"), dbu(integrate(spectrum.frequencies, values, weight))))
    return sorted(rows, key=lambda row: -row[1])[:count]


def budget_rows() -> tuple[list[str], dict[str, list[tuple[str, float]]]]:
    a_to = {key: white_ratio(weight, WEIGHTINGS["A"]) for key, weight in WEIGHTINGS.items()}
    lines = ["| 입력 조건 | en | 출력 소스 | 코덱 | 입력 쪽 dBu(A) | 출력 쪽 dBu(A) | 합산 dBu(A) | DR dB(A) | DR 125~8000 dB | T7 구간 | 지배 쪽 |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    top: dict[str, list[tuple[str, float]]] = {}
    for en, en_label in EN_VALUES:
        inputs = {c: run_noise(input_body(c), "seed_audio_in1", "Vin", en, f"in_{c}") for c in ("short", "bass")}
        outputs = {r: run_noise(output_body(r), "out_c", "Vdac", en, f"out_{r:g}") for r in (0.0, 100.0)}
        if en == "7n":
            top["입력 잭 단락"] = contributions(inputs["short"], WEIGHTINGS["A"])
            top["출력 소스 0Ω"] = contributions(outputs[0.0], WEIGHTINGS["A"])
        for condition, spectrum_in in inputs.items():
            full_scale_in = CODEC_FULL_SCALE_DBU - 20 * math.log10(gain_at_1k(spectrum_in))
            for source_ohm, spectrum_out in outputs.items():
                full_scale_out = CODEC_FULL_SCALE_DBU + 20 * math.log10(gain_at_1k(spectrum_out))
                for codec_label, adc_db, dac_db in CODEC_ROWS:
                    levels = {}
                    for key, weight in WEIGHTINGS.items():
                        analog_in = dbu(input_referred(spectrum_in, weight))
                        analog_out = dbu(integrate(spectrum_out.frequencies, spectrum_out.vectors["onoise_spectrum"], weight))
                        scale_db = 20 * math.log10(a_to[key])
                        side_in = power_sum_db(analog_in, full_scale_in - adc_db + scale_db)
                        side_out = power_sum_db(analog_out, full_scale_out - dac_db + scale_db)
                        levels[key] = (side_in, side_out, power_sum_db(side_in, side_out))
                    side_in, side_out, total = levels["A"]
                    dr_a = full_scale_in - total
                    dr_band = full_scale_in - levels["125-8000"][2]
                    dominant = "출력" if side_out > side_in else "입력"
                    label = "잭 단락" if condition == "short" else "베이스(가정값)"
                    lines.append(f"| {label} | {en_label} | {source_ohm:g}Ω | {codec_label} | {side_in:.1f} | "
                                 f"{side_out:.1f} | {total:.1f} | {dr_a:.1f} | {dr_band:.1f} | {t7_zone(dr_a)} | {dominant} |")
    return lines, top


def main() -> None:
    lines, top = budget_rows()
    print("\n".join(lines))
    for path_label, rows in top.items():
        print(f"\n{path_label} 부품별 기여 상위 (A 가중, en 7nV, 출력 노드 기준)\n")
        print("| 소자 | dBu(A) |\n|---|---|")
        for name, level in rows:
            print(f"| {name} | {level:.1f} |")


if __name__ == "__main__":
    main()
