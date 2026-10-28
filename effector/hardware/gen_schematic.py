"""베이스 컴프레서 페달 회로도 생성기.

KiCad 공식 심볼 라이브러리(9.0.7 태그의 .kicad_sym)에서 심볼 정의를 그대로 끌어와
bass-compressor.kicad_sch(KiCad 8 형식, 9·10에서 열림)와 netlist.md를 만든다.
값과 결선은 이 파일의 PARTS 표가 기준이고, 설계 근거는
devices/bass-effector-schematic.md에 있다.

사용:
    python gen_schematic.py --libs <kicad_sym 폴더> [--out <출력 폴더>]

폴더는 설치된 KiCad 9의 심볼 폴더(Ubuntu: /usr/share/kicad/symbols)이거나, GitLab kicad-symbols
저장소 9.0.7 태그에서 받은 파일들이다.
    https://gitlab.com/api/v4/projects/kicad%2Flibraries%2Fkicad-symbols/repository/files/<이름>.kicad_sym/raw?ref=9.0.7
"""

from __future__ import annotations

import argparse
import io
import re
import uuid
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = "bass-compressor"
TITLE = "베이스 컴프레서 페달 1호"
FONT = "(effects (font (size 1.27 1.27)))"
STUB = 2.54


# ---------------------------------------------------------------- s-expression
class Sym(str):
    """따옴표 없는 토큰."""


def parse(text: str) -> list:
    tokens = re.findall(r'"(?:[^"\\]|\\.)*"|[()]|[^\s()"]+', text)
    stack: list[list] = [[]]
    for token in tokens:
        if token == "(":
            stack.append([])
        elif token == ")":
            node = stack.pop()
            stack[-1].append(node)
        elif token.startswith('"'):
            stack[-1].append(token[1:-1].replace('\\"', '"').replace("\\\\", "\\"))
        else:
            stack[-1].append(Sym(token))
    assert len(stack) == 1, "괄호가 안 맞는다"
    return stack[0]


def dump(node, indent: int = 0) -> str:
    if isinstance(node, Sym):
        return str(node)
    if isinstance(node, str):
        return '"' + node.replace("\\", "\\\\").replace('"', '\\"') + '"'
    pad = "\t" * indent
    if all(not isinstance(child, list) for child in node):
        return pad + "(" + " ".join(dump(child) for child in node) + ")"
    head = [dump(child) for child in node if not isinstance(child, list)]
    body = [dump(child, indent + 1) for child in node if isinstance(child, list)]
    return pad + "(" + " ".join(head) + "\n" + "\n".join(body) + "\n" + pad + ")"


def find_all(node: list, key: str) -> list[list]:
    return [child for child in node if isinstance(child, list) and child and child[0] == key]


def find(node: list, key: str) -> list | None:
    hits = find_all(node, key)
    return hits[0] if hits else None


# ---------------------------------------------------------------- 심볼 라이브러리
@dataclass
class Pin:
    number: str
    name: str
    x: float
    y: float
    angle: int
    unit: int
    electrical: str


