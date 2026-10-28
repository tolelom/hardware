"""멀티 이펙터 플랫폼 회로 블록. 하드웨어 플랫폼 설계서 4절·5.2절, 값 근거 devices/bass-multi-platform-schematic.md.

    python3 effector/hardware/platform_blocks.py --libs <KiCad .kicad_sym 폴더>

platform.kicad_sch, platform-netlist.md, spice/generated/platform/platform_<문자>.cir를 쓴다. 생성물은 손으로 고치지 않는다.
블록 제목 앞의 [메인]·[조작부]가 부품이 놓이는 기판이다.
"""
from __future__ import annotations

import argparse
import dataclasses
from pathlib import Path

import gen_spice
import pinmap
from gen_schematic import BLOCKS, HERE, Block, Library, Part, render

SPICE_DIR = gen_spice.GENERATED / "platform"
NAME = "platform"
TITLE = "멀티 이펙터 플랫폼"

# 1호에서 Seed3 핀 이름이 바뀐 넷. 물리 핀은 같다(D0, D20)
SEED_RENAMES = {
    "SEED_D0_K1DRV": pinmap.net("K1_DRIVE"),
    "SEED_A5_VMON": pinmap.net("VMON_ADC"),
}
# 계승 블록에서 바꿔 끼우는 부품. 아래 블록 P 참고
REPLACED = {"J2"}
# EXP ADC 값이 전체 눈금의 이 비율 이상이면 펌웨어가 페달이 빠졌다고 본다
EXP_UNPLUGGED_THRESHOLD = 0.97


def _inherit(block: Block) -> Block:
    parts = []
    for part in block.parts:
        if part.ref in REPLACED:
            continue
        nets = {pin: SEED_RENAMES.get(net, net) for pin, net in part.nets.items()}
        parts.append(dataclasses.replace(part, nets=nets))
    return Block("[메인] " + block.title, parts)


def _seed_socket() -> Part:
    nets = {str(n): "NC" for n in range(1, 41)}
    nets.update({"16": "SEED_AUDIO_IN1", "18": "SEED_AUDIO_OUT1", "19": "SEED_AUDIO_OUT2",
                 "20": "GND", "21": "SEED_3V3A", "38": "SEED_3V3D", "39": "SEED_VIN", "40": "GND"})
    for function, name in pinmap.PLATFORM.items():
        nets[str(pinmap.SEED3_PINS[name].physical)] = pinmap.net(function)
    return Part("J4", "Connector_Generic", "Conn_01x40", "Daisy Seed3 2x20 소켓", nets,
                note="핀 번호는 Seed3 데이터시트 v2.1.0 표 2 물리 핀. 배정은 pinmap.PLATFORM. "
                     "17 Audio In 2, D9·D28~D30은 비움. 38 3V3D는 MIDI 회로 전원")


N = pinmap.net
# 2×10 헤더 핀 1~20의 넷. 메인 J20과 조작부 J21이 같은 순서로 잇는다
BOARD_LINK = [
    "V9", "GND", "3V3_AUX", "GND",
    N("SPI1_SCK"), N("SPI1_MOSI"), N("SPI1_NSS"), N("DISPLAY_DC"), N("DISPLAY_RST"), N("DISPLAY_BL_PWM"),
    N("I2C1_SCL"), N("I2C1_SDA"), N("MCP_INT_A"), N("MCP_INT_B"),
    N("FS1"), N("FS2"), N("FS3"), N("LED1"), N("LED2"), N("LED3"),
]


def _link(ref: str, note: str) -> Part:
    return Part(ref, "Connector_Generic", "Conn_02x10_Odd_Even", "2x10 2.54mm",
                {str(i + 1): net for i, net in enumerate(BOARD_LINK)}, note=note)


