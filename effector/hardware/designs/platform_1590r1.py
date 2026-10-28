"""멀티 이펙터 플랫폼 패널(F4, 왼손형, 탑 잭). 하드웨어 플랫폼 설계서 3절.

면 좌표는 면 중심 기준 mm, x는 바깥에서 볼 때 오른쪽, y는 위(윗면은 뒷면 쪽)다.
출처가 "대표값"인 치수는 흔한 부품의 대표 치수라 --final이 미측정과 같이 막는다.
"""
from __future__ import annotations

from enclosure import Box, Case, Design, Dimension, PartSpec, Placement

REP = "대표값"


def _d(value: float | None, source: str) -> Dimension:
    return Dimension(value, source)


CASE = Case(_d(192, "Hammond 1590R1, 판매점 DigiKey"), _d(111.25, "Hammond 1590R1, 판매점 DigiKey"),
            _d(60.76, "Hammond 1590R1, 판매점 DigiKey"), _d(2.03, "Hammond 1590R1, 판매점 DigiKey"),
            _d(None, "미측정"))

PARTS = {
    "screen": PartSpec("화면 3.5\"", _d(0, "사각 타공"), _d(0, "사각"), _d(82.94, "Orient AFK320480A0 외형"),
                       _d(54.66, "Orient AFK320480A0 외형"), _d(7.74, "Orient AFK320480A0 두께"),
                       cutout=(_d(75.4, "보이는 영역 74.4 + 1mm, EA TFT035 기준"), _d(51.0, "보이는 영역 50 + 1mm, EA TFT035 기준")),
                       outside_rect=(_d(75.4, "타공과 같음"), _d(51.0, "타공과 같음"))),
    "enc": PartSpec("인코더", _d(7.5, f"{REP}, EC11 부싱"), _d(15, f"{REP}, 노브"), _d(12.5, f"{REP}, EC11"),
                    _d(13.4, f"{REP}, EC11"), _d(20, f"{REP}, EC11 몸체와 조작부 기판")),
    "nav": PartSpec("NAV 인코더", _d(7.5, f"{REP}, EC11 부싱"), _d(20, f"{REP}, 노브"), _d(12.5, f"{REP}, EC11"),
                    _d(13.4, f"{REP}, EC11"), _d(20, f"{REP}, EC11 몸체와 조작부 기판")),
    "fs": PartSpec("풋스위치", _d(12, "1호 설계서 5.2절"), _d(20, f"{REP}, 와셔"), _d(None, "미측정"),
                   _d(None, "미측정"), _d(25, "판매처 표기, 패널 아래 약 25mm")),
    "led": PartSpec("LED 3mm", _d(3, "1호 설계서 5.2절"), _d(7, f"{REP}, 베젤"), _d(3, "1호 설계서 5.2절"),
                    _d(3, "1호 설계서 5.2절"), _d(None, "미측정")),
    "trs14": PartSpec("1/4\" TRS 잭", _d(9.5, "1호 설계서 5.2절"), _d(14, f"{REP}, 너트"), _d(16, "1호 설계서 5.2절"),
                      _d(16, "1호 설계서 5.2절"), _d(30, f"{REP}, 잭 몸체 길이")),
    "trs35": PartSpec("3.5mm TRS 잭", _d(6, f"{REP}"), _d(8, f"{REP}, 너트"), _d(8, f"{REP}"),
                      _d(10, f"{REP}"), _d(15, f"{REP}, 잭 몸체 길이")),
    "usb": PartSpec("USB-C", _d(0, "사각 타공"), _d(0, "사각"), _d(None, "미측정"), _d(None, "미측정"),
                    _d(None, "미측정"), cutout=(_d(9.5, REP), _d(3.8, REP)), outside_rect=(_d(12, REP), _d(7.2, REP))),
    "dc": PartSpec("DC 잭", _d(12, "1호 설계서 5.2절"), _d(14, f"{REP}, 너트"), _d(None, "미측정"),
                   _d(None, "미측정"), _d(None, "미측정")),
}

PLACEMENTS = [
    Placement("화면", "screen", "top", 46.55, 20.28),
    *[Placement(f"E{i + 1}", "enc", "top", x, y) for i, (x, y) in
      enumerate([(-54, 34.63), (-31, 34.63), (-8, 34.63), (-54, 11.63), (-31, 11.63), (-8, 11.63)])],
    Placement("NAV", "nav", "top", -79, 33.63),
    *[Placement(f"LED{i + 1}", "led", "top", x, -18.38) for i, x in enumerate((-60, 0, 60))],
    *[Placement(f"FS{i + 1}", "fs", "top", x, -34.38) for i, x in enumerate((-60, 0, 60))],
    Placement("OUT", "trs14", "back", 76, 0), Placement("IN", "trs14", "back", 52, 0),
    Placement("EXP", "trs14", "back", 26, 0), Placement("MIDI OUT", "trs35", "back", 3, 0),
    Placement("MIDI IN", "trs35", "back", -17, 0), Placement("USB-C", "usb", "back", -44, 0),
    Placement("DC 9V", "dc", "back", -72, 0),
]


def internal(values: dict[str, float]) -> list[Box]:
    floor = values["case.wall"]
    return [Box("Seed3 소켓 스택", 40, 80, -15, 30, floor, floor + 20)]


def missing_extra() -> list[str]:
    return ["Seed3 소켓 스택 위치 (초안)"]


DESIGN = Design("platform_1590r1", CASE, PARTS, PLACEMENTS, internal, missing_extra)
