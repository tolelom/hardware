"""악보 페달(발판 2개, XIAO nRF52840, 블루투스·USB, TRS 겸용). 악보 페달 설계서 3절.

면 좌표는 면 중심 기준 mm, x는 바깥에서 볼 때 오른쪽, y는 위(윗면은 뒷면 쪽)다.
발판은 연주자 쪽(앞)에 두고 케이블(USB-C, TRS)은 모두 뒷면으로 낸다.
출처가 "대표값"인 치수는 --final이 미측정과 같이 막는다.
"""
from __future__ import annotations

from enclosure import Box, Case, Design, Dimension, PartSpec, Placement

REP = "대표값"


def _d(value: float | None, source: str) -> Dimension:
    return Dimension(value, source)


CASE = Case(_d(119.5, "Hammond 1590BB, 판매점 표기"), _d(93.9, "Hammond 1590BB, 판매점 표기"),
            _d(34.0, "Hammond 1590BB 뚜껑 포함, 판매점 표기"), _d(None, "미측정"), _d(None, "미측정"))

PARTS = {
    "fs": PartSpec("풋스위치 SF12011F", _d(12, "SF12011F M12 나사"), _d(20, f"{REP}, 와셔"),
                   _d(16, f"{REP}, 몸체"), _d(16, f"{REP}, 몸체"), _d(25, f"{REP}, 패널 아래 몸체와 단자")),
    "led": PartSpec("키 세트 LED 창", _d(3, f"{REP}, 라이트 파이프"), _d(5, f"{REP}, 라이트 파이프 머리"),
                    _d(3, f"{REP}"), _d(3, f"{REP}"), _d(8, f"{REP}, 라이트 파이프 길이")),
    "trs": PartSpec("6.35mm 스테레오 잭", _d(9.5, "1호 설계서 5.2절"), _d(14, f"{REP}, 너트"),
                    _d(16, "1호 설계서 5.2절"), _d(16, "1호 설계서 5.2절"), _d(30, f"{REP}, 잭 몸체 길이")),
    "usb": PartSpec("XIAO USB-C", _d(0, "사각 타공"), _d(0, "사각"), _d(17.5, "Seeed XIAO 21 x 17.5mm"),
                    _d(4.5, f"{REP}, 기판과 커넥터 높이"), _d(21, "Seeed XIAO 21 x 17.5mm"),
                    cutout=(_d(9.5, REP), _d(3.8, REP)), outside_rect=(_d(12, REP), _d(7.2, REP))),
}

# 윗면: 발판 중심 간격 60mm, 앞쪽. LED 창은 XIAO 위
# 뒷면(밖에서 볼 때): 왼쪽 USB-C, 오른쪽 TRS. 위에서 보면 좌우가 바뀐다(케이스 x = -뒷면 x)
PLACEMENTS = [
    Placement("발판 왼쪽", "fs", "top", -30, -12),
    Placement("발판 오른쪽", "fs", "top", 30, -12),
    Placement("키 세트 LED", "led", "top", 24, 34),
    Placement("USB-C", "usb", "back", -24, 0),
    Placement("TRS", "trs", "back", 22, 0),
]


def internal(values: dict[str, float]) -> list[Box]:
    return []


def missing_extra() -> list[str]:
    return ["XIAO 고정 방식과 높이(USB-C 중심이 뒷면 가운데에 오도록 받침 필요)",
            "LiPo 배터리 자리(발판 몸체 뒤쪽, 풋스위치 단자와 떨어뜨려 고정)"]


DESIGN = Design("page_pedal_1590bb", CASE, PARTS, PLACEMENTS, internal, missing_extra)
