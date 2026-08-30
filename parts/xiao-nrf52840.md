# Seeed XIAO nRF52840 / Sense

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
최종 갱신: 2026-08-30

## 스펙

출처: [Seeed Studio Wiki, XIAO BLE](https://wiki.seeedstudio.com/XIAO_BLE/)

| 항목 | 값 |
|---|---|
| MCU | Nordic nRF52840, Cortex-M4F 32bit, 64 MHz |
| RAM | 256 KB |
| Flash | 내장 1 MB, 온보드 외장 2 MB |
| 노출 GPIO | 11 (전부 PWM 가능) |
| 아날로그 입력 | 6 |
| 인터페이스 | UART ×1, I2C ×1, SPI ×1, NFC ×1, SWD ×1 |
| USB | Type-C |
| 배터리 충전 | BQ25101 내장. 50 mA / 100 mA 선택 (Pin 13) |
| 크기 | 21 × 17.8 mm |
| 온보드 | RGB 3-in-1 LED, 충전 상태 LED |

Sense 변종이 추가하는 것: 6축 IMU `LSM6DS3TR-C`, PDM 디지털 마이크.
Plus 변종(미보유): GPIO 20, UART ×2, SPI ×2, I2S ×1.

## 보유 현황

| 품목 | 수량 | 상태 |
|---|---|---|
| XIAO nRF52840 (논-Sense) | 1 | 보유 |
| XIAO nRF52840 Sense | 1 | 보유 |

컨트롤러 정식 구성은 Sense 2개를 전제로 하므로 1개 부족하다.
다만 IMU를 안 쓰는 프로토타입은 현 보유분으로 가능하다.

## 제약

확인된 것만 적는다.

1. 노출 GPIO가 11개뿐이다. 칩 자체는 48 GPIO지만 XIAO 폼팩터에서 나오는 것은 11개다.
   컨트롤러 오른손 배치(버튼 8, 조이스틱 X/Y 2)가 10핀을 쓰므로 여유가 1핀이다.
   계산은 [devices/split-controller.md](../devices/split-controller.md)에 있다.
2. WiFi가 없다. 무선은 BLE, 802.15.4, 2.4GHz 독자 프로토콜뿐이다.
3. BLE 버전은 모름. Seeed 위키에서 정확한 표기를 찾지 못했다.

## 툴체인

| 스택 | 상태 |
|---|---|
| CircuitPython | 확인 필요. 직접 안 돌려봤다 |
| Arduino (Seeed nRF52 보드패키지) | 확인 필요 |
| Zephyr / nRF Connect SDK | 확인 필요 |
| ZMK | 확인 필요. nRF52840은 ZMK가 널리 쓰는 칩이지만 XIAO 보드 정의 유무는 확인 안 함 |

"검증됨"이 하나도 없는 것은 아직 한 번도 안 켜봤기 때문이다.
첫 작업에서 하나를 검증하면 이 표부터 갱신한다.

## 디버깅 경로

- SWD 패드가 노출돼 있다. 출처는 Seeed 위키. SWDIO와 SWDCLK를 연결하면 디버깅과 부트로더 재기록이 된다.
- Raspberry Pi Debug Probe는 CMSIS-DAP SWD라 원리상 붙는다. 실제 연결은 안 해봤다.
- UART 1채널이 있다. 핀 위치는 확인 안 함.
- SWD 패드의 물리적 위치와 크기는 모름. 실물로 확인해야 한다.

## 쓰이는 기기

- [devices/split-controller.md](../devices/split-controller.md). 분리형 무선 게임패드
