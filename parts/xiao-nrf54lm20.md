# Seeed XIAO nRF54LM20A / Sense

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
보유. Sense 1개와 기본 1개(2026-09-28 도착). 실물로 확인한 항목은 제약 절에 적는다.
최종 갱신: 2026-09-28

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

Sense 변종이 추가하는 것: 6축 IMU LSM6DS3TR-C, PDM 마이크 MSM261DGT006.
출처는 Seeed 위키 getting started와 Zephyr 보드 문서(2026-09-07). IMU가 nRF52840 Sense와
같은 부품이라 IMU 코드는 그대로 옮겨진다. 마이크는 부품번호가 다르다.
외장 플래시는 PY25Q64HA 64 Mbit(8 MB). Zephyr 보드 문서 기준.

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

1. 두 보드 모두 USB 전원 동작을 확인했다(2026-09-28). 공장 펌웨어가 빨간 LED를 깜빡이고,
   SWD로 DP와 CTRL-AP가 응답한다. 디버거 시리얼은 Sense `F2BA830C`, 기본 `971797F7`이고
   pyOCD `-u`로 보드를 고를 수 있다.
2. WiFi는 온보드가 아니다. 칩 사양상 WiFi는 nRF70 시리즈 컴패니언 IC가 있어야 한다.
   XIAO 보드에 nRF70이 탑재됐는지는 모름.
   자료의 "WiFi 지원" 표기는 칩 계열 설명일 가능성이 높다.
3. IPEX 안테나 커넥터가 있다. 외장 안테나는 동봉되지 않았다. 온보드 칩 안테나가 함께 있는지,
   외장 안테나가 필수인지는 모름.
4. GPIO 28개가 전부 브레드보드에 꽂히는 것은 아니다. Seeed 자료는 헤더를 2×13으로 적고
   관통홀과 캐스털레이티드 패드가 번갈아 배치된다고 서술한다. 캐스털레이티드 패드는 보드를
   다른 PCB에 얹어 납땜해야 닿으므로 브레드보드 단계에서는 쓸 수 없다.
   실물을 눈으로 보면 관통홀과 패드가 대략 반반이다. 정확한 개수는 아직 안 셌다.
5. 공장 펌웨어는 APPROTECT가 걸려 있다(AHB-AP disabled). 코어·메모리에 접근하려면
   CTRL-AP 전체 삭제가 필요하고 공장 펌웨어가 지워진다. Seeed의 공장 이미지 배포 여부는 모름.

## 툴체인

| 스택 | 상태 |
|---|---|
| Zephyr | 문서상 지원. Zephyr 본가에 `xiao_nrf54lm20a` 보드가 등록돼 있다(2026-09-07 확인) |
| nRF Connect SDK | 문서상 지원. Seeed 위키가 nRF Connect SDK와 PlatformIO(Zephyr) 경로를 권장한다 |
| ZMK | 모름. ZMK 공식 하드웨어 문서에서 nRF54L 계열 지원 표기를 찾지 못했다(2026-09-07 재확인) |
| MicroPython | Seeed가 튜토리얼을 제공한다고 적혀 있다. 내용은 안 봤다 |
| CircuitPython / Arduino | 모름. Seeed 포럼에 nrf54 Arduino 코어 언급이 있으나 공식 여부는 확인 안 함 |

ZMK 항목이 이 부품의 가장 큰 미지수다.
컨트롤러 설계가 ZMK를 지목하고 있는데 nRF54L에서 도는지 확인되지 않았다.
도착 후 첫 확인 항목이다. Zephyr 본가에 보드가 있으므로 ZMK가 Zephyr 버전을
따라잡는 시점에 열릴 가능성은 있다. 추측이다.

## 디버깅 경로

온보드 CMSIS-DAP 디버거가 있어 USB-C 하나로 플래시·디버그가 된다. Debug Probe는
필요 없다. 호스트에는 `2886:0068` "Seeed Studio XIAO nRF54LM20A CMSIS-DAP"(full-speed)로
잡히고 HID(CMSIS-DAP)와 CDC ACM 시리얼을 함께 낸다. 이 USB는 디버거 칩의 것이고
nRF54LM20A 자체의 HS USB가 아니다.

| 항목 | 내용 |
|---|---|
| udev | `/etc/udev/rules.d/99-xiao-nrf54-cmsis-dap.rules`. hidraw와 usb 두 서브시스템을 plugdev에 연다. pyOCD는 리눅스에서 pyusb 백엔드를 써서 usb 줄이 필요하다 |
| pyOCD | 0.45.1이 `nrf54lm20a`를 builtin 타깃으로 지원한다. 팩 불필요 |
| 주의 | pyOCD `auto_unlock`은 기본값이 켜짐이라 잠긴 칩을 만나면 묻지 않고 전체 삭제한다. 공장 펌웨어를 남기려면 `-O auto_unlock=false`를 붙인다 |

사용자 LED는 RGB(빨강 P1.22, 초록 P1.24, 파랑 P1.23)로 nRF54가 구동하고, 충전 LED는
nPM1300이 구동한다. Seeed 위키 기준.

## 쓰이는 기기

- [devices/split-controller.md](../devices/split-controller.md). 도착 후 판단한다. 현재는 nRF52840으로 진행