def _exp_block() -> Block:
    return Block("[메인] N. EXP 입력 (설계서 5.2절)", [
        Part("J12", "Connector_Audio", "AudioJack3", "EXP 6.35mm TRS", {"T": "EXP_TIP", "R": "EXP_RING", "S": "GND"},
             note="팁 와이퍼, 링 기준, 슬리브 GND"),
        Part("R50", "Device", "R", "10k", {"1": "SEED_3V3A", "2": "EXP_RING"},
             note="링 기준 직렬. 꽂힌 페달의 최대값을 R/(R+10k)로 낮춰 뺀 상태(전체 눈금)와 구분한다. "
                  "10k 페달 50%, 100k 90.9%, 250k 96.2%(문턱 97%)"),
        Part("R51", "Device", "R", "100k", {"1": "SEED_3V3A", "2": "EXP_TIP"},
             note="팁 풀업. 뺐을 때 ADC가 전체 눈금 = 꽂힘 감지"),
        Part("R52", "Device", "R", "1k", {"1": "EXP_TIP", "2": N("EXP_ADC")}, note="ADC 직렬, 정전기 전류 제한"),
        Part("C50", "Device", "C", "100n", {"1": N("EXP_ADC"), "2": "GND"}, note="R52와 1.6kHz"),
    ])


def _sd_block() -> Block:
    pull = [("R54", "SDMMC1_CMD"), ("R55", "SDMMC1_D0"), ("R56", "SDMMC1_D1"), ("R57", "SDMMC1_D2"), ("R58", "SDMMC1_D3")]
    return Block("[메인] O. microSD (설계서 5.2절, SDMMC1 4비트)", [
        Part("J13", "Connector", "Micro_SD_Card", "microSD 푸시푸시 소켓", {
            "1": N("SDMMC1_D2"), "2": N("SDMMC1_D3"), "3": N("SDMMC1_CMD"), "4": "3V3_AUX",
            "5": "SD_CLK", "6": "GND", "7": N("SDMMC1_D0"), "8": N("SDMMC1_D1"), "9": "GND"},
            note="SD 물리 계층 규격: CMD·DAT 풀업 필요, CLK는 풀업 없음"),
        Part("R53", "Device", "R", "33R", {"1": N("SDMMC1_CK"), "2": "SD_CLK"}, note="CLK 직렬. 반사 완화"),
    ] + [Part(ref, "Device", "R", "47k", {"1": "3V3_AUX", "2": N(function)}, note="풀업") for ref, function in pull] + [
        Part("C51", "Device", "C", "100n", {"1": "3V3_AUX", "2": "GND"}, note="소켓 VDD 가까이"),
        Part("C52", "Device", "C", "10u", {"1": "3V3_AUX", "2": "GND"}, note="삽입 순간 돌입 전류"),
    ])


def _stereo_block() -> Block:
    dnp = "DNP(자리만). "
    return Block("[메인] P. 출력 잭과 스테레오 출력 자리 (설계서 5.2절). 값은 블록 E와 같다", [
        Part("J2", "Connector_Audio", "AudioJack3", "OUT 6.35mm TRS", {"T": "OUT_TIP", "R": "OUT2_TIP", "S": "GND"},
             note="1호 TS 잭을 TRS로 바꿨다. 링 = 스테레오 자리. 모노 TS 플러그는 링을 GND에 붙이므로 R70 100Ω이 단락 전류를 막는다"),
        Part("C70", "Device", "C", "1u 필름", {"1": "SEED_AUDIO_OUT2", "2": "OUT2_IN"}, note=dnp + "C3와 같음"),
        Part("R71", "Device", "R", "100k", {"1": "OUT2_IN", "2": "VBIAS"}, note=dnp + "R6와 같음"),
        Part("U3", "Amplifier_Operational", "TL072", "OPA2992", {"3": "OUT2_IN", "2": "OUT2_FB", "1": "U3A_OUT"},
             unit=1, note=dnp + "U1B와 같음"),
        Part("U3", "Amplifier_Operational", "TL072", "OPA2992", {"5": "VBIAS", "6": "U3B_OUT", "7": "U3B_OUT"},
             unit=2, note=dnp + "안 쓰는 반쪽. 버퍼로 VBIAS에 묶는다"),
        Part("U3", "Amplifier_Operational", "TL072", "OPA2992", {"8": "VRAIL", "4": "GND"}, unit=3, note=dnp + "전원"),
        Part("R72", "Device", "R", "47k", {"1": "U3A_OUT", "2": "OUT2_FB"}, note=dnp + "R7과 같음"),
        Part("R73", "Device", "R", "10k", {"1": "OUT2_FB", "2": "OUT2_FB_C"}, note=dnp + "R8과 같음"),
        Part("C71", "Device", "C_Polarized", "10u 50V", {"1": "OUT2_FB_C", "2": "GND"}, note=dnp + "C4와 같음"),
        Part("R70", "Device", "R", "100R", {"1": "U3A_OUT", "2": "OUT2_R"}, note=dnp + "R9와 같음"),
        Part("C72", "Device", "C_Polarized", "10u 50V", {"1": "OUT2_R", "2": "OUT2_TIP"}, note=dnp + "C5와 같음. 바이패스 때도 링은 K1을 거치지 않는다"),
        Part("R74", "Device", "R", "100k", {"1": "OUT2_TIP", "2": "GND"}, note=dnp + "R10과 같음"),
    ])


