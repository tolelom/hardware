"""CNC 타공용 최소 DXF(R12, ENTITIES 구역만). 단위 mm, 면 중심이 원점.

엔티티: ("circle", cx, cy, r, layer), ("rect", cx, cy, w, h, layer)는 LINE 4개, ("text", x, y, h, s, layer).
레이어: CUT(타공), OUTLINE(면 외곽), NOTE(좌표 글자).
"""
from __future__ import annotations


def _n(value: float) -> str:
    return f"{value:.4f}"


def _pairs(*items: tuple[int, str]) -> list[str]:
    out = []
    for code, value in items:
        out += [str(code), value]
    return out


def _line(x1: float, y1: float, x2: float, y2: float, layer: str) -> list[str]:
    return _pairs((0, "LINE"), (8, layer), (10, _n(x1)), (20, _n(y1)), (11, _n(x2)), (21, _n(y2)))


def document(entities: list[tuple]) -> str:
    lines = _pairs((0, "SECTION"), (2, "ENTITIES"))
    for entity in entities:
        kind = entity[0]
        if kind == "circle":
            _, cx, cy, r, layer = entity
            lines += _pairs((0, "CIRCLE"), (8, layer), (10, _n(cx)), (20, _n(cy)), (40, _n(r)))
        elif kind == "rect":
            _, cx, cy, w, h, layer = entity
            x0, x1, y0, y1 = cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2
            lines += _line(x0, y0, x1, y0, layer) + _line(x1, y0, x1, y1, layer)
            lines += _line(x1, y1, x0, y1, layer) + _line(x0, y1, x0, y0, layer)
        elif kind == "text":
            _, x, y, height, text, layer = entity
            lines += _pairs((0, "TEXT"), (8, layer), (10, _n(x)), (20, _n(y)), (40, _n(height)), (1, text))
        else:
            raise ValueError(f"DXF 엔티티 종류: {kind}")
    lines += _pairs((0, "ENDSEC"), (0, "EOF"))
    return "\n".join(lines) + "\n"


def parse(text: str) -> list[tuple]:
    raw = text.splitlines()
    pairs = [(int(raw[i].strip()), raw[i + 1]) for i in range(0, len(raw) - 1, 2)]
    entities, current = [], None
    for code, value in pairs + [(0, "EOF")]:
        if code == 0:
            if current and current[0] in ("CIRCLE", "LINE", "TEXT"):
                kind, fields = current
                layer = fields.get(8, "")
                if kind == "CIRCLE":
                    entities.append((kind, layer, (float(fields[10]), float(fields[20]), float(fields[40]))))
                elif kind == "LINE":
                    entities.append((kind, layer, tuple(float(fields[c]) for c in (10, 20, 11, 21))))
                else:
                    entities.append((kind, layer, (float(fields[10]), float(fields[20]), float(fields[40]), fields[1])))
            current = (value, {})
        elif current is not None:
            current[1][code] = value
    return entities
