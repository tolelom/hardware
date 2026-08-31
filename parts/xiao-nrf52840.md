# Seeed XIAO nRF52840 / Sense

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
최종 갱신: 2026-08-31

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

| 품목 | 수량 | USB 시리얼 |
|---|---|---|
| XIAO nRF52840 (논-Sense) | 1 | `EC977810429427B8` |
| XIAO nRF52840 Sense | 1 | `602E9B3FB161B682` |

두 보드는 USB 문자열이 같아 호스트에서 구분되지 않는다. 시리얼이 유일한 구분자다.
어느 쪽이 Sense인지는 포장 라벨과 외형 부품 차이로 판정했다. I2C로 IMU 응답을 확인하는
최종 확인은 하지 않았다.

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

## USB 식별과 부트로더

보드 2개를 Ubuntu 호스트에 연결해 확인했다. 두 보드의 값이 모두 같다.

| 모드 | USB ID | 인터페이스 | 저장장치 |
|---|---|---|---|
| 앱 | `2886:8045` | CDC 시리얼 | 없음 |
| 부트로더 | `2886:0045` | CDC + 대용량 저장장치 | `XIAO-SENSE` 32.1 MB |

부트로더 진입은 리셋 버튼을 두 번 빠르게 누른다. 부트로더가 보고하는 값은 이렇다.

| 항목 | 값 |
|---|---|
| 버전 | Adafruit UF2 Bootloader 0.6.1 |
| Board-ID | `Seeed_XIAO_nRF52840_Sense` |
| SoftDevice | S140 7.3.0 |
| 빌드 날짜 | 2021-11-12 |

Board-ID는 논-Sense 보드도 같은 값을 보고한다. 부트로더가 UF2를 받을 때 이 값을 대조하므로
논-Sense 보드에 Sense용 UF2를 올려도 통과한다. IMU와 마이크를 쓰는 코드가 그 하드웨어 없이
어떻게 실패하는지는 확인하지 않았다.

저장장치 볼륨 크기는 부트로더가 만들어 보이는 값이고 실제 플래시 용량과 무관하다.

SoftDevice는 Nordic이 바이너리로 배포하는 BLE 스택이다. CircuitPython과 Arduino 경로가
이것을 쓰고 Zephyr는 자체 BLE 스택을 쓴다. Zephyr로 갈 때 이 부트로더 구성을 바꾸게 되는지는
확인하지 않았다.

공장 출하 펌웨어는 RGB LED를 순환시키고 시리얼로는 아무것도 보내지 않는다.

## 디버깅 경로

- 부트로더가 플래시에 있어 지울 수 있다. 지우면 SWD로 복구해야 한다.
- SWD 패드가 노출돼 있다. 출처는 Seeed 위키. SWDIO와 SWDCLK를 연결하면 디버깅과 부트로더 재기록이 된다.
- Raspberry Pi Debug Probe는 CMSIS-DAP SWD라 원리상 붙는다. 실제 연결은 안 해봤다.
- UART 1채널이 있다. 핀 위치는 확인 안 함.
- SWD 패드의 물리적 위치와 크기는 모름. 실물로 확인해야 한다.
- 호스트가 Ubuntu일 때 dialout과 plugdev 그룹에 속해 있으면 별도 udev 규칙 없이 접근된다.

## 쓰이는 기기

- [devices/split-controller.md](../devices/split-controller.md). 분리형 무선 게임패드