def _midi_block() -> Block:
    return Block("[메인] M. MIDI IN·OUT, TRS 타입 A 3.5mm (설계서 5.2절, MMA CA-033·RP-054)", [
        Part("J10", "Connector_Audio", "AudioJack3", "MIDI IN 3.5mm TRS",
             {"T": "MIDI_IN_SINK", "R": "MIDI_IN_SRC", "S": "NC"},
             note="RP-054: 팁 = DIN 5(싱크), 링 = DIN 4(소스), 슬리브 = DIN 2(실드). 수신 쪽 실드는 접지하지 않는다"),
        Part("R40", "Device", "R", "220R", {"1": "MIDI_IN_SRC", "2": "OPTO_A"}, note="CA-033 그림 2 RB"),
        Part("D30", "Device", "D", "1N4148", {"1": "OPTO_A", "2": "MIDI_IN_SINK"},
             note="CA-033 그림 2 D1(1N914). LED 역병렬, 역전압 보호"),
        Part("U11", "Isolator", "H11L1", "H11L1", {
            "1": "OPTO_A", "2": "MIDI_IN_SINK", "3": "NC", "4": N("USART1_RX"), "5": "GND", "6": "SEED_3V3D"},
            note="H11L1M 데이터시트: VCC 3~15V, IF(on) 최대 1.6mA. 1 애노드, 2 캐소드, 4 출력(오픈 컬렉터), 5 GND, 6 VCC. "
                 "Daisy Pod Rev5와 같은 계열"),
        Part("R41", "Device", "R", "270R", {"1": "SEED_3V3D", "2": N("USART1_RX")},
             note="출력 풀업. Daisy Pod Rev5 R64와 같은 값"),
        Part("C40", "Device", "C", "100n", {"1": "SEED_3V3D", "2": "GND"}, note="U11 핀 6 가까이"),
        Part("J11", "Connector_Audio", "AudioJack3", "MIDI OUT 3.5mm TRS",
             {"T": "MIDI_OUT_SINK", "R": "MIDI_OUT_SRC", "S": "GND"}, note="RP-054. 송신 쪽 실드는 접지"),
        Part("R42", "Device", "R", "33R 0.5W", {"1": "3V3_AUX", "2": "MIDI_OUT_SRC"},
             note="CA-033 그림 1 RA, 3.3V ±5% 값. TS 플러그가 링을 GND에 붙이면 100mA(0.33W)가 흐르므로 "
                  "Seed3 3V3D가 아니라 2A 보조 레일에서 받는다"),
        Part("R43", "Device", "R", "10R", {"1": N("USART1_TX"), "2": "MIDI_OUT_SINK"},
             note="CA-033 그림 1 RC, 3.3V ±5% 값"),
    ])


