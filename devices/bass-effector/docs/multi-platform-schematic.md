# 멀티 이펙터 플랫폼 회로 설계

설계서: [multi-platform-design.md](multi-platform-design.md)
회로도 파일: [devices/bass-effector/hardware/platform.kicad_sch](../hardware/platform.kicad_sch)
넷리스트(자동 생성): [devices/bass-effector/hardware/platform-netlist.md](../hardware/platform-netlist.md)
최종 갱신: 2026-10-28

설계서 4절·5.2절의 블록을 부품 값과 결선으로 푼다. 지정자와 결선의 기준은 `devices/bass-effector/hardware/platform_blocks.py`의
`PLATFORM_BLOCKS`이고, 이 문서는 값의 근거를 적는다. 둘이 어긋나면 생성기를 고치고 이 문서를 맞춘다.
블록 제목 앞의 `[메인]`·`[조작부]`가 부품이 놓이는 기판이다. 생성기 `check()` 결과(2026-10-28): 부품 192개, 넷 127개,
핀 접속 548개, 검사 통과.

---

## 1. 계승 블록 A~F (메인)

[schematic.md](schematic.md) 1~6절의 값과 결선 그대로다. 다른 점은 두 가지다.

- Seed3 핀 넷 이름을 `pinmap.net` 규칙으로 바꿨다. `SEED_D0_K1DRV` → `SEED_D0_K1_DRIVE`, `SEED_A5_VMON` → `SEED_D20_VMON_ADC`.
  물리 핀은 같다.
- 출력 잭 J2를 TS에서 TRS로 바꿔 블록 P로 옮겼다(4절).
- 블록 A2로 C37 1000µF 25V를 V9–GND에 더했다. C7과 합쳐 2000µF. 플랫폼 부하에서 전원 차단 뒤 K1을 놓을 시간을 확보한다(설계서 5.4절). 높이 20mm 이하 품번, 풋스위치 아래를 피해 배치. 테스트 `test_bulk_holds_v9_long_enough_to_release_k1`이 이 조건을 지킨다.

## 2. Seed3 소켓 H2 (메인)

J4 핀은 Seed3 데이터시트 v2.1.0 표 2의 물리 번호이고 배정은 설계서 5.3절 표다. 그 밖에 16 Audio In 1, 18 Audio Out 1,
19 Audio Out 2(블록 P), 20·40 GND, 21 3V3A(EXP 기준), 38 3V3D(MIDI 회로), 39 VIN이다. 17 Audio In 2, D9, D28~D30은 비운다.

## 3. MIDI IN·OUT, EXP, microSD (메인)

| 블록 | 부품 | 값 | 근거 |
|---|---|---|---|
| M IN | J10 | 3.5mm TRS | MMA RP-054: 팁 = DIN 5(싱크), 링 = DIN 4(소스), 슬리브 = DIN 2. 수신 쪽 실드는 잇지 않는다 |
| | R40 | 220Ω | CA-033 그림 2 RB |
| | D30 | 1N4148 | CA-033 그림 2 D1(1N914급). LED와 역병렬 |
| | U11 | H11L1 | H11L1M 데이터시트: VCC 3~15V, IF(on) 최대 1.6mA. Daisy Pod Rev5 MIDI 입력과 같은 계열 |
| | R41 | 270Ω | 출력 풀업 SEED_3V3D. Daisy Pod Rev5 R64와 같은 값 |
| | C40 | 100nF | U11 전원 |
| M OUT | J11 | 3.5mm TRS | RP-054. 송신 쪽 슬리브는 GND |
| | R42 | 33Ω 0.5W | CA-033 그림 1 RA, 3.3V ±5% 값. 3V3_AUX에서 링(DIN 4)으로. TS 플러그가 링을 GND에 붙이면 100mA(0.33W)라 Seed3 3V3D 대신 2A 보조 레일을 쓴다 |
| | R43 | 10Ω | CA-033 그림 1 RC, 3.3V ±5% 값. USART1 TX에서 팁(DIN 5)으로 |
| N EXP | J12 | 6.35mm TRS | 팁 와이퍼, 링 기준, 슬리브 GND. 2버튼 풋스위치를 꽂으면 팁–슬리브가 1번, 링–슬리브가 2번 |
| | R50 | 10k | 링 기준 직렬. 꽂힌 페달의 최대값이 R/(R + 10k)라 10k 페달 50%, 100k 90.9%, 250k 96.2%. 문턱 97%(`EXP_UNPLUGGED_THRESHOLD`) |
| | R51 | 100k | 팁 풀업. 뺐을 때 ADC가 전체 눈금이 되어 꽂힘을 핀 없이 구분한다. 펌웨어 문턱은 97% |
| | R52, C50 | 1k, 100nF | ADC 직렬과 필터, 1.6kHz |
| | R59, C53 | 1k, 100nF | 링 감지 ADC(D28) 직렬과 필터. 링 전압으로 꽂힌 것을 구분한다: 익스프레션 페달이면 R/(R+10k)로 일정(10k 50%, 250k 96.2%), 2버튼 풋스위치면 떼면 100%·밟으면 0%, 빈 잭이면 100%. 펌웨어 규칙: 링이 98% 아래에서 일정하면 익스프레션 페달, 아니면 팁·링이 0%로 떨어지는 것을 각각 1·2번 발판 밟음으로 본다. 빈 잭과 밟지 않은 풋스위치는 구분할 필요가 없다(둘 다 입력 없음) |
| O microSD | J13 | microSD 소켓 | KiCad `Connector:Micro_SD_Card`. Connector_Card 라이브러리는 9.0.7에 없다 |
| | R53 | 33Ω | CLK 직렬 |
| | R54~R58 | 47k | CMD, DAT0~3 풀업 3V3_AUX. CLK는 풀업하지 않는다 |
| | C51, C52 | 100nF, 10µF | 소켓 전원 |

