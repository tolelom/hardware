# 설계 검증 도구 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BLOCKS에서 SPICE를 생성해 기존 시뮬레이션을 하나의 기준으로 묶고, 잡음 예산·몬테카를로·기구 검사 도구를 만들어 결과를 회로 설계와 설계서에 반영한다.

**Architecture:** `gen_spice.py`가 `gen_schematic.py`의 BLOCKS를 읽어 블록별 평면 넷리스트를 `spice/generated/`에 쓴다. 테스트벤치는 생성물을 include하고 소스·해석·meas만 둔다. `ngspice_run.py`가 ngspice 배치 실행과 결과 파싱을 맡고, `noise_budget.py`와 `monte_carlo.py`가 그 위에서 돈다. 기구 검사는 `enclosure.py` 한 파일이 데이터·검사·SVG 출력을 맡는다.

**Tech Stack:** Python 3 표준 라이브러리, unittest, ngspice 47(Homebrew)

**Spec:** [bass-effector-tools-design.md](bass-effector-tools-design.md)

## Global Constraints

- 파이썬은 표준 라이브러리만 쓴다
- 테스트는 `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware`로 저장소 루트에서 돈다
- ngspice가 필요한 테스트는 ngspice가 없으면 건너뛰고 건너뛴 사실을 출력한다
- BLOCKS(회로 기준)는 이 계획에서 고치지 않는다. 결과가 변경을 요구하면 고칠 블록·부품·값과 근거를 결과 보고에 적는다
- 생성물(`spice/generated/`)은 손으로 고치지 않고 커밋한다
- 문서는 기존 문서 문체("~다", 표·짧은 목록)를 따르고 em dash를 쓰지 않는다
- 커밋 메시지는 `[Feat]`, `[Docs]`, `[Test]`, `[Refactor]` 머리말과 한국어 요약

## Review Focus

- BLOCKS를 고치고 생성기를 다시 돌리지 않음: 테스트가 생성물 머리의 해시와 현재 BLOCKS 해시를 비교해 실패해야 한다 (Task 2)
- include 경로 오류나 모델 이름 오류로 ngspice가 에러를 내고도 일부 값만 출력: 실행기가 에러 줄이나 기대한 결과 누락에서 예외를 내야 한다 (Task 2)
- 테스트벤치가 릴레이·포트 파라미터(`k1_on`, `pos_rv1`)를 정의하지 않음: ngspice 에러가 실행기 예외로 올라와야 한다 (Task 2)
- 로그 간격 주파수 점이 적을 때 잡음 적분 오차: 백색 잡음 해석해와 0.1dB 안이어야 한다 (Task 5)
- 면 크기를 벗어난 좌표나 음수 좌표의 배치: 가장자리 검사가 실패를 보고해야 한다 (Task 7)

---

### Task 1: 값 파싱과 소자 매핑

**Files:**
- Create: `effector/hardware/gen_spice.py`
- Create: `effector/hardware/tests/__init__.py` (빈 파일)
- Create: `effector/hardware/tests/test_gen_spice.py`

**Interfaces:**
- Consumes: `gen_schematic.BLOCKS: list[Block]`, `Part(ref, lib, name, value, nets, unit, note, footprint)`
- Produces:
  - `parse_value(text: str) -> float`
  - `node(net: str) -> str` (GND -> "0", 그 외 소문자)
  - `opamp_power(blocks: list[Block]) -> dict[str, tuple[str, str]]`
  - `part_lines(part: Part, power: dict[str, tuple[str, str]], scale: float = 1.0) -> list[str]`
  - `SpiceError(Exception)`
  - 생성 넷리스트의 op-amp 부분회로 이름은 `opamp_model`(핀 inp inn out vcc vee), LT1054는 `lt1054`(핀 vin capp gnd), MOSFET은 `IRF9540N`(핀 d g s)
  - 테스트벤치 파라미터: 릴레이 `<지정자 소문자>_on`(0 또는 1), 포트 `pos_<지정자 소문자>`(0~1)

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_gen_spice.py`:

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
from gen_schematic import BLOCKS, Part  # noqa: E402


def part(ref: str, lib: str, name: str, value: str, nets: dict[str, str], unit: int = 1) -> Part:
    return Part(ref, lib, name, value, nets, unit)


class ParseValueTest(unittest.TestCase):
    def test_suffixes(self) -> None:
        cases = {"4.7k": 4.7e3, "10R": 10.0, "3.3R": 3.3, "2.2M": 2.2e6, "1000u 25V": 1e-3,
                 "100n 필름": 1e-7, "4.7n": 4.7e-9, "820R": 820.0, "1.8M": 1.8e6}
        for text, expected in cases.items():
            with self.subTest(text=text):
                self.assertAlmostEqual(gen_spice.parse_value(text), expected, delta=expected * 1e-12)

    def test_unparsable(self) -> None:
        for text in ("abc", "", "4k7", "10 ohm"):
            with self.subTest(text=text):
                with self.assertRaises(gen_spice.SpiceError):
                    gen_spice.parse_value(text)


class PartLinesTest(unittest.TestCase):
    power = {"U1": ("vrail", "0")}

    def lines(self, p: Part, scale: float = 1.0) -> list[str]:
        return gen_spice.part_lines(p, self.power, scale)

    def test_resistor_and_scale(self) -> None:
        r = part("R7", "Device", "R", "47k", {"1": "U1B_OUT", "2": "FB"})
        self.assertEqual(self.lines(r), ["R7 u1b_out fb 47000"])
        self.assertEqual(self.lines(r, 1.01), ["R7 u1b_out fb 47470"])

    def test_capacitor_first_token(self) -> None:
        c = part("C7", "Device", "C_Polarized", "1000u 25V", {"1": "V9", "2": "GND"})
        self.assertEqual(self.lines(c), ["C7 v9 0 0.001"])

    def test_diode_anode_first(self) -> None:
        d = part("D1", "Device", "D_Schottky", "1N5819", {"1": "PUMPX", "2": "V9"})
        self.assertEqual(self.lines(d), ["D1 v9 pumpx D1N5819"])
        z = part("D3", "Device", "D_Zener", "1N4742A 12V", {"1": "ZK", "2": "GND"})
        self.assertEqual(self.lines(z), ["D3 0 zk DZ12"])
        led = part("D5", "Device", "LED", "3mm 미터 1", {"1": "GND", "2": "LED1_A"})
        self.assertEqual(self.lines(led), ["D5 led1_a 0 DLEDR"])

    def test_bjt_collector_base_emitter(self) -> None:
        q = part("Q3", "Transistor_BJT", "2N3906", "2N3906", {"1": "VS", "2": "OV_B", "3": "VG"})
        self.assertEqual(self.lines(q), ["Q3 vg ov_b vs Q2N3906"])

    def test_mosfet_subcircuit(self) -> None:
        m = part("Q1", "Transistor_FET", "IRF9540N", "IRF9540N", {"1": "VG", "2": "VIN_RAW", "3": "VS"})
        self.assertEqual(self.lines(m), ["XQ1 vin_raw vg vs IRF9540N"])

    def test_opamp_units(self) -> None:
        a = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"3": "IN_C", "2": "BUF_OUT", "1": "BUF_OUT"}, 1)
        b = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"5": "OUT_IN", "6": "FB", "7": "U1B_OUT"}, 2)
        p = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"8": "VRAIL", "4": "GND"}, 3)
        self.assertEqual(self.lines(a), ["XU1A in_c buf_out buf_out vrail 0 opamp_model"])
        self.assertEqual(self.lines(b), ["XU1B out_in fb u1b_out vrail 0 opamp_model"])
        self.assertEqual(self.lines(p), [])

    def test_lt1054(self) -> None:
        u = part("U2", "Regulator_SwitchedCapacitor", "LT1054", "LT1054CP",
                 {"8": "V9", "3": "GND", "5": "GND", "2": "PUMP", "1": "NC", "4": "NC", "6": "NC", "7": "NC"})
        self.assertEqual(self.lines(u), ["XU2 v9 pump 0 lt1054"])

    def test_relay(self) -> None:
        k = part("K1", "Relay", "Relay_DPDT", "TQ2-9V",
                 {"A1": "V9", "A2": "K1_COIL", "11": "IN_TIP", "12": "BYPASS", "14": "IN_A",
                  "21": "OUT_TIP", "22": "BYPASS", "24": "OUT_C"})
        self.assertEqual(self.lines(k), [
            "RK1_COIL v9 k1_coil 579",
            "RK1_11_12 in_tip bypass {k1_on > 0.5 ? 1e+12 : 0.05}",
            "RK1_11_14 in_tip in_a {k1_on > 0.5 ? 0.05 : 1e+12}",
            "RK1_21_22 out_tip bypass {k1_on > 0.5 ? 1e+12 : 0.05}",
            "RK1_21_24 out_tip out_c {k1_on > 0.5 ? 0.05 : 1e+12}",
        ])

    def test_potentiometer(self) -> None:
        rv = part("RV1", "Device", "R_Potentiometer", "B10K Input",
                  {"1": "SEED_3V3A", "2": "SEED_A0_INPUT", "3": "GND"})
        self.assertEqual(self.lines(rv), [
            "RRV1_A seed_3v3a seed_a0_input {max(1, 10000 * (1 - pos_rv1))}",
            "RRV1_B seed_a0_input 0 {max(1, 10000 * pos_rv1)}",
        ])

    def test_skipped_symbols(self) -> None:
        for lib, name in gen_spice.SKIPPED:
            with self.subTest(name=name):
                self.assertEqual(self.lines(part("J9", lib, name, "x", {"1": "A", "2": "B"})), [])

    def test_unknown_symbol_fails(self) -> None:
        with self.assertRaises(gen_spice.SpiceError):
            self.lines(part("U9", "Amplifier_Operational", "NE5532", "NE5532", {"1": "A"}))

    def test_unknown_diode_value_fails(self) -> None:
        with self.assertRaises(gen_spice.SpiceError):
            self.lines(part("D9", "Device", "D", "1N4007", {"1": "A", "2": "B"}))


class BlocksTest(unittest.TestCase):
    def test_every_blocks_part_maps(self) -> None:
        power = gen_spice.opamp_power(BLOCKS)
        self.assertEqual(power, {"U1": ("vrail", "0")})
        for block in BLOCKS:
            for p in block.parts:
                gen_spice.part_lines(p, power)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: `ModuleNotFoundError: No module named 'gen_spice'`

- [ ] **Step 3: 구현**

`effector/hardware/gen_spice.py`:

```python
"""gen_schematic.py의 BLOCKS에서 블록별 SPICE 넷리스트를 만든다.

    python3 effector/hardware/gen_spice.py

spice/generated/에 block_<문자>.cir와 부분 묶음(SUBSETS)을 쓴다. 생성물은 손으로 고치지 않는다.
설계 검증 도구 설계서 3절.
"""
from __future__ import annotations

import re

from gen_schematic import Block, Part

SKIPPED = {
    ("Connector", "Barrel_Jack"),
    ("Connector_Audio", "AudioJack2"),
    ("Connector_Generic", "Conn_01x40"),
    ("Switch", "SW_Push"),
    ("power", "PWR_FLAG"),
}
DIODE_MODELS = {"1N4148": "D1N4148", "1N5819": "D1N5819", "1N4742A": "DZ12", "1N4744A": "DZ15"}
BJT_MODELS = {"2N3904": "Q2N3904", "2N3906": "Q2N3906"}
SUFFIXES = {"": 1.0, "R": 1.0, "k": 1e3, "K": 1e3, "M": 1e6, "m": 1e-3, "u": 1e-6, "n": 1e-9, "p": 1e-12}
RELAY_COIL_OHM = 579.0
CONTACT_CLOSED_OHM = 0.05
CONTACT_OPEN_OHM = 1e12


