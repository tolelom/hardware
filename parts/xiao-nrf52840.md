# Seeed XIAO nRF52840 / Sense

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
"문서 기준"은 벤더 문서나 공개 저장소에서 읽은 값이고 실물로는 안 해봤다는 뜻이다.
최종 갱신: 2026-09-07

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
3. BLE 버전은 Nordic 제품 브리프 v3.0 기준 Bluetooth 5.3이다. Seeed 위키에는 표기가 없다.
   보드에 실린 SoftDevice S140 7.3.0이 어느 버전으로 인증됐는지는 모름.
4. 배터리 전압 측정 경로가 있고 노출 GPIO를 쓰지 않는다. 아래 배터리 절에 있다.
   단 P0.14를 HIGH로 두면 P0.31에 최대 3.6V가 걸려 핀이 상할 수 있다. 출처는 Seeed 위키.

## 배터리 (문서 기준)

출처: Seeed 위키 XIAO_BLE 및 battery_charging_considerations. 실물로는 안 해봤다.

| 신호 | 핀 | 내용 |
|---|---|---|
| READ_BAT_ENABLE | P0.14 | LOW로 두면 분압 경로가 열린다. 읽는 동안 LOW 유지 |
| AIN7_BAT | P0.31 | ADC 입력. 배터리 전압의 약 1/3이 들어온다 |
| CHARGE_RATE | P0.13 | 충전 전류 선택. 50 mA / 100 mA |
| CHARGE_STATUS | P0.17 | 충전 상태 |

CircuitPython 보드 정의에는 `VBATT`(P0.31), `CHARGE_STATUS`, `CHARGE_RATE`가 있고
P0.14는 이름이 없어 `microcontroller.pin.P0_14`로 잡는다.
컨트롤러 계획서의 GPIO 예산에서 배터리 전압 측정은 이 경로를 쓰므로 핀을 소모하지 않는다.

## 툴체인

| 스택 | 상태 |
|---|---|
| CircuitPython | 문서 기준 지원. circuitpython.org에 `Seeed_XIAO_nRF52840_Sense` 보드 이미지가 있고 최신 안정판은 10.3.0이다(2026-09-07). 8.2.0 이상은 UF2 부트로더 0.6.1 이상을 요구하는데 보유 보드가 0.6.1이라 충족한다. 직접 안 돌려봤다 |
| Arduino (Seeed nRF52 보드패키지) | 확인 필요 |
| Zephyr / nRF Connect SDK | 문서 기준 지원. Zephyr 본가에 `xiao_ble` 보드가 등록돼 있다 |
| ZMK | 문서 기준 지원. ZMK 공식 하드웨어 목록에 `xiao_ble//zmk`가 있다 |

"검증됨"이 하나도 없는 것은 아직 한 번도 안 켜봤기 때문이다.
첫 작업에서 하나를 검증하면 이 표부터 갱신한다.

### CircuitPython 핀 이름 (문서 기준)

출처는 CircuitPython 저장소 `ports/nordic/boards/Seeed_XIAO_nRF52840_Sense/pins.c`.
Sense의 IMU와 마이크는 전원 핀이 따로 있고, IMU는 기본 I2C와 다른 버스에 달려 있다.
`board.I2C()`로 스캔하면 IMU가 안 보인다. 이것이 CircuitPython 트랙 1단계의 함정이다.

| 대상 | 핀 이름 | 물리 핀 |
|---|---|---|
| 기본 I2C (외부 핀) | `SDA`, `SCL` | P0.04, P0.05 |
| IMU 전원 | `IMU_PWR` | P1.08. HIGH로 켠다 |
| IMU I2C | `IMU_SDA`, `IMU_SCL` | P0.07, P0.27 |
| IMU 인터럽트 | `IMU_INT1` | P0.11 |
| 마이크 전원 | `MIC_PWR` | P1.10. HIGH로 켠다 |
| PDM | `PDM_CLK`, `PDM_DATA` | P1.00, P0.16 |
| RGB LED | `LED_RED`, `LED_GREEN`, `LED_BLUE` | P0.26, P0.30, P0.06 |

`audiobusio.PDMIn`이 소리에 반응하지 않고 상수만 채우는 문제는 `MIC_PWR`을 안 켜서 생긴다.
CircuitPython 이슈 #6602(2022)가 이 원인으로 닫혔다.

`datasheets/`의 회로도 3쪽과 대조했다. 넷 이름이 `INTERNAL_I2C_SCL`(P0.27),
`INTERNAL_I2C_SDA`(P0.07), `6D_PWR`(P1.08), `6D_INT1`, `P1.10_MIC_PWR`, `P1.00_PDM_CLK`,
`P0.16_PDM_DATA`, `P0.14_~READ_BAT`, `P0.31_AIN7_BAT`로 위 표와 맞는다.
LSM6DS3TR-C의 I2C 주소는 `0x6A`다. SA0가 GND에 묶여 있어 고정이다.

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

## 데이터시트

2026-09-01에 받아 `datasheets/`에 두었다. 출처는 전부 Seeed 위키(files.seeedstudio.com) 미러다.

| 문서 | 파일 | 비고 |
|---|---|---|
| nRF52840 Product Specification v1.5 | [datasheets/nrf52840_ps.pdf](datasheets/nrf52840_ps.pdf) | Nordic 공식 최신은 v1.11이나 직접 다운로드가 403이라 미러 v1.5를 받았다 |
| LSM6DS3TR-C 데이터시트 | [datasheets/lsm6ds3tr-c.pdf](datasheets/lsm6ds3tr-c.pdf) | TR-C 변종 문서임을 내용으로 확인했다 |
| PDM 마이크 MSM261D3526H1CPM | [datasheets/msm261d3526h1cpm-mic.pdf](datasheets/msm261d3526h1cpm-mic.pdf) | 부품번호는 Seeed 위키 기준. 실물 보드 리비전과 같은지는 확인 안 함 |
| 충전 IC BQ25101 | [datasheets/bq25101-charger.pdf](datasheets/bq25101-charger.pdf) | |
| XIAO nRF52840 회로도 | [datasheets/xiao-nrf52840-sense-schematic.pdf](datasheets/xiao-nrf52840-sense-schematic.pdf) | LSM6DS3TR-C가 포함돼 있어 Sense를 커버한다 |

## 이 보드로 하는 작업

- [nrf52840/](../nrf52840/). CircuitPython 학습 트랙. 미착수. 0, 1단계가 컨트롤러 계획서와 겹친다

## 쓰이는 기기

- [devices/split-controller.md](../devices/split-controller.md). 분리형 무선 게임패드