## 4. 출력 잭과 스테레오 자리 P (메인)

J2는 6.35mm TRS다. 팁 OUT_TIP(K1 경유, 1호와 같다), 링 OUT2_TIP, 슬리브 GND. 링 쪽 출력단은 블록 E와 같은 값
(C70 1µ, R71 100k, U3A ×5.7, R72 47k, R73 10k, C71 10µ, R70 100Ω, C72 10µ, R74 100k)이고 모두 DNP(자리만)다.
U3B는 버퍼로 VBIAS에 묶는다. 링은 K1을 거치지 않아 바이패스 때도 OUT2 출력단에 붙어 있다. 모노 TS 플러그는 링을 GND에
붙이므로 부품을 얹을 때는 R70이 단락 전류를 막는다.

## 5. 기판 연결 Q (메인 J20, 조작부 J21)

2×10 2.54mm 헤더, 1:1. 접지는 이 헤더에서만 두 기판을 잇는다.

| 핀 | 넷 | 핀 | 넷 |
|---|---|---|---|
| 1 | V9 | 2 | GND |
| 3 | 3V3_AUX | 4 | GND |
| 5 | SPI1 SCK | 6 | SPI1 MOSI |
| 7 | SPI1 NSS | 8 | DISPLAY_DC |
| 9 | DISPLAY_RST | 10 | DISPLAY_BL_PWM |
| 11 | I2C1 SCL | 12 | I2C1 SDA |
| 13 | MCP_INT_A | 14 | MCP_INT_B |
| 15~17 | FS1~3 | 18~20 | LED1~3 |

V9는 조작부 강압기로 가고 3V3_AUX는 메인의 microSD로 돌아온다.

## 6. 보조 3.3V L (조작부)

TI TPS562208(데이터시트 SLVSD91D, TPS562201과 같은 문서). VIN 4.5~17V(절대 최대 19V), 2A, 580kHz, 강제 연속 모드라 경부하에서도
주파수가 오디오 대역으로 내려가지 않는다(TPS562201은 펄스 스킵이라 뺐다). V9 입력은 FB3 + C36 47µF 파이 필터로 받아 입력 리플과
백라이트 PWM 전류가 메인 기판 V9로 돌아가지 않게 한다. KiCad에는 이 심볼이 없어
핀 배치가 같은 TPS562200 심볼을 쓴다(표 4-1: 1 GND, 2 SW, 3 VIN, 4 VFB, 5 EN, 6 VBST).

| 부품 | 값 | 근거 |
|---|---|---|
| FB3, C36 | 600Ω@100MHz 1A, 47µF 25V | 입력 파이 필터 |
| C30, C31 | 10µF 25V, 100nF | 7.2.2.4절: 입력 10µF 이상, 최대 VIN 이상 정격 |
| R80 | 100k | EN을 V9로 |
| C32 | 100nF | VBST–SW 부트스트랩, 7.2.2.5절 |
| L10 | 3.3µH | 표 7-2(3.3V 출력, 최대 4.7µH). 포화 전류 3A 이상 |
| C33, C34 | 22µF ×2 | 표 7-2 출력 20~68µF |
| R81, R82 | 33.2k, 10k 1% | 표 7-2. VOUT = 0.768 × (1 + 33.2/10) = 3.32V |
| FB2, C35 | 600Ω@100MHz 1A, 10µF | 스위칭 잡음을 조작부 안에 가둔다 |

## 7. 화면 K (조작부)