def _aux_supply_block() -> Block:
    return Block("[조작부] L. 보조 3.3V 강압 (설계서 4절·5.2절, TPS562208, 데이터시트 SLVSD91D 7.2절)", [
        Part("FB3", "Device", "FerriteBead", "600R@100MHz 1A", {"1": "V9", "2": "BUCK_VIN"},
             note="입력 파이 필터. 강압기 입력 리플과 백라이트 PWM 전류가 V9를 타고 메인 기판 아날로그단으로 가지 않게 한다"),
        Part("C36", "Device", "C_Polarized", "47u 25V", {"1": "V9", "2": "GND"}, note="파이 필터 J21 쪽"),
        Part("U10", "Regulator_Switching", "TPS562200", "TPS562208", {
            "1": "GND", "2": "BUCK_SW", "3": "BUCK_VIN", "4": "BUCK_FB", "5": "BUCK_EN", "6": "BUCK_BST"},
            note="VIN 4.5~17V, 2A, 580kHz, 강제 연속 모드(FCCM). 경부하에서도 주파수가 오디오 대역으로 내려가지 않는다. "
                 "TPS562201과 핀 호환, 같은 데이터시트(SLVSD91D). 심볼은 핀 배치가 같은 TPS562200"),
        Part("C30", "Device", "C", "10u 25V", {"1": "BUCK_VIN", "2": "GND"}, note="입력. 7.2.2.4절: 10µF 이상, 최대 VIN 이상 정격"),
        Part("C31", "Device", "C", "100n 25V", {"1": "BUCK_VIN", "2": "GND"}, note="U10 핀 3 바로 옆"),
        Part("R80", "Device", "R", "100k", {"1": "BUCK_VIN", "2": "BUCK_EN"}, note="EN을 VIN 쪽으로. EN 절대 최대 19V"),
        Part("C32", "Device", "C", "100n", {"1": "BUCK_BST", "2": "BUCK_SW"}, note="부트스트랩. 7.2.2.5절"),
        Part("L10", "Device", "L", "3.3u", {"1": "BUCK_SW", "2": "3V3_BUCK"}, note="표 7-2: 3.3V 출력 3.3µH(최대 4.7µH). 포화 3A 이상"),
        Part("C33", "Device", "C", "22u 10V", {"1": "3V3_BUCK", "2": "GND"}, note="출력. 표 7-2 20~68µF"),
        Part("C34", "Device", "C", "22u 10V", {"1": "3V3_BUCK", "2": "GND"}, note="출력. C33과 합 44µF"),
        Part("R81", "Device", "R", "33.2k 1%", {"1": "3V3_BUCK", "2": "BUCK_FB"}, note="표 7-2 R1. 0.768 × (1 + 33.2/10) = 3.32V"),
        Part("R82", "Device", "R", "10k 1%", {"1": "BUCK_FB", "2": "GND"}, note="표 7-2 R2"),
        Part("FB2", "Device", "FerriteBead", "600R@100MHz 1A", {"1": "3V3_BUCK", "2": "3V3_AUX"},
             note="스위칭 잡음을 조작부 안에 가둔다. 정격 1A 이상"),
        Part("C35", "Device", "C", "10u 10V", {"1": "3V3_AUX", "2": "GND"}, note="페라이트 뒤"),
        Part("PF7", "power", "PWR_FLAG", "PWR_FLAG", {"1": "3V3_AUX"}),
    ])


