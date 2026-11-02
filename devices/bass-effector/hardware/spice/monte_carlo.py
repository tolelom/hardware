"""몬테카를로. 공차와 어댑터 전압 범위에서 레일, 패드 비율, 출력 스윙, Vbias 비의 분포를 본다.
설계 검증 도구 설계서 3.4절.

    python3 devices/bass-effector/hardware/spice/monte_carlo.py [--count 200] [--seed 1]
"""
from __future__ import annotations

import argparse
import math
import random
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import gen_spice  # noqa: E402
import ngspice_run  # noqa: E402
from gen_schematic import BLOCKS  # noqa: E402

THERMAL_VOLTAGE = 0.025852
SCHOTTKY_N = 1.05
FILM_OR_CERAMIC = 0.10
# 동작 모델 온저항. 10mA 손실이 데이터시트 인버터 구성 값(통상 0.35V, 최대 0.55V)이 되는 범위.
# 데이터시트는 다른 구성에서 손실이 더 클 수 있다고 적는다. 기존 모델 25옴(10mA 손실 약 1.05V)은 보수 시나리오
LT1054_RON_DATASHEET = (7.9, 12.7)
LT1054_RON_CONSERVATIVE = (25.0, 25.0)
JUDGEMENTS = [("rail_min", "아날로그 레일 최솟값 V", lambda v: v >= 16.1, "16.1 이상"),
              ("pad_db", "패드 비율 dB", lambda v: abs(v + 15) <= 1.5, "-15 ±1.5"),
              ("out_dbu", "출력단 0dBFS 스윙 dBu", lambda v: v >= 16.5, "+16.5 이상"),
              ("vbias_ratio", "Vbias / 레일", lambda v: abs(v - 0.5) <= 0.01, "0.5 ±2%")]


def _tolerances() -> dict[str, float]:
    table = {}
    for block in BLOCKS:
        for p in block.parts:
            if (p.lib, p.name) == ("Device", "R"):
                table[p.ref] = 0.05 if p.ref == "R32" else 0.01
            elif (p.lib, p.name) == ("Device", "C"):
                table[p.ref] = 0.05 if p.ref in ("C23", "C24") else FILM_OR_CERAMIC
            elif (p.lib, p.name) == ("Device", "C_Polarized"):
                table[p.ref] = 0.20
    return table


TOLERANCES = _tolerances()


@dataclass(frozen=True)
class Sample:
    adapter_v: float
    schottky_vf: float
    lt1054_ron: float
    codec_ohm: float
    scales: dict[str, float]


def sample_scales(rng: random.Random, tolerances: dict[str, float]) -> dict[str, float]:
    return {ref: 1 + rng.uniform(-width, width) for ref, width in sorted(tolerances.items())}


def draw(rng: random.Random, zero: bool = False, ron_range: tuple[float, float] = LT1054_RON_DATASHEET) -> Sample:
    if zero:
        return Sample(9.0, 0.3125, 25.0, 5000.0, {ref: 1.0 for ref in TOLERANCES})
    scales = sample_scales(rng, TOLERANCES)
    return Sample(rng.uniform(8.7, 9.6), rng.uniform(0.25, 0.45), rng.uniform(*ron_range), rng.uniform(4000.0, 6000.0),
                  scales)


def schottky_is(vf_at_1ma: float) -> float:
    return 1e-3 / math.exp(vf_at_1ma / (SCHOTTKY_N * THERMAL_VOLTAGE))


def _write(workdir: Path, sample: Sample) -> None:
    generated = workdir / "generated"
    gen_spice.generate(generated, sample.scales)
    models = (HERE / "models.inc").read_text(encoding="utf-8")
    models = re.sub(r"^\.model D1N5819 .*$", f".model D1N5819 D (IS={schottky_is(sample.schottky_vf):.4g} N={SCHOTTKY_N} RS=0.05)",
                    models, flags=re.MULTILINE)
    (workdir / "models.inc").write_text(models, encoding="utf-8")
    for name in ("lt1054_behavioral.inc", "opamp_rr.inc"):
        (workdir / name).write_text((HERE / name).read_text(encoding="utf-8"), encoding="utf-8")