Orient AFK320480A0-3.5INTH-ANO(데이터시트 V01, 20쪽 도면). 4선 SPI 전용, 1 GND, 2 VDD(2.5~3.6V), 3 SCL, 4 SDA, 5 RST,
6 DC, 7 CS, 8 BLK. 도면에서 읽은 것:

- 모듈 기판 82.94 × 65.66mm(유리 54.66 위아래로 헤더 띠), PCB 1.6mm, 전체 최대 7.74mm. 패널 FPC가 한쪽 변을 돌아 3.50±0.4mm 나온다.
- 2.54mm 8핀 헤더가 아래 변(1~8, 신호)과 위 변(9~16, 모두 NC)에 있다. 조작부 기판에는 J22(신호)와 J23(지지)을 둔다.
- 장착 구멍 4개, 중심 간격 80.44 × 60.66mm. 지름은 도면 해상도로 Ø2.2로 읽힌다(M2).
- 표시 영역 73.44 × 48.96mm. 패널 창은 1mm 더한 74.44 × 49.96mm(`designs/platform_1590r1.py`).
- LED 회로: LEDA·LEDK 6직렬 2병렬, 40mA, 17.1~19.8V. 헤더 전원은 VDD 3.3V뿐이므로 승압 LED 구동기가 모듈 기판에 있고
  BLK는 그 제어 입력이다. 외부 스위치를 두지 않는다.

R60 100Ω은 BLK 직렬, R61 100k는 부팅 중 백라이트를 끈다(모듈에 풀업이 있으면 그 값에 따라 켜진 채로 남을 수 있다).
C60 10µF, C61 100nF는 모듈 전원.

## 8. 인코더, 풋스위치, LED G2 (조작부)

- MCP23017-E/SO ×2(데이터시트 DS20001952C 표 2-1). U20은 A0~A2 GND로 0x20, U21은 A0만 3V3_AUX로 0x21. RESET 10k 풀업
  (R98, R99), 100nF(C93, C94). INTA를 MCP_INT_A·B로 내고 IOCON.MIRROR로 두 포트를 묶는다. INTB는 비운다.
- GPA7·GPB7은 출력 전용이라 입력으로 쓰지 않는다.
- 인코더 배정(A, B, 누름이 연속 세 핀): U20 GPA0~2 E1, GPA3~5 E2, GPB0~2 E3, GPB3~5 E4. U21 GPA0~2 E5, GPA3~5 E6, GPB0~2 NAV.
- 인코더 A·B는 ALPS EC11E 권장 필터: 10k 풀업, 10k 직렬, 10nF. C는 GND. 누름은 MCP 내부 풀업(GPPU, 5V에서 40~115µA).
- I2C 풀업 4.7k ×2(R96, R97).
- 풋스위치 SW10~12: 10k 풀업 + 100nF(1호 R22·C15와 같음). LED D20~22: 470Ω, 약 2.8mA.

## 9. ERC

M4 KiCad 10.0.7 `kicad-cli sch erc --severity-all`(2026-10-29): `platform.kicad_sch`와 `bass-compressor.kicad_sch` 모두
오류 0건, 경고 0건. 회로도와 같은 폴더에 KiCad 9.0.7 심볼 파일을 가리키는 `sym-lib-table`을 두고 돌렸다. 없으면 심볼마다
"라이브러리가 설정에 없음" 경고가 나온다. 처음 돌렸을 때 강압기 입력 페라이트 FB3 뒤 `BUCK_VIN`에 PWR_FLAG가 없어 U10 VIN이
구동되지 않는다는 오류가 나와 PF8을 더했다. 같은 누락은 테스트 `test_every_power_input_net_is_driven`이 막는다.

## 10. 확인 필요

| 항목 | 이유 |
|---|---|
| 화면 모듈 BLK 극성, 모듈 쪽 풀업, 허용 PWM 주파수 | 도면으로 구동기가 모듈에 있다는 것까지 알 수 있다. 펌웨어는 20kHz 이상(오디오 대역 밖)으로 시작하고 실물에서 밝기 선형성 확인 |
| 표시 영역 중심과 모듈 기판 중심의 차이, FPC 돌출 방향 | 도면 해상도로 판독 불가. 기구 모델은 가운데 정렬, 돌출은 양쪽으로 잡았다 |
| 백라이트 승압 효율, microSD 실소모 | 전류 예산(설계서 5.4절)의 가정값 |
| H11L1 출력 풀업 270Ω에서 3.3V 상승 시간 | CA-033은 3.3V 수신 풀업 값을 주지 않는다. Daisy Pod 값을 따랐다 |
| TPS562208 EN 내부 풀업 유무 | R80으로 어느 쪽이든 켜지게 했다 |
| 새 블록 SPICE 테스트벤치(EXP 분압, 보조 3.3V 입력 필터) | 넷리스트 생성까지만 했다 |