def _display_block() -> Block:
    return Block("[조작부] K. 화면 Orient AFK320480A0-3.5INTH-ANO (설계서 3절, 모듈 데이터시트 V01 2절·도면)", [
        Part("J22", "Connector_Generic", "Conn_01x08", "화면 모듈 신호 헤더 2.54mm 8핀", {
            "1": "GND", "2": "3V3_AUX", "3": N("SPI1_SCK"), "4": N("SPI1_MOSI"), "5": N("DISPLAY_RST"),
            "6": N("DISPLAY_DC"), "7": N("SPI1_NSS"), "8": "DISPLAY_BLK"},
            note="데이터시트 2절: 1 GND, 2 VDD(2.5~3.6V), 3 SCL, 4 SDA, 5 RST, 6 DC, 7 CS, 8 BLK. 4선 SPI 전용. "
                 "도면 V01: 모듈 기판 아래 변 1~8, 2.54mm"),
        Part("J23", "Connector_Generic", "Conn_01x08", "화면 모듈 지지 헤더 2.54mm 8핀",
             {str(n): "NC" for n in range(1, 9)},
             note="모듈 9~16핀(위 변, 모두 NC). 기구 지지용. 장착 구멍 4개는 중심 간격 80.44 × 60.66mm"),
        Part("R60", "Device", "R", "100R", {"1": N("DISPLAY_BL_PWM"), "2": "DISPLAY_BLK"},
             note="BLK 직렬. BLK는 모듈 기판 위 승압 LED 구동기의 제어 입력이다(도면 V01: 헤더 전원은 VDD 3.3V뿐, LED는 "
                  "6직렬 2병렬 17.1~19.8V). 극성·풀업·허용 PWM 주파수는 실물 확인"),
        Part("R61", "Device", "R", "100k", {"1": "DISPLAY_BLK", "2": "GND"}, note="부팅 중 백라이트 꺼짐"),
        Part("C60", "Device", "C", "10u 10V", {"1": "3V3_AUX", "2": "GND"},
             note="백라이트 LED 6직렬 2병렬, VF 19.2V 40mA. 3.3V 쪽 약 0.27A(승압 효율 85% 가정)"),
        Part("C61", "Device", "C", "100n", {"1": "3V3_AUX", "2": "GND"}, note="J22 핀 2 가까이"),
    ])


ENCODERS = ["ENC1", "ENC2", "ENC3", "ENC4", "ENC5", "ENC6", "NAV"]
# (확장기, 첫 GPIO 핀 번호): A, B, 누름이 연속 세 핀. GPA7(28)·GPB7(8)은 출력 전용이라 쓰지 않는다
ENCODER_PINS = [("U20", 21), ("U20", 24), ("U20", 1), ("U20", 4), ("U21", 21), ("U21", 24), ("U21", 1)]


def _expander(ref: str, address_a0: str, interrupt: str, reset_pull: str, decoupling: str) -> list[Part]:
    nets = {str(n): "NC" for n in range(1, 29)}
    nets.update({"9": "3V3_AUX", "10": "GND", "12": N("I2C1_SCL"), "13": N("I2C1_SDA"),
                 "15": address_a0, "16": "GND", "17": "GND", "18": f"{ref}_RESET", "20": interrupt})
    for name, (owner, first) in zip(ENCODERS, ENCODER_PINS):
        if owner == ref:
            nets.update({str(first): f"{name}_A", str(first + 1): f"{name}_B", str(first + 2): f"{name}_SW"})
    address = "0x21" if address_a0 == "3V3_AUX" else "0x20"
    return [
        Part(ref, "Interface_Expansion", "MCP23017_SO", "MCP23017-E/SO", nets,
             note=f"I2C {address}. 데이터시트 DS20001952C 표 2-1 핀. INTA에 두 포트를 묶는다(IOCON.MIRROR). "
                  "누름 입력은 내부 풀업(GPPU)"),
        Part(reset_pull, "Device", "R", "10k", {"1": "3V3_AUX", "2": f"{ref}_RESET"}, note="RESET 풀업"),
        Part(decoupling, "Device", "C", "100n", {"1": "3V3_AUX", "2": "GND"}, note=f"{ref} 핀 9 가까이"),
    ]


def _encoder(index: int, name: str) -> list[Part]:
    resistor = 100 + index * 4
    capacitor = 100 + index * 2
    parts = [Part(f"SW{20 + index}", "Device", "RotaryEncoder_Switch", f"EC11E 누름 {name}", {
        "A": f"{name}_A_RAW", "B": f"{name}_B_RAW", "C": "GND", "S1": f"{name}_SW", "S2": "GND"},
        note="ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다")]
    for offset, line in enumerate(("A", "B")):
        parts += [
            Part(f"R{resistor + offset * 2}", "Device", "R", "10k", {"1": "3V3_AUX", "2": f"{name}_{line}_RAW"},
                 note="ALPS 권장 필터 풀업"),
            Part(f"R{resistor + offset * 2 + 1}", "Device", "R", "10k", {"1": f"{name}_{line}_RAW", "2": f"{name}_{line}"},
                 note="ALPS 권장 필터 직렬"),
            Part(f"C{capacitor + offset}", "Device", "C", "10n", {"1": f"{name}_{line}", "2": "GND"},
                 note="ALPS 권장 필터 0.01µF. 10k와 16kHz"),
        ]
    return parts