def simulate(sample: Sample, workdir: Path) -> dict[str, float]:
    _write(workdir, sample)
    rail = ngspice_run.run_text("\n".join([
        "* MC 레일", ".include models.inc", ".include lt1054_behavioral.inc",
        ".include generated/block_A.cir", ".include generated/block_B.cir",
        f"Vin vin_raw 0 {sample.adapter_v:.4f}", "Rv9 v9 0 40", "Rload vrail 0 2800",
        f".param ron_value={sample.lt1054_ron:.3f}",
        ".tran 1u 150m", ".control", "run",
        "meas tran rail_min MIN v(vrail) from=100m to=150m", ".endc", ".end"]) + "\n",
        workdir, ["rail_min"])
    bias = ngspice_run.run_text("\n".join([
        "* MC Vbias 비. C6 시정수 0.5s라 과도 해석 대신 DC 동작점", ".include generated/vbias.cir",
        "Vrail vrail 0 1", ".op", ".control", "run", "print v(vbias)", ".endc", ".end"]) + "\n",
        workdir, ["v(vbias)"])
    pad = ngspice_run.run_text("\n".join([
        "* MC 패드", ".include opamp_rr.inc", ".include generated/vbias.cir", ".include generated/block_D.cir",
        f"Vrail vrail 0 {rail['rail_min']:.4f}", "Vin in_a 0 DC 0 AC 1",
        f"Rcodec seed_audio_in1 0 {sample.codec_ohm:.1f}", ".ac lin 1 1k 1k", ".control", "run",
        "let pad_db = vdb(seed_audio_in1)", "print pad_db", ".endc", ".end"]) + "\n", workdir, ["pad_db"])
    swing = ngspice_run.run_text("\n".join([
        "* MC 출력 스윙", ".include opamp_rr.inc", ".include generated/vbias.cir", ".include generated/block_E.cir",
        f"Vrail vrail 0 {rail['rail_min']:.4f}", "Vseed seed_audio_out1 0 SIN(1.65 1.4142 1k)", "Rjack out_c 0 10k",
        f".ic v(vbias)={rail['rail_min'] / 2:.4f} v(fb_c)={rail['rail_min'] / 2:.4f} "
        f"v(out_r)={rail['rail_min'] / 2:.4f} v(out_in)={rail['rail_min'] / 2:.4f}",
        ".tran 5u 400m", ".control", "run", "meas tran out_pp PP v(out_c) from=380m to=400m", ".endc", ".end"]) + "\n",
        workdir, ["out_pp"])
    return {"rail_min": rail["rail_min"], "vbias_ratio": bias["v(vbias)"], "pad_db": pad["pad_db"],
            "out_dbu": 20 * math.log10(swing["out_pp"] / (2 * math.sqrt(2)) / 0.7746)}


def run(count: int, seed: int, ron_range: tuple[float, float] = LT1054_RON_DATASHEET) -> list[dict[str, float]]:
    rng = random.Random(seed)
    results = []
    for _ in range(count):
        sample = draw(rng, ron_range=ron_range)
        with tempfile.TemporaryDirectory(dir=HERE) as directory:
            results.append(simulate(sample, Path(directory)))
    return results


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * (len(ordered) - 1) + 0.5))]


def summarize(results: list[dict[str, float]]) -> list[str]:
    lines = ["| 값 | 최솟값 | 1% | 50% | 99% | 최댓값 | 기준 | 미달 |", "|---|---|---|---|---|---|---|---|"]
    for key, label, passes, criterion in JUDGEMENTS:
        values = [r[key] for r in results]
        failures = sum(1 for v in values if not passes(v))
        lines.append(f"| {label} | {min(values):.3f} | {percentile(values, 0.01):.3f} | {percentile(values, 0.5):.3f} | "
                     f"{percentile(values, 0.99):.3f} | {max(values):.3f} | {criterion} | {failures}/{len(values)} |")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=200)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--conservative", action="store_true", help="LT1054 온저항을 기존 모델 25옴으로 고정")
    args = parser.parse_args()
    ron_range = LT1054_RON_CONSERVATIVE if args.conservative else LT1054_RON_DATASHEET
    print("\n".join(summarize(run(args.count, args.seed, ron_range))))


if __name__ == "__main__":
    main()