class SpiceError(Exception):
    pass


def parse_value(text: str) -> float:
    tokens = text.split()
    match = re.fullmatch(r"([0-9]+(?:\.[0-9]+)?)([RkKMmunp]?)", tokens[0]) if tokens else None
    if not match:
        raise SpiceError(f"값을 파싱하지 못함: {text!r}")
    return float(match.group(1)) * SUFFIXES[match.group(2)]


def node(net: str) -> str:
    return "0" if net == "GND" else net.lower()


def fmt(value: float) -> str:
    return f"{value:.6g}"


def opamp_power(blocks: list[Block]) -> dict[str, tuple[str, str]]:
    power = {}
    for block in blocks:
        for part in block.parts:
            if part.name == "TL072" and part.unit == 3:
                power[part.ref] = (node(part.nets["8"]), node(part.nets["4"]))
    return power


def _contact(ref: str, common: str, other: str, nets: dict[str, str], closed_when_on: bool) -> str:
    on, off = (CONTACT_CLOSED_OHM, CONTACT_OPEN_OHM) if closed_when_on else (CONTACT_OPEN_OHM, CONTACT_CLOSED_OHM)
    return f"R{ref}_{common}_{other} {nets[common]} {nets[other]} {{{ref.lower()}_on > 0.5 ? {on:g} : {off:g}}}"


def part_lines(part: Part, power: dict[str, tuple[str, str]], scale: float = 1.0) -> list[str]:
    kind = (part.lib, part.name)
    if kind in SKIPPED:
        return []
    nets = {pin: node(net) for pin, net in part.nets.items() if net != "NC"}
    ref = part.ref
    if kind in (("Device", "R"), ("Device", "C"), ("Device", "C_Polarized")):
        return [f"{ref} {nets['1']} {nets['2']} {fmt(parse_value(part.value) * scale)}"]
    if part.lib == "Device" and part.name in ("D", "D_Schottky", "D_Zener", "LED"):
        model = "DLEDR" if part.name == "LED" else DIODE_MODELS.get(part.value.split()[0])
        if model is None:
            raise SpiceError(f"다이오드 모델 없음: {ref} {part.value}")
        return [f"{ref} {nets['2']} {nets['1']} {model}"]
    if part.lib == "Transistor_BJT" and part.name in BJT_MODELS:
        return [f"{ref} {nets['3']} {nets['2']} {nets['1']} {BJT_MODELS[part.name]}"]
    if kind == ("Transistor_FET", "IRF9540N"):
        return [f"X{ref} {nets['2']} {nets['1']} {nets['3']} IRF9540N"]
    if kind == ("Amplifier_Operational", "TL072"):
        if part.unit == 3:
            return []
        vcc, vee = power[ref]
        plus, minus, out = ("3", "2", "1") if part.unit == 1 else ("5", "6", "7")
        letter = "AB"[part.unit - 1]
        return [f"X{ref}{letter} {nets[plus]} {nets[minus]} {nets[out]} {vcc} {vee} opamp_model"]
    if kind == ("Regulator_SwitchedCapacitor", "LT1054"):
        return [f"X{ref} {nets['8']} {nets['2']} {nets['3']} lt1054"]
    if kind == ("Relay", "Relay_DPDT"):
        return [f"R{ref}_COIL {nets['A1']} {nets['A2']} {fmt(RELAY_COIL_OHM * scale)}",
                _contact(ref, "11", "12", nets, closed_when_on=False),
                _contact(ref, "11", "14", nets, closed_when_on=True),
                _contact(ref, "21", "22", nets, closed_when_on=False),
                _contact(ref, "21", "24", nets, closed_when_on=True)]
    if kind == ("Device", "R_Potentiometer"):
        total = fmt(parse_value(part.value.split()[0].lstrip("AB")) * scale)
        position = f"pos_{ref.lower()}"
        return [f"R{ref}_A {nets['1']} {nets['2']} {{max(1, {total} * (1 - {position}))}}",
                f"R{ref}_B {nets['2']} {nets['3']} {{max(1, {total} * {position})}}"]
    raise SpiceError(f"SPICE 매핑 없음: {ref} {part.lib}:{part.name}")
```

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/gen_spice.py effector/hardware/tests/
git commit -m "[Feat] 이펙터 BLOCKS에서 SPICE 소자 줄을 만드는 매핑"
```

---

### Task 2: 모델 파일, 생성 CLI, ngspice 실행기

**Files:**
- Modify: `effector/hardware/gen_spice.py` (생성 함수와 CLI 추가)
- Create: `effector/hardware/ngspice_run.py`
- Create: `effector/hardware/spice/models.inc`
- Create: `effector/hardware/spice/lt1054_behavioral.inc`
- Create: `effector/hardware/spice/generated/` (생성물)
- Create: `effector/hardware/tests/test_spice_models.py`

**Interfaces:**
- Consumes: Task 1의 `part_lines`, `opamp_power`, `node`
- Produces:
  - `gen_spice.SUBSETS: dict[str, list[str]]` (`{"vbias": ["R11", "R12", "C6"]}`)
  - `gen_spice.blocks_hash() -> str` (16자리 16진수)
  - `gen_spice.emit(title: str, parts: list[Part], scales: dict[str, float] | None = None) -> str`
  - `gen_spice.generate(out: Path = GENERATED, scales: dict[str, float] | None = None) -> list[Path]`
  - `ngspice_run.SPICE_DIR: Path`, `ngspice_run.available() -> bool`
  - `ngspice_run.run(netlist: Path, expect: list[str] = ()) -> dict[str, float]` (키는 소문자)
  - `ngspice_run.run_text(text: str, directory: Path = SPICE_DIR, expect: list[str] = ()) -> dict[str, float]`
  - `ngspice_run.NgspiceError(Exception)`
  - `ngspice_run.requires_ngspice` (unittest 건너뛰기 데코레이터)

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_spice_models.py`:

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
import ngspice_run  # noqa: E402
from gen_schematic import BLOCKS, Part  # noqa: E402


class GeneratedFreshTest(unittest.TestCase):
    def test_generated_files_match_blocks(self) -> None:
        expected = gen_spice.blocks_hash()
        files = sorted(gen_spice.GENERATED.glob("*.cir"))
        self.assertTrue(files, "spice/generated/가 비어 있다. python3 effector/hardware/gen_spice.py 실행")
        for path in files:
            with self.subTest(path=path.name):
                header = path.read_text(encoding="utf-8").splitlines()[2]
                self.assertEqual(header, f"* BLOCKS {expected}",
                                 "BLOCKS가 바뀌었다. python3 effector/hardware/gen_spice.py로 다시 생성")

    def test_net_preservation(self) -> None:
        power = gen_spice.opamp_power(BLOCKS)
        for block in BLOCKS:
            expected = set()
            for p in block.parts:
                if (p.lib, p.name) in gen_spice.SKIPPED or (p.name == "TL072" and p.unit == 3):
                    continue
                expected |= {gen_spice.node(n) for n in p.nets.values() if n != "NC"}
            emitted = set()
            for p in block.parts:
                for line in gen_spice.part_lines(p, power):
                    fields = line.split()
                    count = {"X": len(fields) - 2, "Q": 3}.get(line[0], 2)
                    emitted |= set(fields[1:1 + count])
            if any(p.name == "TL072" and p.unit != 3 for p in block.parts):
                emitted -= {"vrail"} - expected
            with self.subTest(block=block.title[0]):
                self.assertEqual(emitted, expected)


@ngspice_run.requires_ngspice
class NgspiceTest(unittest.TestCase):
    def test_vbias_divider(self) -> None:
        text = ".include generated/vbias.cir\nVrail vrail 0 16.9\n.op\n.control\nrun\nprint v(vbias)\n.endc\n.end\n"
        result = ngspice_run.run_text(text, expect=["v(vbias)"])
        self.assertAlmostEqual(result["v(vbias)"], 8.45, places=6)

    def test_rc_cutoff(self) -> None:
        power: dict[str, tuple[str, str]] = {}
        r = gen_spice.part_lines(Part("R1", "Device", "R", "1k", {"1": "IN", "2": "OUT"}), power)
        c = gen_spice.part_lines(Part("C1", "Device", "C", "159.155n", {"1": "OUT", "2": "GND"}), power)
        text = "\n".join(["* rc", "Vin in 0 AC 1", *r, *c, ".ac dec 200 100 10k", ".control", "run",
                          "meas ac f3 WHEN vdb(out)=-3.0103", ".endc", ".end"]) + "\n"
        result = ngspice_run.run_text(text, expect=["f3"])
        self.assertAlmostEqual(result["f3"], 1000.0, delta=2.0)

    def relay_text(self, k1_on: str) -> str:
        return "\n".join([
            ".include models.inc", ".include generated/block_F.cir", k1_on,
            "Vt in_tip 0 1", "Rl in_a 0 1k", "Vv9 v9 0 9", "Vdrv seed_d0_k1drv 0 0",
            ".op", ".control", "run", "print v(in_a)", ".endc", ".end"]) + "\n"

    def test_relay_parameter(self) -> None:
        on = ngspice_run.run_text(self.relay_text(".param k1_on=1"), expect=["v(in_a)"])
        off = ngspice_run.run_text(self.relay_text(".param k1_on=0"), expect=["v(in_a)"])
        self.assertAlmostEqual(on["v(in_a)"], 1000 / 1000.05, places=6)
        self.assertLess(abs(off["v(in_a)"]), 1e-6)

    def test_missing_parameter_raises(self) -> None:
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text(self.relay_text("* k1_on 없음"), expect=["v(in_a)"])

    def test_error_and_missing_result_raise(self) -> None:
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text("* bad\nX1 a b nosuch\n.op\n.end\n")
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text("* ok\nR1 a 0 1k\nV1 a 0 1\n.op\n.control\nrun\n.endc\n.end\n", expect=["v(a)"])

    def test_led_forward_voltage(self) -> None:
        text = ".include models.inc\nI1 0 a 3m\nD1 a 0 DLEDR\n.op\n.control\nrun\nprint v(a)\n.endc\n.end\n"
        result = ngspice_run.run_text(text, expect=["v(a)"])
        self.assertTrue(1.8 < result["v(a)"] < 2.1)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: `ModuleNotFoundError: No module named 'ngspice_run'`

- [ ] **Step 3: 모델 파일 작성**

`effector/hardware/spice/models.inc` (기존 protection.cir·doubler.cir 모델을 모으고 나머지는 범용값):

```spice
* 모델 카드. gen_spice.py 생성물이 쓰는 이름이다. 값의 출처는 spice/README.md
.model D1N5819 D (IS=1e-8 N=1.05 RS=0.05)
.model D1N4148 D (IS=2.52n N=1.752 RS=0.568 BV=100 IBV=100u)
.model DZ12 D (IS=1e-12 BV=12 IBV=1m N=1)
.model DZ15 D (IS=1e-12 BV=15 IBV=1m N=1)
.model DLEDR D (IS=1e-20 N=1.9 RS=10)
.model Q2N3906 PNP (IS=1e-14 BF=200 VAF=50)
.model Q2N3904 NPN (IS=1e-14 BF=200 VAF=100)
.model PMOSQ PMOS (LEVEL=1 VTO=-3.7 KP=1.36 LAMBDA=0.001)
.model DBODY D (IS=1e-12 N=1.2 RS=0.02)
.subckt IRF9540N d g s
M1 d g s s PMOSQ W=1u L=1u
Db d s DBODY
.ends
```

`effector/hardware/spice/lt1054_behavioral.inc` (기존 doubler.cir의 스위치 모델):

```spice
* LT1054 동작 모델. 25kHz 스위치(온저항 RON)가 CAP+를 VIN과 GND 사이로 흔든다. 손실은 6mA에서 약 0.3V
.subckt lt1054 vin capp gnd params: ron=25
Vosc osc gnd PULSE(0 1 0 100n 100n 20u 40u)
Boscn oscn gnd V = 1 - V(osc)
S1 vin capp osc gnd SWM
S2 capp gnd oscn gnd SWM
.model SWM SW (RON={ron} ROFF=1e9 VT=0.5 VH=0.1)
.ends
```

- [ ] **Step 4: 실행기와 생성 CLI 구현**

`effector/hardware/ngspice_run.py`:

```python
"""ngspice 배치 실행과 결과 읽기. meas 결과와 print 결과(`이름 = 값`)를 사전으로 돌려준다."""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SPICE_DIR = Path(__file__).resolve().parent / "spice"
RESULT = re.compile(r"^\s*([A-Za-z_][\w().,+\-*/]*)\s+=\s+([-+]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][-+]?[0-9]+)?)")
ERROR = re.compile(r"^\s*(?:error|.*\bfailed\b)", re.IGNORECASE | re.MULTILINE)