def _controls_block() -> Block:
    parts = _expander("U20", "GND", N("MCP_INT_A"), "R98", "C93")
    parts += _expander("U21", "3V3_AUX", N("MCP_INT_B"), "R99", "C94")
    parts += [
        Part("R96", "Device", "R", "4.7k", {"1": "3V3_AUX", "2": N("I2C1_SCL")}, note="I2C 풀업"),
        Part("R97", "Device", "R", "4.7k", {"1": "3V3_AUX", "2": N("I2C1_SDA")}, note="I2C 풀업"),
    ]
    for index, name in enumerate(ENCODERS):
        parts += _encoder(index, name)
    for index in range(3):
        footswitch, led = N(f"FS{index + 1}"), N(f"LED{index + 1}")
        parts += [
            Part(f"SW{10 + index}", "Switch", "SW_Push", f"풋스위치 {index + 1} 모멘터리", {"1": footswitch, "2": "GND"}),
            Part(f"R{90 + index}", "Device", "R", "10k", {"1": "3V3_AUX", "2": footswitch}, note="풀업. 1호 R22와 같음"),
            Part(f"C{90 + index}", "Device", "C", "100n", {"1": footswitch, "2": "GND"}, note="디바운스 보조. 1호 C15와 같음"),
            Part(f"R{93 + index}", "Device", "R", "470R", {"1": led, "2": f"FSLED{index + 1}_A"}, note="약 2.8mA"),
            Part(f"D{20 + index}", "Device", "LED", f"3mm 풋스위치 {index + 1}", {"1": "GND", "2": f"FSLED{index + 1}_A"}),
        ]
    return Block("[조작부] G2. 인코더 7개, 풋스위치 3개, LED 3개 (설계서 3절·5.2절)", parts)


def _flags_block() -> Block:
    return Block("[메인] I2. ERC용 전원 플래그. GND는 U2 VOUT이 구동한다", [
        Part("PF1", "power", "PWR_FLAG", "PWR_FLAG", {"1": "VIN_RAW"}),
        Part("PF2", "power", "PWR_FLAG", "PWR_FLAG", {"1": "V9"}),
        Part("PF3", "power", "PWR_FLAG", "PWR_FLAG", {"1": "VRAIL"}),
        Part("PF4", "power", "PWR_FLAG", "PWR_FLAG", {"1": "SEED_3V3A"}),
        Part("PF6", "power", "PWR_FLAG", "PWR_FLAG", {"1": "SEED_3V3D"}),
    ])


def _socket_block() -> Block:
    return Block("[메인] H2. Daisy Seed3 소켓 (설계서 5.3절, pinmap.PLATFORM)", [_seed_socket()])


PLATFORM_BLOCKS = [_inherit(block) for block in BLOCKS[:6]] + [
    _socket_block(),
    _midi_block(),
    _exp_block(),
    _sd_block(),
    _stereo_block(),
    Block("[메인] Q1. 기판 연결 메인 쪽 (설계서 4절)", [_link("J20", "조작부 J21과 1:1. 접지는 이 헤더 한 곳에서만 잇는다")]),
    _flags_block(),
    Block("[조작부] Q2. 기판 연결 조작부 쪽 (설계서 4절)", [_link("J21", "메인 J20과 1:1")]),
    _aux_supply_block(),
    _display_block(),
    _controls_block(),
]


def render_schematic(library: Library, out: Path = HERE) -> None:
    render(library, out, blocks=PLATFORM_BLOCKS, name=NAME, title=TITLE, doc="bass-multi-platform-schematic.md")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--libs", help="KiCad .kicad_sym 폴더. 없으면 SPICE만 쓴다")
    args = parser.parse_args()
    for path in gen_spice.generate(SPICE_DIR, blocks=PLATFORM_BLOCKS, prefix=NAME):
        print(path)
    if args.libs:
        render_schematic(Library(Path(args.libs)))


if __name__ == "__main__":
    main()