class Library:
    def __init__(self, folder: Path):
        self.folder = folder
        self.cache: dict[str, dict[str, list]] = {}

    def raw(self, lib: str, name: str) -> list:
        if lib not in self.cache:
            text = io.open(self.folder / f"{lib}.kicad_sym", encoding="utf-8").read()
            tree = parse(text)[0]
            self.cache[lib] = {s[1]: s for s in find_all(tree, "symbol")}
        return self.cache[lib][name]

    def flattened(self, lib: str, name: str) -> list:
        """extends를 풀고 이름을 lib:name으로 바꾼 심볼 트리."""
        child = self.raw(lib, name)
        parent_name = find(child, "extends")
        if parent_name is None:
            tree = _clone(child)
            base = name
        else:
            tree = _clone(self.raw(lib, parent_name[1]))
            base = parent_name[1]
            child_props = {p[1]: p for p in find_all(child, "property")}
            # 자식이 정의한 속성은 값만이 아니라 위치·글꼴까지 통째로 자식 것을 쓴다(KiCad와 같은 결과)
            is_property = lambda n: isinstance(n, list) and n and n[0] == "property"
            for index, node in enumerate(tree):
                if is_property(node) and node[1] in child_props:
                    tree[index] = _clone(child_props[node[1]])
            last_property = max(i for i, n in enumerate(tree) if is_property(n))
            for prop_name, prop in child_props.items():
                if not any(is_property(n) and n[1] == prop_name for n in tree):
                    last_property += 1
                    tree.insert(last_property, _clone(prop))
        tree[1] = f"{lib}:{name}"
        for sub in find_all(tree, "symbol"):
            sub[1] = name + sub[1][len(base):]
        tree[:] = [c for c in tree if not (isinstance(c, list) and c and c[0] in ("embedded_fonts", "extends"))]
        return tree

    def pins(self, lib: str, name: str) -> list[Pin]:
        tree = self.flattened(lib, name)
        pins = []
        for sub in find_all(tree, "symbol"):
            unit = int(sub[1].rsplit("_", 2)[1])
            for pin in find_all(sub, "pin"):
                at = find(pin, "at")
                pins.append(Pin(find(pin, "number")[1], find(pin, "name")[1],
                                float(at[1]), float(at[2]), int(float(at[3])), unit, str(pin[1])))
        return pins


def _clone(node):
    return [_clone(c) for c in node] if isinstance(node, list) else node


# ---------------------------------------------------------------- 부품 표
@dataclass
class Part:
    ref: str
    lib: str
    name: str
    value: str
    nets: dict[str, str]          # 핀 번호 -> 넷 이름. "NC"는 미접속
    unit: int = 1
    note: str = ""
    footprint: str = ""


@dataclass
class Block:
    title: str
    parts: list[Part] = field(default_factory=list)