class NgspiceError(Exception):
    pass


def available() -> bool:
    return shutil.which("ngspice") is not None


requires_ngspice = unittest.skipUnless(available(), "ngspice 없음. brew install ngspice 뒤 다시 실행")


def run(netlist: Path, expect: list[str] = ()) -> dict[str, float]:
    completed = subprocess.run(["ngspice", "-b", netlist.name], cwd=netlist.parent,
                               capture_output=True, text=True, timeout=1800)
    output = completed.stdout + completed.stderr
    if completed.returncode != 0 or ERROR.search(output):
        raise NgspiceError(f"{netlist.name}: ngspice 에러\n{output[-3000:]}")
    results = {}
    for line in output.splitlines():
        match = RESULT.match(line)
        if match:
            results[match.group(1).lower()] = float(match.group(2))
    missing = [name for name in expect if name.lower() not in results]
    if missing:
        raise NgspiceError(f"{netlist.name}: 결과 없음 {missing}\n{output[-3000:]}")
    return results


def run_text(text: str, directory: Path = SPICE_DIR, expect: list[str] = ()) -> dict[str, float]:
    handle, name = tempfile.mkstemp(suffix=".cir", prefix="_run_", dir=directory)
    try:
        with os.fdopen(handle, "w", encoding="utf-8") as stream:
            stream.write(text)
        return run(Path(name), expect)
    finally:
        os.unlink(name)
```

`gen_spice.py`에 추가(맨 위 import에 `hashlib`, `io`, `Path`와 `BLOCKS`를 더한다):

```python
import hashlib
import io
from pathlib import Path

from gen_schematic import BLOCKS, Block, Part

GENERATED = Path(__file__).resolve().parent / "spice" / "generated"
SUBSETS = {"vbias": ["R11", "R12", "C6"]}


def blocks_hash() -> str:
    return hashlib.sha256(repr(BLOCKS).encode("utf-8")).hexdigest()[:16]


def emit(title: str, parts: list[Part], scales: dict[str, float] | None = None) -> str:
    power = opamp_power(BLOCKS)
    scales = scales or {}
    lines = ["* 자동 생성: gen_spice.py. 손으로 고치지 않는다.", f"* {title}", f"* BLOCKS {blocks_hash()}"]
    for part in parts:
        lines += part_lines(part, power, scales.get(part.ref, 1.0))
    return "\n".join(lines) + "\n"


def generate(out: Path = GENERATED, scales: dict[str, float] | None = None) -> list[Path]:
    out.mkdir(parents=True, exist_ok=True)
    written = []
    by_ref = {p.ref: p for b in BLOCKS for p in b.parts if p.unit == 1}
    targets = [(f"block_{b.title[0]}.cir", b.title, b.parts) for b in BLOCKS]
    targets += [(f"{name}.cir", f"부분 묶음 {name}: {', '.join(refs)}. block_B와 같이 include하지 않는다",
                 [by_ref[r] for r in refs]) for name, refs in SUBSETS.items()]
    for filename, title, parts in targets:
        path = out / filename
        io.open(path, "w", encoding="utf-8", newline="\n").write(emit(title, parts, scales))
        written.append(path)
    return written


def main() -> None:
    for path in generate():
        print(path)


if __name__ == "__main__":
    main()
```

`.gitignore`(spice 폴더)에 실행기 임시 파일을 더한다:

```
vendor/
_run_*.cir
```

- [ ] **Step 5: 생성과 통과 확인**

Run: `python3 effector/hardware/gen_spice.py && python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: block_A~I와 vbias.cir 경로 10줄 출력, 모든 테스트 OK

- [ ] **Step 6: 커밋**

```bash
git add effector/hardware/gen_spice.py effector/hardware/ngspice_run.py effector/hardware/spice/ effector/hardware/tests/
git commit -m "[Feat] 이펙터 SPICE 생성 CLI, 모델 파일, ngspice 실행기"
```

---

### Task 3: 기존 테스트벤치 이전

**Files:**
- Modify: `effector/hardware/spice/opamp_rr.inc` (부분회로 이름을 `opamp_model`로)
- Create: `effector/hardware/spice/opamp_ti.inc`
- Modify: `effector/hardware/spice/protection.cir`, `doubler.cir`, `output_stage.cir`, `boot.cir`, `dc_polarity.cir`, `cable_ti.cir`
- Modify: `effector/hardware/spice/README.md`
- Create: `effector/hardware/tests/test_benches.py`

**Interfaces:**
- Consumes: Task 2의 `generated/*.cir`, `models.inc`, `lt1054_behavioral.inc`, `ngspice_run.run`
- Produces: 테스트벤치 meas 이름(아래 테스트가 기준). `opamp_model` 부분회로(핀 inp inn out vcc vee)

판정 범위는 회로 설계 12절 값에 이전으로 생길 차이를 더한 것이다. 더블러는 Vbias 분압(20kΩ)이 부하에 더해져
레일이 조금 낮아지고, 보호 회로는 게이트 보호 제너 D14가 더해진다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_benches.py`:

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ngspice_run  # noqa: E402

SPICE = ngspice_run.SPICE_DIR


@ngspice_run.requires_ngspice
class BenchTest(unittest.TestCase):
    def test_protection(self) -> None:
        r = ngspice_run.run(SPICE / "protection.cir",
                            ["v9_rev18", "v9_at9", "v9_at12", "v9_at15"])
        self.assertLess(abs(r["v9_rev18"]), 0.01)
        self.assertAlmostEqual(r["v9_at9"], 8.94, delta=0.03)
        self.assertGreater(r["v9_at12"], 11.9)
        self.assertLess(abs(r["v9_at15"]), 0.01)

    def test_doubler(self) -> None:
        r = ngspice_run.run(SPICE / "doubler.cir",
                            ["rail_avg", "rail_pp", "raw_pp", "t_90", "c8_avg", "c9_avg", "vbias_ratio"])
        self.assertTrue(16.40 < r["rail_avg"] < 16.65, r["rail_avg"])
        self.assertLess(r["rail_pp"], 50e-6)
        self.assertLess(r["raw_pp"], 3e-3)
        self.assertTrue(0.030 < r["t_90"] < 0.045, r["t_90"])
        self.assertGreater(r["c8_avg"], 1.0)
        self.assertGreater(r["c9_avg"], 1.0)
        self.assertAlmostEqual(r["vbias_ratio"], 0.5, delta=0.001)

    def test_output_stage(self) -> None:
        r = ngspice_run.run(SPICE / "output_stage.cir", ["out_pp", "out_pp_161"])
        self.assertAlmostEqual(r["out_pp"], 15.94, delta=0.1)
        self.assertAlmostEqual(r["out_pp_161"], 15.35, delta=0.1)

    def test_boot(self) -> None:
        r = ngspice_run.run(SPICE / "boot.cir", ["outc_max", "t_60dbu"])
        self.assertAlmostEqual(r["outc_max"], 5.91, delta=0.2)
        self.assertTrue(9.0 < r["t_60dbu"] < 9.8, r["t_60dbu"])

    def test_dc_polarity(self) -> None:
        r = ngspice_run.run(SPICE / "dc_polarity.cir", ["c2", "c4", "c5", "c6"])
        for name in ("c2", "c4", "c5", "c6"):
            with self.subTest(capacitor=name):
                self.assertGreater(r[name], 8.0)

    @unittest.skipUnless((SPICE / "vendor" / "opa2992.lib").exists(), "TI 모델 없음(spice/README.md)")
    def test_cable_ti(self) -> None:
        r = ngspice_run.run(SPICE / "cable_ti.cir", ["step_final", "step_max", "step47n_final", "step47n_max"])
        for final, peak in (("step_final", "step_max"), ("step47n_final", "step47n_max")):
            with self.subTest(case=final):
                overshoot = (r[peak] - r[final]) / abs(r[final] - 8.3)
                self.assertLess(overshoot, 0.02)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest effector.hardware.tests.test_benches -v` 대신 `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_benches.py" -v`
Expected: protection `결과 없음 ['v9_rev18', ...]`으로 실패, doubler `c8_avg` 등 누락으로 실패

- [ ] **Step 3: op-amp 선택 파일**

`opamp_rr.inc`의 `.subckt opamp_rr`를 `.subckt opamp_model`로, 첫 줄 주석 끝에 "생성 넷리스트의 opamp_model"을 더한다.

`effector/hardware/spice/opamp_ti.inc`:

```spice
* TI OPA2992 모델을 opamp_model 핀 순서로 감싼다. vendor/opa2992.lib 필요(README)
.include vendor/opa2992.lib
.subckt opamp_model inp inn out vcc vee
X1 inp inn vcc vee out OPA2992
.ends
```

- [ ] **Step 4: 테스트벤치 다시 쓰기**

`protection.cir`:

```spice
* 블록 A: 백투백 P-MOSFET 보호. VIN_RAW -18~+18V DC 스윕. 부하 40옴(9V에서 225mA)
.include models.inc
.include generated/block_A.cir
Vin vin_raw 0 DC 9
Rload v9 0 40
.dc Vin -18 18 0.1
.control
run
meas dc v9_rev18 FIND v(v9) AT=-18
meas dc v9_at9 FIND v(v9) AT=9
meas dc v9_at12 FIND v(v9) AT=12
meas dc v9_at12p5 FIND v(v9) AT=12.5
meas dc v9_at13 FIND v(v9) AT=13
meas dc v9_at15 FIND v(v9) AT=15
.endc
.end
```

`doubler.cir`:

```spice
* 블록 B: LT1054 동작 모델 더블러의 레일 평균, 리플, 기동, 극성 C 전위. V9 9V, 레일 부하 2.8k(약 6mA)
.include models.inc
.include lt1054_behavioral.inc
.include generated/block_B.cir
V9 v9 0 9
Rload vrail 0 2800
.tran 0.5u 300m
.control
run
meas tran rail_avg AVG v(vrail) from=250m to=300m
meas tran rail_pp PP v(vrail) from=290m to=300m
meas tran raw_pp PP v(vrail_raw) from=290m to=300m
meas tran t_90 WHEN v(vrail)=15.2 RISE=1
meas tran c8_avg AVG v(pumpx,pump) from=250m to=300m
meas tran c9_avg AVG v(vrail_raw) from=250m to=300m
meas tran vbias_avg AVG v(vbias) from=250m to=300m
let vbias_ratio = vbias_avg / rail_avg
print vbias_ratio
.endc
.end
```

`output_stage.cir`:

```spice
* 블록 E: 출력단 헤드룸. 모듈 출력 1Vrms(공통모드 1.65V)를 +15.1dB로. 레일 16.9V와 16.1V에서 클립 확인
.include opamp_rr.inc
.include generated/vbias.cir
.include generated/block_E.cir
Vrail vrail 0 16.9
Vseed seed_audio_out1 0 SIN(1.65 1.4142 1k)
Rjack out_c 0 10k
.ic v(vbias)=8.45 v(fb_c)=8.45 v(out_r)=8.45 v(out_in)=8.45
.tran 5u 400m
.control
run
meas tran out_pp PP v(out_c) from=380m to=400m
meas tran out_max MAX v(u1b_out) from=380m to=400m
meas tran out_min MIN v(u1b_out) from=380m to=400m
alter Vrail 16.1
run
meas tran out_pp_161 PP v(out_c) from=380m to=400m
meas tran out_max_161 MAX v(u1b_out) from=380m to=400m
meas tran out_min_161 MIN v(u1b_out) from=380m to=400m
.endc
.end
```

`boot.cir`:

```spice
* 부팅 과도: 레일이 20ms에 걸쳐 0에서 16.9V로, 모듈 출력 DC 1.65V가 100ms에 서는 경우 OUT_C(K1 NO 접점 쪽) 잔류 전압
.include opamp_rr.inc
.include generated/vbias.cir
.include generated/block_E.cir
Vrail vrail 0 PWL(0 0 20m 16.9)
Vseed seed_audio_out1 0 PWL(0 0 100m 1.65)
.tran 1m 15
.control
run
meas tran outc_max MAX v(out_c)
meas tran t_100mv WHEN v(out_c)=0.1 FALL=LAST
meas tran t_10mv WHEN v(out_c)=0.01 FALL=LAST
meas tran t_1mv WHEN v(out_c)=0.001 FALL=LAST
meas tran t_60dbu WHEN v(out_c)=0.00078 FALL=LAST
meas tran v_5s FIND v(out_c) AT=5
meas tran v_10s FIND v(out_c) AT=10
.endc
.end
```

`dc_polarity.cir`:

```spice
* 극성 커패시터 C2, C4, C5, C6 양단 DC 전위(+ 쪽 - 쪽). 레일 16.6V(더블러 시뮬레이션 평균), 모듈 출력 DC 1.65V, 입력 무신호
.include opamp_rr.inc
.include generated/vbias.cir
.include generated/block_D.cir
.include generated/block_E.cir
Vrail vrail 0 16.6
Vseed seed_audio_out1 0 1.65
Rcodec seed_audio_in1 0 5k
.op
.control
run
let c2 = v(buf_out) - v(pad_in)
let c4 = v(fb_c)
let c5 = v(out_r) - v(out_c)
let c6 = v(vbias)
print c2 c4 c5 c6
.endc
.end
```

`cable_ti.cir`:

```spice
* 출력단(TI OPA2992 모델) 계단 응답. R9 100옴 뒤 케이블 용량 0 / 1nF / 4.7nF, 앰프 입력 10k. vendor/opa2992.lib 필요(README)
.include opamp_ti.inc
.include generated/vbias.cir
.include generated/block_E.cir
Vrail vrail 0 16.6
Vseed seed_audio_out1 0 PULSE(1.55 1.75 400m 1u 1u 2m 4m)
Rjack out_c 0 10k
Ccable out_c 0 1p
.tran 1u 403m 399m
.control
run
meas tran step_final FIND v(out_c) AT=402.9m
meas tran step_max MAX v(out_c) from=400m to=400.5m
alter Ccable 1n
run
meas tran step1n_final FIND v(out_c) AT=402.9m
meas tran step1n_max MAX v(out_c) from=400m to=400.5m
alter Ccable 4.7n
run
meas tran step47n_final FIND v(out_c) AT=402.9m
meas tran step47n_max MAX v(out_c) from=400m to=400.5m
.endc
.end
```

`README.md` 표 위에 다음 단락을 더하고, 표의 "모델의 한계" 칸에서 부품값 서술을 지운다(값은 생성물이 가진다):

```markdown
부품 값은 테스트벤치에 적지 않는다. `python3 effector/hardware/gen_spice.py`가 `gen_schematic.py`의 BLOCKS에서
`generated/block_<문자>.cir`와 `generated/vbias.cir`(R11·R12·C6)를 만들고 테스트벤치가 include한다. BLOCKS를 고치면
다시 생성한다. 판정 범위는 `effector/hardware/tests/test_benches.py`에 있다. 모델 카드는 `models.inc`,
op-amp는 `opamp_rr.inc`(동작 모델)·`opamp_rr_noise.inc`(잡음 모델)·`opamp_ti.inc`(TI 모델) 중 하나를 include한다.
```

- [ ] **Step 5: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK. cable_ti는 TI 모델이 없으면 skipped

판정 범위를 벗어나면 원인을 찾는다. 기존 사본과 BLOCKS의 값 차이면 BLOCKS가 맞고, 범위를 그 값으로 고친 뒤
결과 보고(Task 9)에 차이를 적는다. 이전 오류면 테스트벤치를 고친다.

- [ ] **Step 6: 커밋**

```bash
git add effector/hardware/spice/ effector/hardware/tests/test_benches.py
git commit -m "[Refactor] 이펙터 SPICE 테스트벤치가 BLOCKS 생성물을 include하도록 이전"
```

---

### Task 4: op-amp 잡음 모델

**Files:**
- Create: `effector/hardware/spice/opamp_rr_noise.inc`
- Create: `effector/hardware/tests/test_noise_model.py`

**Interfaces:**
- Consumes: Task 2의 `ngspice_run.run_text`
- Produces: `opamp_model` 부분회로(핀 inp inn out vcc vee, 파라미터 `en`(V/√Hz, 기본 7n), `in`(A/√Hz, 기본 60f))

OPA2992 데이터시트: 전압 잡음 1kHz 7nV/√Hz, 10kHz 4.4nV/√Hz, 전류 잡음 1kHz 60fA/√Hz. 1/f 모양은 넣지 않고 백색
`en` 두 값(7n 보수, 4.4n 광대역)으로 범위를 낸다. 잡음원은 동작점과 분리된 1kΩ 저항의 열잡음 전류를 전류
감지 전압원으로 읽어 CCVS·CCCS로 키운다. 내부 저항은 `noisy=0`이다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_noise_model.py`:

```python
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ngspice_run  # noqa: E402

BOLTZMANN = 1.380649e-23
KELVIN = 300.15


def thermal(ohm: float) -> float:
    return math.sqrt(4 * BOLTZMANN * KELVIN * ohm)


def noise_at_1k(body: str) -> float:
    text = "\n".join([".include opamp_rr_noise.inc", body, ".noise v(out) Vin dec 10 1k 1k",
                      ".control", "run", "setplot noise1", "print onoise_spectrum", ".endc", ".end"]) + "\n"
    return ngspice_run.run_text(text, expect=["onoise_spectrum"])["onoise_spectrum"]


@ngspice_run.requires_ngspice
class NoiseModelTest(unittest.TestCase):
    def test_single_resistor(self) -> None:
        value = noise_at_1k("Vin in 0 DC 0 AC 1\nRs in out 10k\nC1 out 0 1f")
        self.assertAlmostEqual(value, thermal(10e3), delta=thermal(10e3) * 0.01)

    def test_noninverting_gain_5_7(self) -> None:
        body = "\n".join(["Vcc vcc 0 16.9", "Vin inp 0 DC 8.45 AC 1",
                          "X1 inp fb out vcc 0 opamp_model en=7n", "R7 out fb 47k", "R8 fb fbc 10k",
                          "Vb fbc 0 8.45"])
        expected = math.sqrt((7e-9 * 5.7) ** 2 + (thermal(10e3) * 4.7) ** 2 + thermal(47e3) ** 2)
        self.assertAlmostEqual(noise_at_1k(body), expected, delta=expected * 0.01)

    def test_current_noise_into_source_resistance(self) -> None:
        body = "\n".join(["Vcc vcc 0 16.9", "Vin src 0 DC 8.45 AC 1", "Rs src inp 1meg noisy=0",
                          "X1 inp out out vcc 0 opamp_model en=1e-15 in=60f"])
        self.assertAlmostEqual(noise_at_1k(body), 60e-15 * 1e6, delta=60e-15 * 1e6 * 0.02)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_noise_model.py" -v`
Expected: NgspiceError (opamp_rr_noise.inc 없음)

- [ ] **Step 3: 구현**

`effector/hardware/spice/opamp_rr_noise.inc`:

```spice
* opamp_rr.inc의 동작에 입력 잡음원을 더한 opamp_model. en: 입력 전압 잡음(V/rtHz), in: 입력 전류 잡음(A/rtHz)
* OPA2992 데이터시트 1kHz 7nV/rtHz, 10kHz 4.4nV/rtHz, 60fA/rtHz. 잡음원은 동작점과 분리된 1k 저항의 열잡음
.subckt opamp_model inp inn out vcc vee params: en=7n in=60f
.param inoise_1k = {sqrt(4 * 1.380649e-23 * 300.15 / 1000)}
Rvn nv 0 1k
Vvn nv 0 0
Hvn inp inpn Vvn {en / inoise_1k}
Rip np 0 1k
Vip np 0 0
Fip 0 inpn Vip {in / inoise_1k}
Rim nm 0 1k
Vim nm 0 0
Fim 0 inn Vim {in / inoise_1k}
B1 a 0 V = 1e4 * (V(inpn) - V(inn))
R1 a b 1k noisy=0
C1 b 0 159n
B2 c 0 V = max(min(V(b), V(vcc) - 0.25), V(vee) + 0.25)
R2 c out 50 noisy=0
.ends
```

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/spice/opamp_rr_noise.inc effector/hardware/tests/test_noise_model.py
git commit -m "[Feat] 이펙터 잡음원이 있는 op-amp 동작 모델과 이론값 대조"
```

---

### Task 5: 잡음 예산

**Files:**
- Create: `effector/hardware/spice/noise_budget.py`
- Create: `effector/hardware/tests/test_noise_budget.py`

**Interfaces:**
- Consumes: Task 2 `ngspice_run.run`, `SPICE_DIR`, Task 4 `opamp_rr_noise.inc`
- Produces:
  - `a_weight(f: float) -> float` (전력 가중, 1kHz에서 1)
  - `band(lo: float, hi: float) -> Callable[[float], float]`
  - `WEIGHTINGS: dict[str, Callable[[float], float]]` (키 "A", "125-8000", "20-20k")
  - `integrate(frequencies: list[float], density: list[float], weight) -> float` (Vrms)
  - `white_ratio(weight, reference) -> float` (백색 잡음에서 두 가중의 RMS 비)
  - `dbu(volts: float) -> float`
  - `t7_zone(dynamic_range_db: float) -> str` ("통과", "회색", "미달")
  - `Spectrum(frequencies: list[float], vectors: dict[str, list[float]])`, `read_spectrum(path: Path) -> Spectrum`
  - `main()`이 예산표 markdown을 출력

입력 경로 잡음은 `inoise_spectrum`(입력 잭 기준), 출력 경로는 `onoise_spectrum`(출력 잭 기준)이다. 시스템이
유니티라 출력 잭 기준 잡음이 곧 입력 기준이다. 코덱 다이내믹 레인지는 A 가중 값으로 보고, 다른 가중은
백색 가정으로 `white_ratio`를 곱한다. 경로 게인은 1kHz의 onoise/inoise 비다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_noise_budget.py`:

```python
import math
import sys
import tempfile
import unittest
from pathlib import Path

HARDWARE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HARDWARE))
sys.path.insert(0, str(HARDWARE / "spice"))

import noise_budget as nb  # noqa: E402


def log_grid(lo: float, hi: float, per_decade: int) -> list[float]:
    count = int(round(math.log10(hi / lo) * per_decade))
    return [lo * 10 ** (i / per_decade) for i in range(count + 1)]


class WeightingTest(unittest.TestCase):
    def test_a_weight_reference_points(self) -> None:
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(1000)), 0.0, delta=0.01)
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(100)), -19.1, delta=0.1)
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(10000)), -2.5, delta=0.1)

    def test_white_ratio_a_vs_band(self) -> None:
        ratio = nb.white_ratio(nb.WEIGHTINGS["A"], nb.WEIGHTINGS["125-8000"])
        self.assertAlmostEqual(20 * math.log10(ratio), 2.12, delta=0.02)


class IntegrateTest(unittest.TestCase):
    def test_white_noise_band_on_log_grid(self) -> None:
        frequencies = log_grid(20, 24000, 50)
        density = [10e-9] * len(frequencies)
        vrms = nb.integrate(frequencies, density, nb.WEIGHTINGS["20-20k"])
        expected = 10e-9 * math.sqrt(20000 - 20)
        self.assertAlmostEqual(20 * math.log10(vrms / expected), 0.0, delta=0.1)

    def test_sparse_grid_stays_within_0_1_db(self) -> None:
        frequencies = log_grid(20, 24000, 10)
        density = [10e-9] * len(frequencies)
        exact = 10e-9 * math.sqrt(nb.white_power(nb.WEIGHTINGS["A"]))
        vrms = nb.integrate(frequencies, density, nb.WEIGHTINGS["A"])
        self.assertAlmostEqual(20 * math.log10(vrms / exact), 0.0, delta=0.1)


class ZoneTest(unittest.TestCase):
    def test_boundaries(self) -> None:
        self.assertEqual(nb.t7_zone(105.0), "통과")
        self.assertEqual(nb.t7_zone(104.99), "회색")
        self.assertEqual(nb.t7_zone(98.0), "회색")
        self.assertEqual(nb.t7_zone(97.99), "미달")


class ReadSpectrumTest(unittest.TestCase):
    def test_reads_named_columns(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "s.txt"
            path.write_text(" frequency onoise_spectrum onoise_r7\n 1.0e+03 2.0e-08 1.0e-08\n"
                            " 2.0e+03 3.0e-08 1.5e-08\n", encoding="utf-8")
            spectrum = nb.read_spectrum(path)
        self.assertEqual(spectrum.frequencies, [1000.0, 2000.0])
        self.assertEqual(spectrum.vectors["onoise_r7"], [1e-8, 1.5e-8])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_noise_budget.py" -v`
Expected: `ModuleNotFoundError: No module named 'noise_budget'`

- [ ] **Step 3: 구현**

`effector/hardware/spice/noise_budget.py`:

```python
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
                        analog_in = dbu(integrate(spectrum_in.frequencies, spectrum_in.vectors["inoise_spectrum"], weight))
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
```

생성 넷리스트의 op-amp 줄(`XU1A ... opamp_model`)에는 파라미터가 없어서, `en`은 전역 `.param en_value`로 넘긴다.
`opamp_rr_noise.inc`를 고친다. 첫 `.subckt` 줄 앞에 `.param en_value=7n`을 두고 `params: en=7n in=60f`를
`params: en={en_value} in=60f`로 바꾼다. 테스트벤치의 `.param en_value=...`가 include 뒤에 와서 그 값이 쓰인다.
Task 4 테스트의 인스턴스 지정(`en=7n`, `en=1e-15`)은 그대로 동작한다.

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK

Run: `python3 effector/hardware/spice/noise_budget.py`
Expected: 표 16행(입력 2 × 출력 소스 2 × 코덱 2 × en 2)과 부품별 기여 표 2개. 출력 쪽과 입력 쪽 값이 설계서 1.1절 어림값
(출력 약 -97.4, 입력 약 -100.8dBu(A), 칩 값 행)과 몇 dB 안이다. 크게 다르면 원인을 찾고 결과 보고에 적는다

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/spice/noise_budget.py effector/hardware/spice/opamp_rr_noise.inc effector/hardware/tests/test_noise_budget.py
git commit -m "[Feat] 이펙터 잡음 예산: 입력·출력 경로 .noise와 코덱 바닥 합산"
```

---

### Task 6: 몬테카를로

**Files:**
- Create: `effector/hardware/spice/monte_carlo.py`
- Create: `effector/hardware/tests/test_monte_carlo.py`

**Interfaces:**
- Consumes: Task 2 `gen_spice.emit`, `gen_spice.SUBSETS`, `ngspice_run.run_text`, `models.inc`, `lt1054_behavioral.inc`
- Produces:
  - `TOLERANCES: dict[str, float]` (지정자별 균일 분포 반폭, 비율)
  - `sample_scales(rng: random.Random, tolerances: dict[str, float]) -> dict[str, float]`
  - `schottky_is(vf_at_1ma: float) -> float`
  - `Sample(adapter_v, schottky_vf, lt1054_ron, scales)`, `draw(rng, zero: bool = False) -> Sample`
  - `simulate(sample: Sample, workdir: Path) -> dict[str, float]` (키 rail_min, pad_db, out_dbu, vbias_ratio)
  - `run(count: int, seed: int) -> list[dict[str, float]]`
  - `summarize(results) -> list[str]` (markdown)

공차: 저항 ±1%(R32 3.3Ω만 ±5%), 필름·세라믹 ±10%(C23 ±5%), 전해 ±20%, 어댑터 8.7~9.6V, 쇼트키 순방향 0.25~0.45V(1mA),
LT1054 온저항 20~40Ω(기존 25Ω 모델 기준. 실행 결과의 손실 범위를 결과 보고에 적는다). 회로는 블록 A·B(전원,
VIN_RAW에 어댑터, V9에 나머지 부하 40Ω, 레일 부하 2.8kΩ), 블록 D의 패드(AC 1kHz), 블록 E(레일 = 그 표본의
레일 최솟값, 0dBFS 사인)다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_monte_carlo.py`:

```python
import random
import sys
import tempfile
import unittest
from pathlib import Path

HARDWARE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HARDWARE))
sys.path.insert(0, str(HARDWARE / "spice"))

import monte_carlo as mc  # noqa: E402
import ngspice_run  # noqa: E402
from gen_schematic import BLOCKS  # noqa: E402


class ToleranceTest(unittest.TestCase):
    def test_every_r_and_c_has_tolerance(self) -> None:
        for block in BLOCKS:
            for p in block.parts:
                if p.lib == "Device" and p.name in ("R", "C", "C_Polarized"):
                    with self.subTest(ref=p.ref):
                        self.assertIn(p.ref, mc.TOLERANCES)
        self.assertEqual(mc.TOLERANCES["R32"], 0.05)
        self.assertEqual(mc.TOLERANCES["C23"], 0.05)
        self.assertEqual(mc.TOLERANCES["C7"], 0.20)
        self.assertEqual(mc.TOLERANCES["R7"], 0.01)

    def test_seed_reproducible_and_bounded(self) -> None:
        a = mc.draw(random.Random(7))
        b = mc.draw(random.Random(7))
        self.assertEqual(a, b)
        for ref, scale in a.scales.items():
            self.assertLessEqual(abs(scale - 1), mc.TOLERANCES[ref] + 1e-12)
        self.assertTrue(8.7 <= a.adapter_v <= 9.6)
        self.assertTrue(4000 <= a.codec_ohm <= 6000)

    def test_zero_sample_is_nominal(self) -> None:
        zero = mc.draw(random.Random(1), zero=True)
        self.assertTrue(all(scale == 1.0 for scale in zero.scales.values()))
        self.assertEqual(zero.lt1054_ron, 25.0)
        self.assertEqual(zero.codec_ohm, 5000.0)

    def test_schottky_is(self) -> None:
        self.assertAlmostEqual(mc.schottky_is(0.3125), 1e-8, delta=1e-9)


@ngspice_run.requires_ngspice
class SimulateTest(unittest.TestCase):
    def test_nominal_matches_benches(self) -> None:
        with tempfile.TemporaryDirectory(dir=ngspice_run.SPICE_DIR) as directory:
            result = mc.simulate(mc.draw(random.Random(1), zero=True), Path(directory))
        self.assertTrue(16.0 < result["rail_min"] < 16.7, result)
        self.assertAlmostEqual(result["pad_db"], -15.0, delta=0.6)
        self.assertGreater(result["out_dbu"], 16.5)
        self.assertAlmostEqual(result["vbias_ratio"], 0.5, delta=0.002)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_monte_carlo.py" -v`
Expected: `ModuleNotFoundError: No module named 'monte_carlo'`

- [ ] **Step 3: 구현**

`effector/hardware/spice/monte_carlo.py`:

```python
"""몬테카를로. 공차와 어댑터 전압 범위에서 레일, 패드 비율, 출력 스윙, Vbias 비의 분포를 본다.
설계 검증 도구 설계서 3.4절.

    python3 effector/hardware/spice/monte_carlo.py [--count 200] [--seed 1]
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
                table[p.ref] = 0.05 if p.ref == "C23" else FILM_OR_CERAMIC
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


def draw(rng: random.Random, zero: bool = False) -> Sample:
    if zero:
        return Sample(9.0, 0.3125, 25.0, 5000.0, {ref: 1.0 for ref in TOLERANCES})
    scales = sample_scales(rng, TOLERANCES)
    return Sample(rng.uniform(8.7, 9.6), rng.uniform(0.25, 0.45), rng.uniform(20.0, 40.0), rng.uniform(4000.0, 6000.0),
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
        "meas tran rail_min MIN v(vrail) from=100m to=150m",
        "meas tran rail_avg AVG v(vrail) from=100m to=150m",
        "meas tran vbias_avg AVG v(vbias) from=100m to=150m",
        "let vbias_ratio = vbias_avg / rail_avg", "print vbias_ratio", ".endc", ".end"]) + "\n",
        workdir, ["rail_min", "vbias_ratio"])
    pad = ngspice_run.run_text("\n".join([
        "* MC 패드", ".include opamp_rr.inc", ".include generated/vbias.cir", ".include generated/block_D.cir",
        f"Vrail vrail 0 {rail['rail_min']:.4f}", "Vin in_a 0 DC 0 AC 1",
        f"Rcodec seed_audio_in1 0 {sample.codec_ohm:.1f}", ".ac lin 1 1k 1k", ".control", "run",
        "meas ac pad_db FIND vdb(seed_audio_in1) AT=1k", ".endc", ".end"]) + "\n", workdir, ["pad_db"])
    swing = ngspice_run.run_text("\n".join([
        "* MC 출력 스윙", ".include opamp_rr.inc", ".include generated/vbias.cir", ".include generated/block_E.cir",
        f"Vrail vrail 0 {rail['rail_min']:.4f}", "Vseed seed_audio_out1 0 SIN(1.65 1.4142 1k)", "Rjack out_c 0 10k",
        f".ic v(vbias)={rail['rail_min'] / 2:.4f} v(fb_c)={rail['rail_min'] / 2:.4f} "
        f"v(out_r)={rail['rail_min'] / 2:.4f} v(out_in)={rail['rail_min'] / 2:.4f}",
        ".tran 5u 400m", ".control", "run", "meas tran out_pp PP v(out_c) from=380m to=400m", ".endc", ".end"]) + "\n",
        workdir, ["out_pp"])
    return {"rail_min": rail["rail_min"], "vbias_ratio": rail["vbias_ratio"], "pad_db": pad["pad_db"],
            "out_dbu": 20 * math.log10(swing["out_pp"] / (2 * math.sqrt(2)) / 0.7746)}


def run(count: int, seed: int) -> list[dict[str, float]]:
    rng = random.Random(seed)
    results = []
    for _ in range(count):
        sample = draw(rng)
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
    args = parser.parse_args()
    print("\n".join(summarize(run(args.count, args.seed))))


if __name__ == "__main__":
    main()
```

`lt1054_behavioral.inc`를 고친다. 첫 `.subckt` 줄 앞에 `.param ron_value=25`를 두고 `params: ron=25`를
`params: ron={ron_value}`로 바꾼다. doubler.cir 동작은 그대로이고, 몬테카를로 레일 테스트벤치의 `.param ron_value=...`가
include 뒤에 와서 그 값이 쓰인다. 코덱 입력 저항(TAC5242 5kΩ ±20%, 설계서 4.2절)은 `Sample.codec_ohm`으로 흔든다.

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK

Run: `python3 effector/hardware/spice/monte_carlo.py --count 200 --seed 1`
Expected: 4행 표. 수 분 걸린다

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/spice/monte_carlo.py effector/hardware/spice/lt1054_behavioral.inc effector/hardware/tests/test_monte_carlo.py
git commit -m "[Feat] 이펙터 몬테카를로: 공차·어댑터 범위의 레일·패드·스윙 분포"
```

---

### Task 7: 기구 데이터와 검사

**Files:**
- Create: `effector/hardware/enclosure.py`
- Create: `effector/hardware/tests/test_enclosure.py`

**Interfaces:**
- Produces:
  - `Dimension(value: float | None, source: str)`, `PartSpec(name, hole_d, outside_d, body_w, body_d, body_h)`
  - `Placement(label: str, part: str, face: str, x: float, y: float)`, `Box(label, x0, x1, y0, y1, z0, z1)`
  - `Case(width, length, height, wall, edge_margin)` (모두 Dimension)
  - `FACES = ("top", "in", "out", "back")`
  - `face_size(case, face, values) -> tuple[float, float]`, `to_case(case, placement, values) -> tuple[float, float, float]`
  - `body_box(case, placement, spec, values) -> Box`
  - `check(case, parts, placements, internal, values) -> list[Finding]`, `Finding(check: str, ok: bool, detail: str)`
  - `resolve(case, parts, internal, draft: bool) -> tuple[dict[str, float], list[str]]` (값 사전, 미측정 목록)
  - 상수 `CASE`, `PARTS`, `PLACEMENTS`, `INTERNAL`, `KNOB_GAP_MM = 6.1`

좌표계: 케이스 바닥 중심이 원점, X는 폭(95mm), Y는 길이(120mm, +Y가 DC 잭이 있는 뒷면), Z는 위. 면 좌표는 면
중심 기준 mm이고 윗면은 (x, y) = (X, Y), 입력면(+X)·출력면(-X)은 (x, y) = (Y, Z - 높이/2), 뒷면(+Y)은 (x, y) = (X,
Z - 높이/2)다. 몸체 상자는 면 안쪽 표면에서 안으로 `body_h`만큼, 단면 `body_w × body_d`다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/hardware/tests/test_enclosure.py`:

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import enclosure as en  # noqa: E402

M = en.Dimension


def case() -> en.Case:
    return en.Case(M(95, "t"), M(120, "t"), M(56, "t"), M(3, "t"), M(5, "t"))


def spec(name: str, outside: float, body: float = 10, depth: float = 10) -> en.PartSpec:
    return en.PartSpec(name, M(7, "t"), M(outside, "t"), M(body, "t"), M(body, "t"), M(depth, "t"))


def values_of(c: en.Case, parts: dict[str, en.PartSpec], internal: list) -> dict[str, float]:
    values, missing = en.resolve(c, parts, internal, draft=False)
    assert not missing, missing
    return values


class GeometryTest(unittest.TestCase):
    def test_face_transform(self) -> None:
        c = case()
        v = values_of(c, {}, [])
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "top", 10, -20), v), (10, -20, 56))
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "in", 30, 5), v), (47.5, 30, 33))
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "back", -15, 0), v), (-15, 60, 28))

    def test_body_box_goes_inward(self) -> None:
        c = case()
        parts = {"p": spec("p", 10, body=16, depth=12)}
        v = values_of(c, parts, [])
        box = en.body_box(c, en.Placement("k", "p", "top", 0, 0), parts["p"], v)
        self.assertEqual((box.x0, box.x1, box.z0, box.z1), (-8, 8, 56 - 3 - 12, 56 - 3))
        side = en.body_box(c, en.Placement("j", "p", "in", 0, 0), parts["p"], v)
        self.assertEqual((side.x0, side.x1), (47.5 - 3 - 12, 47.5 - 3))


class CheckTest(unittest.TestCase):
    def run_check(self, placements: list[en.Placement], parts: dict[str, en.PartSpec], internal=()) -> list[en.Finding]:
        c = case()
        return en.check(c, parts, placements, list(internal), values_of(c, parts, list(internal)))

    def failures(self, findings: list[en.Finding]) -> list[str]:
        return [f.check for f in findings if not f.ok]

    def test_outside_overlap(self) -> None:
        parts = {"knob": spec("knob", 18.9)}
        close = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 20, 0)]
        far = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 25, 0)]
        self.assertIn("패널 바깥 간섭", self.failures(self.run_check(close, parts)))
        self.assertNotIn("패널 바깥 간섭", self.failures(self.run_check(far, parts)))

    def test_knob_gap_rule(self) -> None:
        parts = {"knob": spec("knob", 18.9)}
        tight = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 24.9, 0)]
        self.assertIn("패널 바깥 간섭", self.failures(self.run_check(tight, parts)))

    def test_edge_margin_and_out_of_face(self) -> None:
        parts = {"led": spec("led", 3)}
        inside = [en.Placement("a", "led", "top", 47.5 - 5 - 1.5, 0)]
        outside = [en.Placement("a", "led", "top", 47.5 - 5 - 1.4, 0)]
        negative = [en.Placement("a", "led", "top", -60, 0)]
        self.assertNotIn("가장자리 여유", self.failures(self.run_check(inside, parts)))
        self.assertIn("가장자리 여유", self.failures(self.run_check(outside, parts)))
        self.assertIn("가장자리 여유", self.failures(self.run_check(negative, parts)))

    def test_inside_collision_across_faces(self) -> None:
        parts = {"pot": spec("pot", 10, body=16, depth=30), "jack": spec("jack", 10, body=16, depth=30)}
        placements = [en.Placement("pot", "pot", "top", 35, 0), en.Placement("jack", "jack", "in", 0, 15)]
        self.assertIn("패널 안쪽 간섭", self.failures(self.run_check(placements, parts)))

    def test_floor_stack(self) -> None:
        parts = {"pot": spec("pot", 10, body=16, depth=40)}
        stack = [en.Box("seed3", -10, 10, -10, 10, 3, 3 + 20)]
        self.assertIn("패널 안쪽 간섭", self.failures(
            self.run_check([en.Placement("pot", "pot", "top", 0, 0)], parts, stack)))
        too_deep = {"pot": spec("pot", 10, body=16, depth=60)}
        self.assertIn("높이 스택", self.failures(self.run_check([en.Placement("pot", "pot", "top", 30, 30)], too_deep)))


class ResolveTest(unittest.TestCase):
    def test_unmeasured_blocks_final(self) -> None:
        c = en.Case(M(95, "t"), M(120, "t"), M(56, "t"), M(None, "미측정"), M(5, "t"))
        values, missing = en.resolve(c, {}, [], draft=False)
        self.assertEqual(missing, ["케이스 벽 두께"])
        values, missing = en.resolve(c, {}, [], draft=True)
        self.assertEqual(missing, ["케이스 벽 두께"])
        self.assertEqual(values["case.wall"], en.DRAFT_DEFAULTS["case.wall"])

    def test_project_data_resolves_in_draft(self) -> None:
        values, missing = en.resolve(en.CASE, en.PARTS, en.INTERNAL, draft=True)
        self.assertTrue(missing)
        en.check(en.CASE, en.PARTS, en.PLACEMENTS, en.INTERNAL, values)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_enclosure.py" -v`
Expected: `ModuleNotFoundError: No module named 'enclosure'`

- [ ] **Step 3: 구현**

`effector/hardware/enclosure.py`:

```python
"""케이스 기구 기준과 검사·타공 도면. 설계 검증 도구 설계서 5절.

    python3 effector/hardware/enclosure.py --draft   미측정 값을 자리표시로 채운 초안
    python3 effector/hardware/enclosure.py --final   미측정 값이 있으면 실패
"""
from __future__ import annotations

import itertools
import math
from dataclasses import dataclass
from pathlib import Path

KNOB_GAP_MM = 6.1
FACES = ("top", "in", "out", "back")


@dataclass(frozen=True)
class Dimension:
    value: float | None
    source: str


@dataclass(frozen=True)
class Case:
    width: Dimension
    length: Dimension
    height: Dimension
    wall: Dimension
    edge_margin: Dimension


@dataclass(frozen=True)
class PartSpec:
    name: str
    hole_d: Dimension
    outside_d: Dimension
    body_w: Dimension
    body_d: Dimension
    body_h: Dimension


@dataclass(frozen=True)
class Placement:
    label: str
    part: str
    face: str
    x: float
    y: float


@dataclass(frozen=True)
class Box:
    label: str
    x0: float
    x1: float
    y0: float
    y1: float
    z0: float
    z1: float


@dataclass(frozen=True)
class Finding:
    check: str
    ok: bool
    detail: str


CASE_FIELDS = {"width": "케이스 폭", "length": "케이스 길이", "height": "케이스 높이",
               "wall": "케이스 벽 두께", "edge_margin": "패널 가장자리 여유"}
PART_FIELDS = {"hole_d": "타공 지름", "outside_d": "바깥 점유 지름", "body_w": "몸체 폭",
               "body_d": "몸체 깊이", "body_h": "패널 아래 높이"}
DRAFT_DEFAULTS = {"case.wall": 3.0, "case.edge_margin": 6.0, "hole_d": 10.0, "outside_d": 14.0,
                  "body_w": 16.0, "body_d": 16.0, "body_h": 30.0}


def resolve(case: Case, parts: dict[str, PartSpec], internal: list[Box], draft: bool) -> tuple[dict[str, float], list[str]]:
    values: dict[str, float] = {}
    missing: list[str] = []
    for field, label in CASE_FIELDS.items():
        dimension: Dimension = getattr(case, field)
        key = f"case.{field}"
        if dimension.value is None:
            missing.append(label)
            if draft:
                values[key] = DRAFT_DEFAULTS[key]
        else:
            values[key] = dimension.value
    for name, spec in parts.items():
        for field, label in PART_FIELDS.items():
            dimension = getattr(spec, field)
            key = f"{name}.{field}"
            if dimension.value is None:
                missing.append(f"{spec.name} {label}")
                if draft:
                    values[key] = DRAFT_DEFAULTS[field]
            else:
                values[key] = dimension.value
    return values, missing


def face_size(case: Case, face: str, values: dict[str, float]) -> tuple[float, float]:
    width, length, height = values["case.width"], values["case.length"], values["case.height"]
    return {"top": (width, length), "in": (length, height), "out": (length, height), "back": (width, height)}[face]


def to_case(case: Case, p: Placement, values: dict[str, float]) -> tuple[float, float, float]:
    width, length, height = values["case.width"], values["case.length"], values["case.height"]
    if p.face == "top":
        return (p.x, p.y, height)
    if p.face == "in":
        return (width / 2, p.x, p.y + height / 2)
    if p.face == "out":
        return (-width / 2, p.x, p.y + height / 2)
    if p.face == "back":
        return (p.x, length / 2, p.y + height / 2)
    raise ValueError(f"면 이름: {p.face}")


def body_box(case: Case, p: Placement, spec: PartSpec, values: dict[str, float]) -> Box:
    x, y, z = to_case(case, p, values)
    wall = values["case.wall"]
    half_w = values[f"{p.part}.body_w"] / 2
    half_d = values[f"{p.part}.body_d"] / 2
    depth = values[f"{p.part}.body_h"]
    if p.face == "top":
        return Box(p.label, x - half_w, x + half_w, y - half_d, y + half_d, z - wall - depth, z - wall)
    if p.face in ("in", "out"):
        sign = 1 if p.face == "in" else -1
        inner = x - sign * wall
        x0, x1 = sorted((inner, inner - sign * depth))
        return Box(p.label, x0, x1, y - half_w, y + half_w, z - half_d, z + half_d)
    inner = y - wall
    return Box(p.label, x - half_w, x + half_w, inner - depth, inner, z - half_d, z + half_d)


def _overlap(a: Box, b: Box) -> bool:
    return a.x0 < b.x1 and b.x0 < a.x1 and a.y0 < b.y1 and b.y0 < a.y1 and a.z0 < b.z1 and b.z0 < a.z1


def check(case: Case, parts: dict[str, PartSpec], placements: list[Placement], internal: list[Box],
          values: dict[str, float]) -> list[Finding]:
    findings: list[Finding] = []
    margin = values["case.edge_margin"]
    for a, b in itertools.combinations(placements, 2):
        if a.face != b.face:
            continue
        distance = math.hypot(a.x - b.x, a.y - b.y)
        gap = distance - (values[f"{a.part}.outside_d"] + values[f"{b.part}.outside_d"]) / 2
        need = KNOB_GAP_MM if a.part == b.part == "knob" else 0.0
        findings.append(Finding("패널 바깥 간섭", gap >= need - 1e-9, f"{a.label}-{b.label} 여유 {gap:.1f}mm (기준 {need:g})"))
    for p in placements:
        width, height = face_size(case, p.face, values)
        radius = values[f"{p.part}.outside_d"] / 2
        room = min(width / 2 - margin - abs(p.x), height / 2 - margin - abs(p.y)) - radius
        findings.append(Finding("가장자리 여유", room >= -1e-9, f"{p.label} 여유 {room:.1f}mm"))
    boxes = [body_box(case, p, parts[p.part], values) for p in placements] + list(internal)
    for a, b in itertools.combinations(boxes, 2):
        findings.append(Finding("패널 안쪽 간섭", not _overlap(a, b), f"{a.label}-{b.label}"))
    floor = values["case.wall"]
    for box in boxes:
        findings.append(Finding("높이 스택", box.z0 >= floor - 1e-9, f"{box.label} 바닥 위 {box.z0 - floor:.1f}mm"))
    return findings
```

프로젝트 데이터(같은 파일 뒤쪽). 값과 출처는 설계서 5.2·5.3절과 판매처 표기다. 확인되지 않은 값은 `None`이다.

```python
def _d(value: float | None, source: str) -> Dimension:
    return Dimension(value, source)


UNMEASURED = "미측정"
CASE = Case(_d(95, "페달파츠 표기"), _d(120, "페달파츠 표기"), _d(56, "페달파츠 표기"),
            _d(None, UNMEASURED), _d(None, UNMEASURED))
PARTS = {
    "knob": PartSpec("노브·포트", _d(7.5, "설계서 5.2절"), _d(18.9, "설계서 5.2절"), _d(16, "설계서 5.2절"),
                     _d(16, "설계서 5.2절"), _d(12, "페달파츠 표기, 패널 안쪽에서 기판까지")),
    "footswitch": PartSpec("풋스위치", _d(12, "설계서 5.2절"), _d(None, UNMEASURED), _d(None, UNMEASURED),
                           _d(None, UNMEASURED), _d(25, "판매처 표기, 패널 아래 약 25mm")),
    "led": PartSpec("LED 3mm", _d(3, "설계서 5.2절"), _d(3, "설계서 5.2절"), _d(3, "설계서 5.2절"),
                    _d(3, "설계서 5.2절"), _d(None, UNMEASURED)),
    "jack": PartSpec("박스형 1/4 잭", _d(9.5, "설계서 5.2절"), _d(None, UNMEASURED), _d(16, "설계서 5.2절"),
                     _d(16, "설계서 5.2절"), _d(None, UNMEASURED)),
    "dc": PartSpec("DC 잭", _d(12, "설계서 5.2절"), _d(None, UNMEASURED), _d(None, UNMEASURED),
                   _d(None, UNMEASURED), _d(None, UNMEASURED)),
    "usb": PartSpec("패널 USB-C", _d(None, UNMEASURED), _d(None, UNMEASURED), _d(None, UNMEASURED),
                    _d(None, UNMEASURED), _d(None, UNMEASURED)),
}
PLACEMENTS = [
    Placement("Input", "knob", "top", -25, 35), Placement("Ratio", "knob", "top", 0, 35),
    Placement("Att/Rel", "knob", "top", 25, 35), Placement("Blend", "knob", "top", -25, 10),
    Placement("Output", "knob", "top", 0, 10),
    *[Placement(f"미터 {i + 1}", "led", "top", 13 + i * 4, 10) for i in range(8)],
    Placement("온 LED", "led", "top", 25, -10),
    Placement("풋스위치", "footswitch", "top", 0, -35),
    Placement("IN", "jack", "in", 25, 5), Placement("OUT", "jack", "out", 25, 5),
    Placement("DC", "dc", "back", -15, 5), Placement("USB", "usb", "back", 15, 5),
]
INTERNAL: list[Box] = []
```

`INTERNAL`(기판 스택)은 Task 8에서 채운다. 첫 배치 좌표는 설계서 5.2절 스케치를 옮긴 초기값이고 Task 9에서 검사
결과를 보고 고친다.

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v`
Expected: 모든 테스트 OK

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/enclosure.py effector/hardware/tests/test_enclosure.py
git commit -m "[Feat] 이펙터 기구 기준 데이터와 패널·내부 간섭 검사"
```

---

### Task 8: 기구 출력과 미측정 차단

**Files:**
- Modify: `effector/hardware/enclosure.py` (SVG, 보고서, CLI, 기판 스택)
- Modify: `effector/hardware/tests/test_enclosure.py`
- Create: `effector/hardware/enclosure/` (출력. `--draft` 결과를 커밋)

**Interfaces:**
- Consumes: Task 7 전부
- Produces:
  - `face_svg(case, face, placements, values, draft: bool) -> str`
  - `board_svg(case, boxes: list[Box], values, board_top: float, draft: bool) -> str`
  - `report(findings, missing, draft) -> str`
  - `main(argv: list[str] | None = None) -> int` (종료 코드. `--final`에서 미측정이 있으면 2, 검사 실패가 있으면 1)
  - `INTERNAL`에 Seed3 소켓 스택 상자

Seed3 소켓 스택은 설계서 5.3절의 약 20mm(스페이서, 만능기판 1.6mm, 소켓 8.51mm, 모듈)와 Seed3 길이 51mm를 쓰고, 폭은
미측정이라 상자 대신 `INTERNAL_SPECS`의 Dimension으로 두어 `resolve`가 다루게 한다.

- [ ] **Step 1: 실패하는 테스트 작성** (`test_enclosure.py`에 추가)

```python
import io
import contextlib
import tempfile


class OutputTest(unittest.TestCase):
    def test_svg_contains_holes_and_draft_mark(self) -> None:
        values, _ = en.resolve(en.CASE, en.PARTS, [], draft=True)
        svg = en.face_svg(en.CASE, "top", en.PLACEMENTS, values, draft=True)
        self.assertTrue(svg.startswith("<svg"))
        self.assertIn("초안", svg)
        self.assertEqual(svg.count("<circle"), sum(1 for p in en.PLACEMENTS if p.face == "top"))
        self.assertIn("Input", svg)

    def test_final_refuses_with_unmeasured(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = en.main(["--final", "--out", directory])
            self.assertEqual(code, 2)
            self.assertIn("미측정", out.getvalue())
            self.assertEqual(list(Path(directory).glob("*.svg")), [])

    def test_draft_writes_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                en.main(["--draft", "--out", directory])
            names = sorted(p.name for p in Path(directory).iterdir())
        self.assertEqual(names, ["back.svg", "board.svg", "in.svg", "out.svg", "report.md", "top.svg"])

    def test_report_lists_failures_first(self) -> None:
        findings = [en.Finding("가장자리 여유", True, "a"), en.Finding("패널 바깥 간섭", False, "b-c")]
        text = en.report(findings, ["케이스 벽 두께"], draft=True)
        self.assertLess(text.index("b-c"), text.index("가장자리 여유"))
        self.assertIn("케이스 벽 두께", text)
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_enclosure.py" -v`
Expected: `AttributeError: module 'enclosure' has no attribute 'face_svg'`

- [ ] **Step 3: 구현** (`enclosure.py`에 추가. `import argparse`를 맨 위 import에 더한다)

