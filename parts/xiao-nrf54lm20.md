# Seeed XIAO nRF54LM20A / Sense

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
미보유. 입고 예정이다. Sense 1개와 기본 1개. 도착 전이라 실물 검증은 전무하다.
최종 갱신: 2026-08-31

## 스펙

출처: [Seeed Studio Wiki](https://wiki.seeedstudio.com/xiao_nrf54lm20a_getting_started/),
[Nordic nRF54LM20A](https://www.nordicsemi.com/Products/nRF54LM20A),
[CNX Software](https://www.cnx-software.com/2026/06/30/xiao-nrf54lm20a-sense-board-enables-ultra-low-power-bluetooth-6-0-matter-thread-zigbee-or-2-4-ghz-proprietary-applications/)

| 항목 | 값 |
|---|---|
| MCU | Nordic nRF54LM20A, Cortex-M33 128 MHz (DSP, FPU) |
| 보조 코어 | RISC-V 코프로세서 (칩 사양) |
| RAM | 512 KB |
| NVM | 온칩 2 MB, 외장 8 MB |
| 노출 GPIO | 28. 관통홀과 캐스털레이티드 패드가 섞여 있다. 제약 4 참조 |
| 무선 | BLE 6.0, Channel Sounding, Matter, Thread, Zigbee, NFC, 2.4GHz 독자 최대 4 Mbps |
| USB | Type-C |
| 전원 관리 | nPM1300 PMIC 탑재 |
| 안테나 | IPEX 커넥터 |
| 소비전류 | 딥슬립 4.76 µA, Ship Mode 0.33 µA, 무선 동작 평균 3.87 mA (3.7V, TX +8 dBm) |
| 공정 | 22 nm |
| 가격 | Sense $14.90 |

Sense 변종이 추가하는 것: 6축 IMU, PDM 마이크.
IMU 부품번호는 모름. 자료에 "six-axis IMU"로만 나온다.

## nRF52840과의 차이

| | nRF52840 | nRF54LM20A |
|---|---|---|
| 코어 | Cortex-M4F 64 MHz | Cortex-M33 128 MHz |
| RAM | 256 KB | 512 KB |
| NVM (온칩) | 1 MB | 2 MB |
| 노출 GPIO (XIAO) | 11 (전부 관통홀) | 28 (관통홀과 캐스털레이티드 혼합) |
| BLE | 모름 (5.x) | 6.0 |
| 2.4GHz 최대 | 2 Mbps | 4 Mbps |
| PMIC | BQ25101 (충전만) | nPM1300 (전원관리 전반) |
| 공정 | 40 nm | 22 nm |

Nordic 공식 표현으로 처리 성능 2배, 처리 효율 3배.

### nRF54L15와의 차이

미보유. 참고용이다.

| | nRF54L15 | nRF54LM20A |
|---|---|---|
| NVM / RAM | 1.5 MB / 256 KB | 2 MB / 512 KB |
| USB | 없음 | High-speed |
| GPIO (칩) | - | 66 / 40 / 32 (패키지별) |

코어와 라디오, 전력 특성은 같다. LM20은 메모리가 큰 상위 모델에 USB가 붙은 것이다.
L15였다면 USB가 없어 유선 HID 프로토타입 자체가 불가능했다.

## 제약

확인된 것만 적는다.

1. 아직 없다. 입고 전이라 모든 항목이 문서 기준이고 실물 검증이 0이다.
2. WiFi는 온보드가 아니다. 칩 사양상 WiFi는 nRF70 시리즈 컴패니언 IC가 있어야 한다.
   XIAO 보드에 nRF70이 탑재됐는지는 모름.
   자료의 "WiFi 지원" 표기는 칩 계열 설명일 가능성이 높다.
3. IPEX 안테나 커넥터가 있다. 온보드 칩 안테나가 함께 있는지, 외장 안테나가 필수인지는 모름.
4. GPIO 28개가 전부 브레드보드에 꽂히는 것은 아니다. Seeed 자료는 헤더를 2×13으로 적고
   관통홀과 캐스털레이티드 패드가 번갈아 배치된다고 서술한다. 캐스털레이티드 패드는 보드를
   다른 PCB에 얹어 납땜해야 닿으므로 브레드보드 단계에서는 쓸 수 없다.
   관통홀로 나오는 GPIO가 몇 개인지는 모름. 실물이 오면 센다.

## 툴체인

| 스택 | 상태 |
|---|---|
| Zephyr | 문서상 지원. nRF54LM20 DK 보드가 Zephyr에 등록돼 있다 |
| nRF Connect SDK | 문서상 지원. Nordic 공식 디바이스 가이드가 있다 |
| ZMK | 모름. ZMK 공식 하드웨어 문서에서 nRF54L 계열 지원 표기를 찾지 못했다 |
| CircuitPython / Arduino | 모름 |

ZMK 항목이 이 부품의 가장 큰 미지수다.
컨트롤러 설계가 ZMK를 지목하고 있는데 nRF54L에서 도는지 확인되지 않았다.
도착 후 첫 확인 항목이다.

## 디버깅 경로

전부 모름. SWD 패드 노출 여부, 위치, Debug Probe 호환성 모두 실물 확인이 필요하다.

## 쓰이는 기기

- [devices/split-controller.md](../devices/split-controller.md). 도착 후 판단한다. 현재는 nRF52840으로 진행
