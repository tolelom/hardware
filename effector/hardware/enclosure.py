"""케이스 기구 기준과 검사·타공 도면. 설계 검증 도구 설계서 5절.

    python3 effector/hardware/enclosure.py --draft   미측정 값을 자리표시로 채운 초안
    python3 effector/hardware/enclosure.py --final   미측정 값이 있으면 실패
"""
from __future__ import annotations

import argparse
import itertools
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

KNOB_GAP_MM = 6.1
FACES = ("top", "in", "out", "back")
# 면 좌표 x는 그 면을 바깥에서 볼 때 오른쪽, y는 위(윗면은 DC 잭 쪽 끝)다. 도면은 보는 그대로 그린다
FACE_TITLES = {"top": "윗면", "in": "입력면(+X)", "out": "출력면(-X)", "back": "뒷면(+Y, DC 잭 쪽)"}


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
    cutout: tuple[Dimension, Dimension] | None = None
    outside_rect: tuple[Dimension, Dimension] | None = None


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
    margin: float | None = None


@dataclass(frozen=True)
class Design:
    name: str
    case: "Case"
    parts: dict
    placements: list
    internal: Callable[[dict[str, float]], list["Box"]]
    missing_extra: Callable[[], list[str]] = lambda: []


CASE_FIELDS = {"width": "케이스 폭", "length": "케이스 길이", "height": "케이스 높이",
               "wall": "케이스 벽 두께", "edge_margin": "패널 가장자리 여유"}
PART_FIELDS = {"hole_d": "타공 지름", "outside_d": "바깥 점유 지름", "body_w": "몸체 폭",
               "body_d": "몸체 깊이", "body_h": "패널 아래 높이"}
DRAFT_DEFAULTS = {"case.wall": 3.0, "case.edge_margin": 6.0, "hole_d": 10.0, "outside_d": 14.0,
                  "body_w": 16.0, "body_d": 16.0, "body_h": 30.0, "cutout_w": 10.0, "cutout_h": 10.0,
                  "outside_w": 10.0, "outside_h": 10.0}
REPRESENTATIVE = "대표값"
RECT_FIELDS = {"cutout": ("사각 타공 폭", "사각 타공 높이"), "outside_rect": ("바깥 점유 폭", "바깥 점유 높이")}
RECT_KEYS = {"cutout": ("cutout_w", "cutout_h"), "outside_rect": ("outside_w", "outside_h")}


def _take(values: dict[str, float], missing: list[str], key: str, default_key: str, dimension: Dimension,
          label: str, draft: bool) -> None:
    if dimension.value is None:
        missing.append(label)
        if draft:
            values[key] = DRAFT_DEFAULTS[default_key]
        return
    values[key] = dimension.value
    if dimension.source.startswith(REPRESENTATIVE):
        missing.append(f"{label} (대표값)")


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
            if spec.cutout is not None and field in ("hole_d",):
                continue
            if spec.outside_rect is not None and field in ("outside_d",):
                continue
            _take(values, missing, f"{name}.{field}", field, getattr(spec, field), f"{spec.name} {label}", draft)
        for field, labels in RECT_FIELDS.items():
            pair = getattr(spec, field)
            if pair is None:
                continue
            for dimension, key, label in zip(pair, RECT_KEYS[field], labels):
                _take(values, missing, f"{name}.{key}", key, dimension, f"{spec.name} {label}", draft)
    return values, missing


def outside_shape(spec: PartSpec, part: str, values: dict[str, float]) -> tuple[float, float, bool]:
    """(반폭, 반높이, 원 여부). 원이면 반폭=반높이=반지름."""
    if spec.outside_rect is not None:
        return values[f"{part}.outside_w"] / 2, values[f"{part}.outside_h"] / 2, False
    r = values[f"{part}.outside_d"] / 2
    return r, r, True