```python
SCALE = 4.0  # SVG px/mm
INTERNAL_SPECS = {"seed3": ("Seed3 소켓 스택", _d(None, UNMEASURED), _d(51, "설계서 5.3절, Seed3 길이"),
                            _d(20, "설계서 5.3절, 스택 약 20mm"))}
INTERNAL_POSITION = {"seed3": (-25.0, -25.0)}


def internal_boxes(values: dict[str, float]) -> list[Box]:
    boxes = []
    for key, (label, width, length, height) in INTERNAL_SPECS.items():
        w = width.value if width.value is not None else values.get(f"{key}.width", DRAFT_DEFAULTS["body_w"])
        x, y = INTERNAL_POSITION[key]
        floor = values["case.wall"]
        boxes.append(Box(label, x - w / 2, x + w / 2, y - length.value / 2, y + length.value / 2, floor, floor + height.value))
    return boxes


def internal_missing() -> list[str]:
    return [f"{label} 폭" for label, width, _, _ in INTERNAL_SPECS.values() if width.value is None]


def face_svg(case: Case, face: str, placements: list[Placement], values: dict[str, float], draft: bool) -> str:
    width, height = face_size(case, face, values)
    w, h = width * SCALE, height * SCALE
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w + 40:.0f}" height="{h + 80:.0f}" '
             f'viewBox="-20 -20 {w + 40:.0f} {h + 80:.0f}" font-family="sans-serif" font-size="10">',
             f'<rect x="0" y="0" width="{w:.1f}" height="{h:.1f}" fill="none" stroke="black"/>']
    if draft:
        parts.append(f'<text x="{w / 2:.1f}" y="{h / 2:.1f}" font-size="28" fill="#c00" opacity="0.3" '
                     f'text-anchor="middle">초안</text>')
    rows = []
    for p in (p for p in placements if p.face == face):
        cx, cy = w / 2 + p.x * SCALE, h / 2 - p.y * SCALE
        hole = values[f"{p.part}.hole_d"]
        parts.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{hole / 2 * SCALE:.1f}" fill="none" stroke="blue"/>')
        parts.append(f'<line x1="{cx - 4:.1f}" y1="{cy:.1f}" x2="{cx + 4:.1f}" y2="{cy:.1f}" stroke="blue"/>'
                     f'<line x1="{cx:.1f}" y1="{cy - 4:.1f}" x2="{cx:.1f}" y2="{cy + 4:.1f}" stroke="blue"/>')
        parts.append(f'<text x="{cx + 6:.1f}" y="{cy - 6:.1f}">{p.label}</text>')
        rows.append(f"{p.label}: x {p.x:+.1f}, y {p.y:+.1f}, 지름 {hole:g}")
    for i, row in enumerate(rows):
        parts.append(f'<text x="0" y="{h + 16 + i * 12:.1f}">{row}</text>')
    parts.append("</svg>")
    svg = "\n".join(parts) + "\n"
    return svg.replace(f'height="{h + 80:.0f}"', f'height="{h + 40 + 12 * len(rows):.0f}"', 1)


def board_svg(case: Case, boxes: list[Box], values: dict[str, float], board_top: float, draft: bool) -> str:
    wall = values["case.wall"]
    width, length = values["case.width"] - 2 * wall, values["case.length"] - 2 * wall
    w, h = width * SCALE, length * SCALE
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w + 40:.0f}" height="{h + 40:.0f}" '
             f'viewBox="-20 -20 {w + 40:.0f} {h + 40:.0f}" font-family="sans-serif" font-size="10">',
             f'<rect x="0" y="0" width="{w:.1f}" height="{h:.1f}" fill="#efe" stroke="black"/>']
    if draft:
        parts.append(f'<text x="{w / 2:.1f}" y="{h / 2:.1f}" font-size="28" fill="#c00" opacity="0.3" '
                     f'text-anchor="middle">초안</text>')
    for box in boxes:
        if box.z0 < board_top:
            x = (box.x0 + width / 2) * SCALE
            y = (length / 2 - box.y1) * SCALE
            parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{(box.x1 - box.x0) * SCALE:.1f}" '
                         f'height="{(box.y1 - box.y0) * SCALE:.1f}" fill="#fcc" stroke="#c00"/>'
                         f'<text x="{x + 2:.1f}" y="{y + 10:.1f}">{box.label}</text>')
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def report(findings: list[Finding], missing: list[str], draft: bool) -> str:
    lines = ["# 기구 검사 보고", "", "초안. 미측정 값을 자리표시 값으로 채웠다." if draft else "최종.", ""]
    failed = [f for f in findings if not f.ok]
    lines += [f"실패 {len(failed)}건, 통과 {len(findings) - len(failed)}건", "", "| 검사 | 결과 | 내용 |", "|---|---|---|"]
    for finding in failed + [f for f in findings if f.ok]:
        lines.append(f"| {finding.check} | {'통과' if finding.ok else '실패'} | {finding.detail} |")
    if missing:
        lines += ["", "## 미측정", ""] + [f"- {name}" for name in missing]
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--draft", action="store_true")
    mode.add_argument("--final", action="store_true")
    parser.add_argument("--out", default=str(Path(__file__).resolve().parent / "enclosure"))
    args = parser.parse_args(argv)
    values, missing = resolve(CASE, PARTS, [], draft=args.draft)
    missing += internal_missing()
    if args.final and missing:
        print("미측정 값이 있어 최종 도면을 내지 않는다:")
        for name in missing:
            print(f"- {name}")
        return 2
    internal = internal_boxes(values)
    findings = check(CASE, PARTS, PLACEMENTS, internal, values)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for face in FACES:
        (out / f"{face}.svg").write_text(face_svg(CASE, face, PLACEMENTS, values, args.draft), encoding="utf-8")
    boxes = [body_box(CASE, p, PARTS[p.part], values) for p in PLACEMENTS] + internal
    board_top = values["case.wall"] + INTERNAL_SPECS["seed3"][3].value
    (out / "board.svg").write_text(board_svg(CASE, boxes, values, board_top, args.draft), encoding="utf-8")
    (out / "report.md").write_text(report(findings, missing, args.draft), encoding="utf-8")
    failed = sum(1 for f in findings if not f.ok)
    print(f"{out}: 검사 실패 {failed}건, 미측정 {len(missing)}건")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

`INTERNAL`(Task 7 상수)은 쓰지 않게 되므로 지우고, Task 7 테스트 `test_project_data_resolves_in_draft`를 다음으로 바꾼다:

```python
    def test_project_data_resolves_in_draft(self) -> None:
        values, missing = en.resolve(en.CASE, en.PARTS, [], draft=True)
        self.assertTrue(missing)
        en.check(en.CASE, en.PARTS, en.PLACEMENTS, en.internal_boxes(values), values)
```

- [ ] **Step 4: 통과 확인과 초안 생성**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v && python3 effector/hardware/enclosure.py --draft`
Expected: 모든 테스트 OK. `effector/hardware/enclosure/`에 SVG 5개와 report.md. 검사 실패가 있으면 종료 코드 1(실패 내용은 Task 9에서 다룬다)

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/enclosure.py effector/hardware/tests/test_enclosure.py effector/hardware/enclosure/
git commit -m "[Feat] 이펙터 타공 도면·기판 영역 SVG와 미측정 값 최종 도면 차단"
```

---

### Task 9: 결과 반영

**Files:**
- Modify: `effector/hardware/enclosure.py` (PLACEMENTS 좌표)
- Modify: `devices/bass-effector-schematic.md` (12절)
- Modify: `devices/bass-effector-design.md` (4.5절, 5.2절)
- Modify: `devices/bass-effector-tools-design.md` (5.3절 기판 가용 영역 서술을 구현에 맞춤)
- Modify: `effector/README.md` (도구 실행법)

**Interfaces:**
- Consumes: Task 3 테스트벤치 결과, Task 5 예산표, Task 6 요약표, Task 8 보고서

- [ ] **Step 1: 전체 테스트와 도구 실행**

Run:
```bash
python3 effector/hardware/gen_spice.py
python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v
for f in protection doubler output_stage boot dc_polarity; do (cd effector/hardware/spice && ngspice -b $f.cir 2>&1 | grep -E '^[a-z_0-9]+ += '); done
python3 effector/hardware/spice/noise_budget.py
python3 effector/hardware/spice/monte_carlo.py --count 200 --seed 1
python3 effector/hardware/enclosure.py --draft
```
Expected: 테스트 전부 OK(cable_ti skip 가능), 나머지 출력은 아래 반영에 쓴다

- [ ] **Step 2: 패널 첫 배치 확정**

`effector/hardware/enclosure/report.md`의 실패를 본다. 실패가 패널 바깥 간섭이나 가장자리 여유면 PLACEMENTS 좌표를 고쳐 다시
돌린다. 기준: 노브 두 줄 간격 25mm 유지, 풋스위치는 앞쪽 끝, LED는 노브와 겹치지 않는 곳. 초안 자리표시 값에 기댄 실패
(미측정 부품 몸체)는 좌표로 풀지 않고 보고에 남긴다. 고친 좌표를 설계서 5.2절에 표로 옮긴다(부품, 면, x, y, 출처
"enclosure.py 초안").

- [ ] **Step 3: 회로 설계 12절 갱신**

12절 머리 문장의 ngspice 버전을 47(M4, Homebrew)로, 실행 날짜를 갱신한다. 표 값은 이전한 테스트벤치 결과로 바꾸고,
이전으로 달라진 값은 차이와 원인(Vbias 분압 부하, D14)을 한 줄로 적는다. 잡음 단락을 다음으로 바꾼다: 잡음 모델
설명 한 문단, `noise_budget.py` 예산표(칩 값 행과 -10dB 행, en 두 값), 부품별 기여 상위표, 입력·출력 중 지배 쪽
결론. 몬테카를로 요약표를 새 소절로 더하고 LT1054 온저항 범위가 만든 손실 범위를 적는다. 판정 미달이 있으면 고칠
블록·부품·값과 근거를 적되 BLOCKS는 고치지 않는다.

- [ ] **Step 4: 설계서 4.5절 갱신**

출력 쪽이 바닥을 정하는지 결론을 4.5절 마지막 단락에 반영한다. 출력 쪽이 지배하면 출력단 잡음의 주 기여 소자와
그 값이 줄었을 때 효과(예: 기여 상위 소자)를 근거로 적는다. 회로 변경 제안은 "회로 설계자 검토 대상"으로 표시한다.

- [ ] **Step 5: 도구 설계서와 README**

도구 설계서 5.3절의 "기판 가용 영역" 서술을 구현(부품 몸체 투영을 표시한 SVG)에 맞추고, 6절 실행 명령을 실제
명령(`-s ... -t ...`)으로 고친다. 계획에서 설계서와 다르게 정한 세 가지도 설계서에 반영한다. 4.1절 1/f 코너 대신 백색 en 두 값(7n,
4.4n)으로 범위를 낸다. 3.4절 LT1054 손실 대신 온저항 20~40Ω을 흔들고 결과 손실 범위를 적는다. 4.2절 픽업 등가는
출처 있는 대표값이 아니라 가정값(7kΩ, 3H, 600pF, 부하 125kΩ)이고 사용자 베이스 실측 전이다. `effector/README.md` 구성 표에 `gen_spice.py`, `ngspice_run.py`, `enclosure.py`,
`hardware/tests/`를 더하고 "설계 검증 도구" 절에 Step 1의 명령을 적는다.

- [ ] **Step 6: 커밋**

```bash
git add -A
git commit -m "[Docs] 이펙터 설계 검증 도구 결과를 회로 설계 12절과 설계서에 반영"
```

## 실행 기록

계획과 다르게 구현한 것은 여기에 적는다.