# 넷 이름 규칙: SEED_<핀 이름>_<역할>은 Daisy Seed3 핀. J4 핀 번호가 Seed3 물리 핀 번호(데이터시트 표 2)다.
BLOCKS = [
    Block("A. 전원 입력과 보호 (설계서 3.5절)", [
        Part("J3", "Connector", "Barrel_Jack", "DC 9V 센터 마이너스", {"1": "GND", "2": "VIN_JACK"},
             note="핀 1 센터 = GND, 핀 2 슬리브 = +9V"),
        Part("FB1", "Device", "FerriteBead", "600R@100MHz", {"1": "VIN_JACK", "2": "VIN_RAW"},
             note="어댑터 선으로 들어오는 고주파와 페달 안 스위칭 잡음(LT1054, 화면)이 밖으로 나가는 것을 막는다. Seed3 Pedal Dev Kit 전원 입력과 같은 구성"),
        Part("Q1", "Transistor_FET", "IRF9540N", "IRF9540N", {"1": "VG", "2": "VIN_RAW", "3": "VS"},
             note="역극성 보호. 드레인이 잭 쪽"),
        Part("Q2", "Transistor_FET", "IRF9540N", "IRF9540N", {"1": "VG", "2": "V9", "3": "VS"},
             note="과전압 차단. 드레인이 부하 쪽. Q1과 소스끼리 맞댐"),
        Part("R13", "Device", "R", "100k", {"1": "VG", "2": "GND"}, note="공통 게이트 풀다운"),
        Part("Q3", "Transistor_BJT", "2N3906", "2N3906", {"1": "VS", "2": "OV_B", "3": "VG"},
             note="과전압이면 게이트를 소스로 끌어올린다"),
        Part("R14", "Device", "R", "10k", {"1": "VS", "2": "OV_B"}, note="Q3 에미터-베이스. 평상시 Q3 차단"),
        Part("R15", "Device", "R", "4.7k", {"1": "OV_B", "2": "ZK"}, note="Q3 베이스 전류 제한"),
        Part("D3", "Device", "D_Zener", "1N4742A 12V", {"1": "ZK", "2": "GND"}, note="임계 약 12.7V"),
        Part("D14", "Device", "D_Zener", "1N4744A 15V", {"1": "VS", "2": "VG"},
             note="Q1·Q2 게이트 보호. 캐소드가 소스. 꽂는 순간 Vgs를 15V로 제한"),
        Part("C7", "Device", "C_Polarized", "1000u 25V", {"1": "V9", "2": "GND"}, note="벌크. 차단 시 VIN 4V까지 약 22ms 유지"),
    ]),
    Block("B. LT1054 더블러와 아날로그 레일 (설계서 4.2절, 데이터시트 그림 20)", [
        Part("U2", "Regulator_SwitchedCapacitor", "LT1054", "LT1054CP",
             {"8": "V9", "3": "GND", "5": "GND", "2": "PUMP", "1": "NC", "4": "NC", "6": "NC", "7": "NC"},
             note="양전압 더블러. VOUT은 GND, CAP-·FB·OSC·VREF 개방"),
        Part("C16", "Device", "C_Polarized", "10u 50V", {"1": "V9", "2": "GND"}, note="LT1054 입력. 핀 8 가까이"),
        Part("C8", "Device", "C_Polarized", "10u 50V", {"1": "PUMPX", "2": "PUMP"}, note="펌프. + 쪽이 다이오드 접점"),
        Part("D1", "Device", "D_Schottky", "1N5819", {"1": "PUMPX", "2": "V9"}, note="애노드 9V"),
        Part("D2", "Device", "D_Schottky", "1N5819", {"1": "VRAIL_RAW", "2": "PUMPX"}),
        Part("C9", "Device", "C_Polarized", "100u 35V", {"1": "VRAIL_RAW", "2": "GND"}, note="더블러 출력"),
        Part("R16", "Device", "R", "10R", {"1": "VRAIL_RAW", "2": "VRAIL"}, note="레일 필터. 6mA에서 60mV 강하"),
        Part("C10", "Device", "C_Polarized", "100u 35V", {"1": "VRAIL", "2": "GND"}, note="레일 필터. 차단 159Hz"),
        Part("C17", "Device", "C", "100n", {"1": "VRAIL", "2": "GND"}, note="U1 핀 8 가까이"),
        Part("R11", "Device", "R", "10k", {"1": "VRAIL", "2": "VBIAS"}, note="Vbias 분압 상"),
        Part("R12", "Device", "R", "10k", {"1": "VBIAS", "2": "GND"}, note="Vbias 분압 하"),
        Part("C6", "Device", "C_Polarized", "100u 35V", {"1": "VBIAS", "2": "GND"}, note="Vbias. 20Hz에서 80옴"),
    ]),
    Block("C. Seed3 VIN 필터와 전원 감시 (설계서 3.5절, 4.1절)", [
        Part("R32", "Device", "R", "3.3R", {"1": "V9", "2": "SEED_VIN"}, note="VIN 필터. 150mA에서 0.5V 강하"),
        Part("C11", "Device", "C_Polarized", "100u 35V", {"1": "SEED_VIN", "2": "GND"}, note="VIN 필터. 차단 0.5kHz"),
        Part("C12", "Device", "C", "100n", {"1": "SEED_VIN", "2": "GND"}, note="핀 39 가까이"),
        Part("R19", "Device", "R", "47k", {"1": "V9", "2": "VMON"}, note="감시 분압 상. 18V에서 3.16V"),
        Part("R20", "Device", "R", "10k", {"1": "VMON", "2": "GND"}, note="감시 분압 하"),
        Part("C14", "Device", "C", "100n", {"1": "VMON", "2": "GND"}, note="시정수 0.8ms"),
        Part("R21", "Device", "R", "1k", {"1": "VMON", "2": "SEED_A5_VMON"}, note="ADC 핀 직렬"),
    ]),
    Block("D. 입력 네트워크와 버퍼, 패드 (설계서 4.1절)", [
        Part("J1", "Connector_Audio", "AudioJack2", "IN 6.35mm TS", {"T": "IN_TIP", "S": "GND"}),
        Part("R1", "Device", "R", "2.2M", {"1": "IN_A", "2": "GND"}, note="R_pd. K1 NO 접점 쪽"),
        Part("R2", "Device", "R", "10k", {"1": "IN_A", "2": "IN_B"}, note="입력 클램프 전류 제한"),
        Part("C1", "Device", "C", "100n 필름", {"1": "IN_B", "2": "IN_C"}, note="입력 커플링. R3와 0.9Hz"),
        Part("C24", "Device", "C", "100p C0G 50V", {"1": "IN_C", "2": "GND"},
             note="RF 필터. R2 10k와 159kHz. 버퍼 입력 핀 가까이. Seed3 Pedal Dev Kit의 100Ω/100pF와 같은 목적"),
        Part("R3", "Device", "R", "1.8M", {"1": "IN_C", "2": "VBIAS"}, note="R_bias. R1과 병렬 약 1M"),
        Part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"3": "IN_C", "2": "BUF_OUT", "1": "BUF_OUT"},
             unit=1, note="A: 유니티 버퍼"),
        Part("C2", "Device", "C_Polarized", "47u 50V", {"1": "BUF_OUT", "2": "PAD_IN"}, note="패드 커플링. 0.85Hz. + 쪽이 버퍼"),
        Part("R4", "Device", "R", "3.3k", {"1": "PAD_IN", "2": "SEED_AUDIO_IN1"}, note="패드 직렬"),
        Part("R5", "Device", "R", "820R", {"1": "SEED_AUDIO_IN1", "2": "GND"}, note="패드 션트. 코덱 5k 포함 -15.1dB"),
        Part("C23", "Device", "C", "4.7n", {"1": "SEED_AUDIO_IN1", "2": "GND"}, note="종단. 580Ω과 58kHz"),
    ]),
    Block("E. 출력단 (설계서 4.2절)", [
        Part("C3", "Device", "C", "1u 필름", {"1": "SEED_AUDIO_OUT1", "2": "OUT_IN"}, note="R6와 1.6Hz"),
        Part("R6", "Device", "R", "100k", {"1": "OUT_IN", "2": "VBIAS"}, note="출력단 + 입력 바이어스"),
        Part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"5": "OUT_IN", "6": "FB", "7": "U1B_OUT"},
             unit=2, note="B: 비반전 +15.1dB"),
        Part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"8": "VRAIL", "4": "GND"},
             unit=3, note="전원"),
        Part("R7", "Device", "R", "47k", {"1": "U1B_OUT", "2": "FB"}, note="피드백. 게인 1 + 47k/10k = 5.7"),
        Part("R8", "Device", "R", "10k", {"1": "FB", "2": "FB_C"}),
        Part("C4", "Device", "C_Polarized", "10u 50V", {"1": "FB_C", "2": "GND"}, note="DC 게인 1. + 쪽이 R8"),
        Part("R9", "Device", "R", "100R", {"1": "U1B_OUT", "2": "OUT_R"}, note="케이블 용량 분리"),
        Part("C5", "Device", "C_Polarized", "10u 50V", {"1": "OUT_R", "2": "OUT_C"}, note="출력 커플링. + 쪽이 op-amp"),
        Part("R10", "Device", "R", "100k", {"1": "OUT_C", "2": "GND"}, note="방전 R. K1 NO 접점 쪽"),
        Part("J2", "Connector_Audio", "AudioJack2", "OUT 6.35mm TS", {"T": "OUT_TIP", "S": "GND"}),
    ]),
    Block("F. K1 릴레이 바이패스와 구동 (설계서 3.4절, 5.4절)", [
        Part("K1", "Relay", "Relay_DPDT", "TQ2-9V",
             {"A1": "V9", "A2": "K1_COIL", "11": "IN_TIP", "12": "BYPASS", "14": "IN_A",
              "21": "OUT_TIP", "22": "BYPASS", "24": "OUT_C"},
             note="코일 꺼짐 = NC = 트루 바이패스. 11·21 COM, 12·22 NC, 14·24 NO"),
        Part("D4", "Device", "D", "1N4148", {"1": "V9", "2": "K1_COIL"}, note="플라이백. 캐소드 9V"),
        Part("Q4", "Transistor_BJT", "2N3904", "2N3904", {"1": "GND", "2": "Q4_B", "3": "K1_COIL"}),
        Part("R17", "Device", "R", "2.2k", {"1": "SEED_D0_K1DRV", "2": "Q4_B"}, note="베이스 1.2mA"),
        Part("R18", "Device", "R", "10k", {"1": "Q4_B", "2": "GND"}, note="리셋 중 K1 해제"),
    ]),
    Block("G. 컨트롤: 노브, 풋스위치, LED (설계서 3.2절, 3.3절, 4.3절)", [
        Part("RV1", "Device", "R_Potentiometer", "B10K Input", {"1": "SEED_3V3A", "2": "SEED_A0_INPUT", "3": "GND"}),
        Part("RV2", "Device", "R_Potentiometer", "B10K Ratio", {"1": "SEED_3V3A", "2": "SEED_A1_RATIO", "3": "GND"}),
        Part("RV3", "Device", "R_Potentiometer", "B10K Att/Rel", {"1": "SEED_3V3A", "2": "SEED_A2_ATTREL", "3": "GND"}),
        Part("RV4", "Device", "R_Potentiometer", "B10K Blend", {"1": "SEED_3V3A", "2": "SEED_A3_BLEND", "3": "GND"}),
        Part("RV5", "Device", "R_Potentiometer", "B10K Output", {"1": "SEED_3V3A", "2": "SEED_A4_OUTPUT", "3": "GND"}),
        Part("C18", "Device", "C", "100n", {"1": "SEED_A0_INPUT", "2": "GND"}),
        Part("C19", "Device", "C", "100n", {"1": "SEED_A1_RATIO", "2": "GND"}),
        Part("C20", "Device", "C", "100n", {"1": "SEED_A2_ATTREL", "2": "GND"}),
        Part("C21", "Device", "C", "100n", {"1": "SEED_A3_BLEND", "2": "GND"}),
        Part("C22", "Device", "C", "100n", {"1": "SEED_A4_OUTPUT", "2": "GND"}),
        Part("SW1", "Switch", "SW_Push", "풋스위치 모멘터리", {"1": "SEED_D1_FSW", "2": "GND"}),
        Part("R22", "Device", "R", "10k", {"1": "SEED_3V3D", "2": "SEED_D1_FSW"}, note="풀업"),
        Part("C15", "Device", "C", "100n", {"1": "SEED_D1_FSW", "2": "GND"}, note="디바운스 보조"),
    ] + [
        item for i in range(8) for item in (
            Part(f"R{23 + i}", "Device", "R", "470R", {"1": f"SEED_D{2 + i}_METER{i + 1}", "2": f"LED{i + 1}_A"}),
            Part(f"D{5 + i}", "Device", "LED", f"3mm 미터 {i + 1}", {"1": "GND", "2": f"LED{i + 1}_A"}),
        )
    ] + [
        Part("R31", "Device", "R", "470R", {"1": "SEED_D10_LEDON", "2": "LED9_A"}),
        Part("D13", "Device", "LED", "3mm 온", {"1": "GND", "2": "LED9_A"}),
    ]),
    Block("H. Daisy Seed3 소켓 (설계서 4.3절). 핀 번호는 Seed3 데이터시트 표 2와 같다", [
        Part("J4", "Connector_Generic", "Conn_01x40", "Daisy Seed3 2x20 소켓", {
            "1": "SEED_D0_K1DRV", "2": "SEED_D1_FSW",
            "3": "SEED_D2_METER1", "4": "SEED_D3_METER2", "5": "SEED_D4_METER3", "6": "SEED_D5_METER4",
            "7": "SEED_D6_METER5", "8": "SEED_D7_METER6", "9": "SEED_D8_METER7", "10": "SEED_D9_METER8",
            "11": "SEED_D10_LEDON", "12": "NC", "13": "NC", "14": "NC", "15": "NC",
            "16": "SEED_AUDIO_IN1", "17": "NC", "18": "SEED_AUDIO_OUT1", "19": "NC",
            "20": "GND", "21": "SEED_3V3A",
            "22": "SEED_A0_INPUT", "23": "SEED_A1_RATIO", "24": "SEED_A2_ATTREL", "25": "SEED_A3_BLEND",
            "26": "SEED_A4_OUTPUT", "27": "SEED_A5_VMON",
            "28": "NC", "29": "NC", "30": "NC", "31": "NC", "32": "NC", "33": "NC", "34": "NC", "35": "NC",
            "36": "NC", "37": "NC", "38": "SEED_3V3D", "39": "SEED_VIN", "40": "GND"},
            note="16 Audio In 1, 18 Audio Out 1, 20 AGND, 21 3V3A, 38 3V3D, 39 VIN, 40 DGND. 12~15(D11~D14)는 비움"),
    ]),
    Block("I. ERC용 전원 플래그. GND는 U2 VOUT(전원 출력 핀)이 구동하므로 플래그를 두지 않는다", [
        Part("PF1", "power", "PWR_FLAG", "PWR_FLAG", {"1": "VIN_RAW"}),
        Part("PF2", "power", "PWR_FLAG", "PWR_FLAG", {"1": "V9"}),
        Part("PF3", "power", "PWR_FLAG", "PWR_FLAG", {"1": "VRAIL"}),
        Part("PF4", "power", "PWR_FLAG", "PWR_FLAG", {"1": "SEED_3V3A"}),
        Part("PF6", "power", "PWR_FLAG", "PWR_FLAG", {"1": "SEED_3V3D"}),
    ]),
]