def _gap(a: Placement, sa: tuple[float, float, bool], b: Placement, sb: tuple[float, float, bool]) -> float:
    dx, dy = abs(a.x - b.x), abs(a.y - b.y)
    if sa[2] and sb[2]:
        return math.hypot(dx, dy) - sa[0] - sb[0]
    if sa[2] or sb[2]:
        circle, rect = (sa, sb) if sa[2] else (sb, sa)
        ox, oy = max(dx - rect[0], 0.0), max(dy - rect[1], 0.0)
        if ox == 0.0 and oy == 0.0:
            return -min(rect[0] - dx, rect[1] - dy) - circle[0]
        return math.hypot(ox, oy) - circle[0]
    ox, oy = dx - sa[0] - sb[0], dy - sa[1] - sb[1]
    if ox < 0 and oy < 0:
        return max(ox, oy)
    return math.hypot(max(ox, 0.0), max(oy, 0.0))


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
        return (-width / 2, -p.x, p.y + height / 2)
    if p.face == "back":
        return (-p.x, length / 2, p.y + height / 2)
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
        gap = _gap(a, outside_shape(parts[a.part], a.part, values), b, outside_shape(parts[b.part], b.part, values))
        need = KNOB_GAP_MM if a.part == b.part == "knob" else 0.0
        findings.append(Finding("패널 바깥 간섭", gap >= need - 1e-9, f"{a.label}-{b.label} 여유 {gap:.1f}mm (기준 {need:g})",
                                gap - need))
    for p in placements:
        width, height = face_size(case, p.face, values)
        half_w, half_h, _ = outside_shape(parts[p.part], p.part, values)
        room = min(width / 2 - margin - abs(p.x) - half_w, height / 2 - margin - abs(p.y) - half_h)
        findings.append(Finding("가장자리 여유", room >= -1e-9, f"{p.label} 여유 {room:.1f}mm", room))
    boxes = [body_box(case, p, parts[p.part], values) for p in placements] + list(internal)
    for a, b in itertools.combinations(boxes, 2):
        findings.append(Finding("패널 안쪽 간섭", not _overlap(a, b), f"{a.label}-{b.label}"))
    floor = values["case.wall"]
    for box in boxes:
        findings.append(Finding("높이 스택", box.z0 >= floor - 1e-9, f"{box.label} 바닥 위 {box.z0 - floor:.1f}mm",
                                box.z0 - floor))
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
    *[Placement(f"미터 {i + 1}", "led", "top", -14 + i * 4, -12) for i in range(8)],
    Placement("온 LED", "led", "top", 25, -12),
    Placement("풋스위치", "footswitch", "top", 0, -35),
    Placement("IN", "jack", "in", 15, 5), Placement("OUT", "jack", "out", -15, 5),
    Placement("DC", "dc", "back", 15, 5), Placement("USB", "usb", "back", -15, 5),
]


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


