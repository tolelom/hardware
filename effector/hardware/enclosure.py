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
