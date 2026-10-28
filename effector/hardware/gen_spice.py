"""gen_schematic.py의 BLOCKS에서 블록별 SPICE 넷리스트를 만든다.

    python3 effector/hardware/gen_spice.py

spice/generated/에 block_<문자>.cir와 부분 묶음(SUBSETS)을 쓴다. 생성물은 손으로 고치지 않는다.
설계 검증 도구 설계서 3절.
"""
from __future__ import annotations

import hashlib
import io
import re
from pathlib import Path

from gen_schematic import BLOCKS, Block, Part

SKIPPED = {
    ("Connector", "Barrel_Jack"),
    ("Connector_Audio", "AudioJack2"),
    ("Connector_Audio", "AudioJack3"),
    ("Connector_Generic", "Conn_01x40"),
    ("Connector_Generic", "Conn_01x08"),
    ("Connector_Generic", "Conn_02x10_Odd_Even"),
    ("Connector", "Micro_SD_Card"),
    ("Device", "RotaryEncoder_Switch"),
    ("Interface_Expansion", "MCP23017_SO"),
    ("Isolator", "H11L1"),
    ("Regulator_Switching", "TPS562200"),
    ("Switch", "SW_Push"),
    ("power", "PWR_FLAG"),
}
DIODE_MODELS = {"1N4148": "D1N4148", "1N5819": "D1N5819", "1N4742A": "DZ12", "1N4744A": "DZ15"}
BJT_MODELS = {"2N3904": "Q2N3904", "2N3906": "Q2N3906"}
SUFFIXES = {"": 1.0, "R": 1.0, "k": 1e3, "K": 1e3, "M": 1e6, "m": 1e-3, "u": 1e-6, "n": 1e-9, "p": 1e-12}
RELAY_COIL_OHM = 579.0
CONTACT_CLOSED_OHM = 0.05
CONTACT_OPEN_OHM = 1e12
FERRITE_DC_OHM = 0.05
GENERATED = Path(__file__).resolve().parent / "spice" / "generated"
SUBSETS = {"vbias": ["R11", "R12", "C6"]}


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
    if kind == ("Device", "L"):
        return [f"L{ref} {nets['1']} {nets['2']} {fmt(parse_value(part.value) * scale)}"]
    if kind == ("Device", "FerriteBead"):
        return [f"R{ref} {nets['1']} {nets['2']} {fmt(FERRITE_DC_OHM)}"]
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


def blocks_hash(blocks: list[Block] | None = None) -> str:
    return hashlib.sha256(repr(BLOCKS if blocks is None else blocks).encode("utf-8")).hexdigest()[:16]


def block_letter(title: str) -> str:
    match = re.match(r"(?:\[[^]]+\] )?([A-Z][0-9]?)\.", title)
    if not match:
        raise SpiceError(f"블록 제목에 문자가 없음: {title!r}")
    return match.group(1)


def emit(title: str, parts: list[Part], scales: dict[str, float] | None = None,
         blocks: list[Block] | None = None) -> str:
    blocks = BLOCKS if blocks is None else blocks
    power = opamp_power(blocks)
    scales = scales or {}
    lines = ["* 자동 생성: gen_spice.py. 손으로 고치지 않는다.", f"* {title}", f"* BLOCKS {blocks_hash(blocks)}"]
    for part in parts:
        lines += part_lines(part, power, scales.get(part.ref, 1.0))
    return "\n".join(lines) + "\n"


def generate(out: Path = GENERATED, scales: dict[str, float] | None = None,
             blocks: list[Block] | None = None, prefix: str = "block") -> list[Path]:
    default = blocks is None
    blocks = BLOCKS if default else blocks
    out.mkdir(parents=True, exist_ok=True)
    written = []
    by_ref = {p.ref: p for b in blocks for p in b.parts if p.unit == 1}
    targets = [(f"{prefix}_{block_letter(b.title)}.cir", b.title, b.parts) for b in blocks]
    if default:
        targets += [(f"{name}.cir", f"부분 묶음 {name}: {', '.join(refs)}. block_B와 같이 include하지 않는다",
                     [by_ref[r] for r in refs]) for name, refs in SUBSETS.items()]
    for filename, title, parts in targets:
        path = out / filename
        with io.open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(emit(title, parts, scales, blocks))
        written.append(path)
    return written


def main() -> None:
    for path in generate():
        print(path)


if __name__ == "__main__":
    main()