def face_svg(case: Case, face: str, placements: list[Placement], values: dict[str, float], draft: bool,
             specs: dict[str, PartSpec] | None = None) -> str:
    width, height = face_size(case, face, values)
    w, h = width * SCALE, height * SCALE
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w + 40:.0f}" height="{h + 80:.0f}" '
             f'viewBox="-20 -20 {w + 40:.0f} {h + 80:.0f}" font-family="sans-serif" font-size="10">',
             f'<rect x="0" y="0" width="{w:.1f}" height="{h:.1f}" fill="none" stroke="black"/>']
    if draft:
        parts.append(f'<text x="{w / 2:.1f}" y="{h / 2:.1f}" font-size="28" fill="#c00" opacity="0.3" '
                     f'text-anchor="middle">초안</text>')
    parts.append(f'<text x="0" y="-6">{FACE_TITLES[face]}. 바깥에서 본 면, 중심 기준 mm</text>')
    rows = []
    for p in (p for p in placements if p.face == face):
        cx, cy = w / 2 + p.x * SCALE, h / 2 - p.y * SCALE
        if specs is not None and specs[p.part].cutout is not None:
            cw, ch = values[f"{p.part}.cutout_w"], values[f"{p.part}.cutout_h"]
            parts.append(f'<rect x="{cx - cw / 2 * SCALE:.1f}" y="{cy - ch / 2 * SCALE:.1f}" width="{cw * SCALE:.1f}" '
                         f'height="{ch * SCALE:.1f}" fill="none" stroke="blue"/>')
            size = f"사각 {cw:g} × {ch:g}"
        else:
            hole = values[f"{p.part}.hole_d"]
            parts.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{hole / 2 * SCALE:.1f}" fill="none" stroke="blue"/>')
            size = f"지름 {hole:g}"
        parts.append(f'<line x1="{cx - 4:.1f}" y1="{cy:.1f}" x2="{cx + 4:.1f}" y2="{cy:.1f}" stroke="blue"/>'
                     f'<line x1="{cx:.1f}" y1="{cy - 4:.1f}" x2="{cx:.1f}" y2="{cy + 4:.1f}" stroke="blue"/>')
        parts.append(f'<text x="{cx + 6:.1f}" y="{cy - 6:.1f}">{p.label}</text>')
        rows.append(f"{p.label}: x {p.x:+.1f}, y {p.y:+.1f}, {size}")
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
    lines += [f"실패 {len(failed)}건, 통과 {len(findings) - len(failed)}건. 통과는 검사별로 여유가 가장 작은 것만 적는다",
              "", "| 검사 | 결과 | 내용 |", "|---|---|---|"]
    tightest: dict[str, Finding] = {}
    for finding in (f for f in findings if f.ok):
        current = tightest.get(finding.check)
        if current is None or (finding.margin is not None and (current.margin is None or finding.margin < current.margin)):
            tightest[finding.check] = finding
    for finding in failed + list(tightest.values()):
        lines.append(f"| {finding.check} | {'통과' if finding.ok else '실패'} | {finding.detail} |")
    if missing:
        lines += ["", "## 미측정", ""] + [f"- {name}" for name in missing]
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--draft", action="store_true")
    mode.add_argument("--final", action="store_true")
    parser.add_argument("--design", choices=("compressor_1590c", "platform_1590r1", "page_pedal_1590bb"), default="compressor_1590c")
    parser.add_argument("--out", default=None)
    parser.add_argument("--dxf", action="store_true", help="면마다 CNC용 DXF도 쓴다")
    args = parser.parse_args(argv)
    if args.design == "platform_1590r1":
        from designs.platform_1590r1 import DESIGN
        return _run_design(DESIGN, args)
    if args.design == "page_pedal_1590bb":
        from designs.page_pedal_1590bb import DESIGN
        return _run_design(DESIGN, args)
    args.out = args.out or str(Path(__file__).resolve().parent / "enclosure")
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


def face_dxf(case: Case, face: str, placements: list[Placement], values: dict[str, float],
             specs: dict[str, PartSpec]) -> str:
    import dxf
    width, height = face_size(case, face, values)
    entities: list[tuple] = [("rect", 0.0, 0.0, width, height, "OUTLINE")]
    for p in (p for p in placements if p.face == face):
        if specs[p.part].cutout is not None:
            entities.append(("rect", p.x, p.y, values[f"{p.part}.cutout_w"], values[f"{p.part}.cutout_h"], "CUT"))
        else:
            entities.append(("circle", p.x, p.y, values[f"{p.part}.hole_d"] / 2, "CUT"))
        label = p.label if p.label.isascii() else p.part
        entities.append(("text", p.x + 1.0, p.y + 1.0, 2.0, f"{label} ({p.x:+.2f}, {p.y:+.2f})", "NOTE"))
    return dxf.document(entities)


def _run_design(design: Design, args: argparse.Namespace) -> int:
    values, missing = resolve(design.case, design.parts, [], draft=args.draft)
    missing += design.missing_extra()
    if args.final and missing:
        print("미측정 값이 있어 최종 도면을 내지 않는다:")
        for name in missing:
            print(f"- {name}")
        return 2
    internal = design.internal(values)
    findings = check(design.case, design.parts, design.placements, internal, values)
    out = Path(args.out or Path(__file__).resolve().parent / "enclosure" / design.name)
    out.mkdir(parents=True, exist_ok=True)
    for face in FACES:
        if any(p.face == face for p in design.placements):
            (out / f"{face}.svg").write_text(face_svg(design.case, face, design.placements, values, args.draft, design.parts),
                                             encoding="utf-8")
            if args.dxf:
                (out / f"{face}.dxf").write_text(face_dxf(design.case, face, design.placements, values, design.parts),
                                                 encoding="utf-8")
    (out / "report.md").write_text(report(findings, missing, args.draft), encoding="utf-8")
    failed = sum(1 for f in findings if not f.ok)
    print(f"{out}: 검사 실패 {failed}건, 미측정 {len(missing)}건")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
