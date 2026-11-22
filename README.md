# hardware

> 공부하면서 실제로 쓸 기기를 만드는 개인 하드웨어 저장소

RP2040 · nRF52840 · PocketBeagle 2 · Daisy Seed3 · CircuitPython · Verilog

키보드, 무선 컨트롤러, 오디오 기기, 베이스 이펙터를 설계하고, 그에 쓰는 보드를 베어메탈과 CircuitPython으로 익힙니다.

## 문서

| 위치 | 내용 |
|---|---|
| `devices/` | 기기별 폴더. 계획서·설계서와 펌웨어·회로를 한 폴더에, 문서가 하나면 `README.md` |
| `learning/` | 보드를 익히는 학습 트랙 |
| `parts/` | 부품 기준 문서. 스펙, 제약, 툴체인, 디버깅 경로, 데이터시트 출처. 여러 기기가 함께 참조 |
| `backups/` | 보드에서 채취한 공장 출하 플래시의 채취 기록과 해시. 덤프 원본은 비공개 저장소 |
| `inventory.md` | 보유 장비 목록 |

### 기기

| 기기 | 보드 | 문서 |
|---|---|---|
| TKL 87키 키보드 | RP2040 베어칩 | [계획](devices/tkl-keyboard/README.md), [설계](devices/tkl-keyboard/design.md) |
| 분리형 무선 컨트롤러 | XIAO nRF52840 ×2 | [계획](devices/split-controller/README.md) |
| 오디오 플레이어 | PocketBeagle 2 | [계획](devices/audio-player/README.md) |
| LDAC 송신 동글 | PocketBeagle 2, RPi 3B+ | [계획](devices/ldac-dongle/README.md) |
| 베이스 컴프레서 페달 | Daisy Seed3 | [계획](devices/bass-effector/docs/plan.md), [설계](devices/bass-effector/docs/design.md), [회로](devices/bass-effector/docs/schematic.md), [검증 도구](devices/bass-effector/docs/tools-design.md), [펌웨어](devices/bass-effector/) |
| 베이스 멀티 이펙터 플랫폼 | Daisy Seed3 | [설계](devices/bass-effector/docs/multi-platform-design.md), [회로](devices/bass-effector/docs/multi-platform-schematic.md) |
| 악보 페달 | XIAO nRF52840 | [설계](devices/page-pedal/README.md) |

### 학습 트랙

| 트랙 | 보드 | 문서 |
|---|---|---|
| RP2040 베어메탈 | XIAO RP2040 | [rp2040-baremetal](learning/rp2040-baremetal/) |
| RP2040 0 to 1 | XIAO RP2040 | [rp2040-zero-to-one](learning/rp2040-zero-to-one/notes.md) |
| CircuitPython | XIAO nRF52840 Sense | [nrf52840-circuitpython](learning/nrf52840-circuitpython/) |
| 칩 설계 | Tiny Tapeout(SKY130) | [chip-design](learning/chip-design/) |

### 부품

| 부품 | 문서 |
|---|---|
| PocketBeagle 2 | [parts/pocketbeagle2.md](parts/pocketbeagle2.md) |
| XIAO nRF52840 / Sense | [parts/xiao-nrf52840.md](parts/xiao-nrf52840.md) |
| XIAO nRF54LM20A / Sense | [parts/xiao-nrf54lm20.md](parts/xiao-nrf54lm20.md) |
| XIAO RP2040 | [parts/xiao-rp2040.md](parts/xiao-rp2040.md) |
| Raspberry Pi 3B+ | [parts/rpi-3bplus.md](parts/rpi-3bplus.md) |
| Arduino Uno 클론 | [parts/arduino-uno.md](parts/arduino-uno.md) |

## 기여

문서 표기 규칙은 `CONTRIBUTING.md`에 있습니다.