# ---------------------------------------------------------------- 배치와 출력
def root_uuid(project: str) -> str:
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "tolelom/hardware/" + project))


def uid(project: str, *keys: object) -> str:
    return str(uuid.uuid5(uuid.NAMESPACE_URL, project + "/" + "/".join(map(str, keys))))


def fmt(v: float) -> str:
    return f"{v:.2f}".rstrip("0").rstrip(".") if abs(v - round(v)) > 1e-9 else str(int(round(v)))


OUTWARD = {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}
LABEL_ANGLE = {0: 180, 180: 0, 90: 270, 270: 90}
LABEL_JUSTIFY = {0: "right", 180: "left", 90: "right", 270: "left"}


def render(library: Library, out: Path, blocks: list[Block] | None = None,
           name: str = PROJECT, title: str = TITLE, doc: str = "bass-effector-schematic.md") -> None:
    blocks = BLOCKS if blocks is None else blocks
    root = root_uuid(name)
    lib_symbols: dict[str, list] = {}
    body: list[str] = []
    netlist: dict[str, list[tuple[str, str, str]]] = {}
    net_rows: list[str] = []
    cell = 40.64
    x_limit = 1150.0
    y0 = 25.4
    for block in blocks:
        x0 = 25.4
        body.append(f'(text "{block.title}" (exclude_from_sim no) (at {fmt(x0)} {fmt(y0 - 5.08)} 0) '
                    f'{FONT[:-1]} (justify left bottom)) (uuid "{uid(name, "text", block.title)}"))')
        column = 0
        row_height = cell
        for part in block.parts:
            key = f"{part.lib}:{part.name}"
            if key not in lib_symbols:
                lib_symbols[key] = library.flattened(part.lib, part.name)
            pins = [p for p in library.pins(part.lib, part.name) if p.unit in (0, part.unit)]
            span = max(1, int((max(abs(p.y) for p in pins) * 2 + 15.24) // cell) + 1) if pins else 1
            height = span * cell
            if x0 + (column + 1) * cell > x_limit:
                column = 0
                y0 += row_height
                row_height = cell
            wide = 2 if pins and max(abs(p.x) for p in pins) > 5.08 else 1
            row_height = max(row_height, height)
            cx = x0 + column * cell + wide * cell / 2
            cy = y0 + height / 2
            column += wide
            body.append(symbol_instance(part, cx, cy, name, root))
            for pin in pins:
                px, py = cx + pin.x, cy - pin.y
                net = part.nets.get(pin.number)
                if net is None:
                    raise SystemExit(f"{part.ref} 핀 {pin.number}({pin.name})의 넷이 없다")
                if net == "NC":
                    body.append(f'(no_connect (at {fmt(px)} {fmt(py)}) (uuid "{uid(name, part.ref, pin.number, "nc")}"))')
                    continue
                dx, dy = OUTWARD[pin.angle]
                ex, ey = px + dx * STUB, py + dy * STUB
                body.append(f'(wire (pts (xy {fmt(px)} {fmt(py)}) (xy {fmt(ex)} {fmt(ey)})) '
                            f'(stroke (width 0) (type default)) (uuid "{uid(name, part.ref, pin.number, "w")}"))')
                body.append(global_label(net, ex, ey, pin.angle, name, part.ref, pin.number))
                netlist.setdefault(net, []).append((part.ref, pin.number, pin.name))
            shown_ref = part.ref if part.name != "TL072" else part.ref + {1: "A", 2: "B", 3: " 전원"}[part.unit]
            net_rows.append(f"| {shown_ref} | {part.value} | " +
                            ", ".join(f"{n}={part.nets[n]}" for n in part.nets) + f" | {part.note} |")
        y0 += row_height + 15.24

    check(netlist, blocks)
    symbols_text = "\n".join(dump(tree, 1) for tree in lib_symbols.values())
    sch = (
        f'(kicad_sch (version 20231120) (generator "gen_schematic.py") (generator_version "8.0")\n'
        f'\t(uuid "{root}")\n\t(paper "A0")\n'
        f'\t(title_block (title "{title}") (date "2026-09-14") (rev "0.1") '
        f'(comment 1 "설계서 devices/bass-effector-design.md, 값 근거 devices/bass-effector-schematic.md"))\n'
        f'\t(lib_symbols\n{symbols_text}\n\t)\n'
        + "\n".join("\t" + line for line in body) + "\n"
        f'\t(sheet_instances (path "/" (page "1")))\n)\n'
    )
    out.mkdir(parents=True, exist_ok=True)
    io.open(out / f"{name}.kicad_sch", "w", encoding="utf-8", newline="\n").write(sch)
    io.open(out / f"{name}.kicad_pro", "w", encoding="utf-8", newline="\n").write(
        '{\n  "meta": {\n    "filename": "%s.kicad_pro",\n    "version": 1\n  },\n'
        '  "sheets": [\n    [\n      "%s",\n      ""\n    ]\n  ]\n}\n' % (name, root))
    netlist_name = "netlist.md" if name == PROJECT else f"{name}-netlist.md"
    write_netlist(out / netlist_name, net_rows, netlist, title, doc)


def symbol_instance(part: Part, x: float, y: float, project: str, root: str) -> str:
    key = f"{part.lib}:{part.name}"
    props = [
        ("Reference", part.ref, x + 5.08, y - 3.81, ""),
        ("Value", part.value, x + 5.08, y - 1.27, ""),
        ("Footprint", part.footprint, x, y, " (hide yes)"),
        ("Datasheet", "~", x, y, " (hide yes)"),
        ("Description", part.note, x, y, " (hide yes)"),
    ]
    prop_text = "\n".join(
        f'\t(property "{k}" "{v}" (at {fmt(px)} {fmt(py)} 0) (effects (font (size 1.27 1.27)) (justify left){h}))'
        for k, v, px, py, h in props)
    pin_text = "\n".join(f'\t(pin "{n}" (uuid "{uid(project, part.ref, part.unit, n, "pin")}"))' for n in part.nets)
    return (
        f'(symbol (lib_id "{key}") (at {fmt(x)} {fmt(y)} 0) (unit {part.unit}) (exclude_from_sim no) '
        f'(in_bom yes) (on_board yes) (dnp no)\n'
        f'\t(uuid "{uid(project, part.ref, part.unit, "sym")}")\n{prop_text}\n{pin_text}\n'
        f'\t(instances (project "{project}" (path "/{root}" (reference "{part.ref}") (unit {part.unit}))))\n)'
    )


def global_label(net: str, x: float, y: float, pin_angle: int, project: str, ref: str, number: str) -> str:
    angle = LABEL_ANGLE[pin_angle]
    justify = LABEL_JUSTIFY[pin_angle]
    return (
        f'(global_label "{net}" (shape passive) (at {fmt(x)} {fmt(y)} {angle}) (fields_autoplaced yes) '
        f'(effects (font (size 1.27 1.27)) (justify {justify})) (uuid "{uid(project, ref, number, "lbl")}")\n'
        f'\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}" (at {fmt(x)} {fmt(y)} 0) '
        f'(effects (font (size 1.27 1.27)) (hide yes))))'
    )


def check(netlist: dict[str, list[tuple[str, str, str]]], blocks: list[Block]) -> None:
    problems = []
    for net, pins in sorted(netlist.items()):
        if len(pins) < 2:
            problems.append(f"넷 {net}에 핀이 하나뿐: {pins}")
    refs = [p.ref for b in blocks for p in b.parts]
    seen: dict[str, int] = {}
    for b in blocks:
        for p in b.parts:
            seen[(p.ref, p.unit)] = seen.get((p.ref, p.unit), 0) + 1
    duplicates = [k for k, v in seen.items() if v > 1]
    if duplicates:
        problems.append(f"지정자·유닛 중복: {duplicates}")
    if problems:
        raise SystemExit("\n".join(problems))
    print(f"부품 {len(set(refs))}개, 넷 {len(netlist)}개, 핀 접속 {sum(len(v) for v in netlist.values())}개. 검사 통과")


def write_netlist(path: Path, rows: list[str], netlist: dict[str, list[tuple[str, str, str]]], title: str,
                  doc: str) -> None:
    heading = "# 베이스 컴프레서 넷리스트 (자동 생성)" if title == TITLE else f"# {title} 넷리스트 (자동 생성)"
    lines = [heading, "",
             "gen_schematic.py가 만든다. 손으로 고치지 않는다. 값의 근거는",
             f"[{doc}](../../devices/{doc}).", "",
             "## 부품별 접속", "", "| 지정자 | 값 | 핀=넷 | 비고 |", "|---|---|---|---|"] + rows + [
             "", "## 넷별 핀", "", "| 넷 | 핀 |", "|---|---|"]
    for net, pins in sorted(netlist.items()):
        lines.append(f"| {net} | " + ", ".join(f"{r}.{n}" for r, n, _ in pins) + " |")
    io.open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--libs", required=True, help="KiCad .kicad_sym 폴더")
    parser.add_argument("--out", default=str(HERE), help="출력 폴더")
    args = parser.parse_args()
    render(Library(Path(args.libs)), Path(args.out))


if __name__ == "__main__":
    main()
