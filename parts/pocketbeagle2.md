# PocketBeagle 2 (TI AM6254)

부품 기준 문서. 확인된 것만 적는다. 모르는 것은 "모름"으로 남긴다.
다른 부품과 달리 이 보드는 실물 검증이 상당히 진행돼 있다. 아래 "검증됨"은 직접 돌려본 것이다.
최종 갱신: 2026-09-02

## 문서 기준은 pocket-beagle2 저장소

브링업 문서 일체는 pocket-beagle2 저장소가 기준이다. 작업 사본은 WSL `~/pocket-beagle2/`.
이 파일에 복제하지 않는다.

| 문서 | 내용 |
|---|---|
| `roadmap.md` | 브링업 0~8단계. 단계별 완료 기준 |
| `applications.md` | 오디오 기기(플레이어, 이펙터) 계획 |
| `00-setup` ~ `12-to-linux` | 구조 사실 13장 |

## 스펙

| 항목 | 값 |
|---|---|
| SoC | TI AM6254 (K3 패밀리) |
| 보드 리비전 | Rev A1 |
| 코어 구성 | A53(리눅스), R5(부트 1패스), M4F, PRU |
| 오디오 | McASP0. `ACLKX`·`AFSX`(전송), `ACLKR`·`AFSR`(수신), `AXR0~3`(데이터 4채널) |
| 코덱 마스터 클럭 | `AUDIO_EXT_REFCLK0`, `AUDIO_EXT_REFCLK1` |
| 아날로그 입력 | `AIN0~AIN7` |
| 화면 | `SPI0`·`SPI1`·`SPI2`, 또는 `VOUT0_DATA0~15`와 `DE`·`HSYNC`·`VSYNC`·`PCLK` |
| USB | USB0은 Type-C, USB1은 헤더(`D+`·`D-`·`VBUS`·`DRVVBUS`) |
| 배터리 | `BQ21040` 리튬 충전기 온보드. `VBAT` 입력 별도 |
| 커널 | 보드 배포 이미지가 `PREEMPT_RT` |

## 제약

확인된 것만 적는다.

1. SWD를 지원하지 않는다. AM62x에는 SWD가 없다.
   따라서 Raspberry Pi Debug Probe의 SWD 포트는 커넥터를 바꿔도 이 보드에 쓸 수 없다.
2. WiFi와 Bluetooth가 없다. 사양표에 항목 자체가 없다. 무선은 USB1에 동글을 물려 얻는다.
3. `AIN0~AIN7`은 I2C를 거친다. 오디오 신호에는 못 쓰고 노브나 페달 같은 제어 입력 전용이다.
4. I2S 코덱이나 DAC가 없으면 오디오 작업을 시작할 수 없다. 현재 미보유.
5. 2패스 빌드다. R5 패스(`tiboot3.bin`)와 A53 패스(`tispl.bin`, `u-boot.img`)를 따로 빌드한다.
6. 이 보드로 연습되지 않는 것이 있다. 망가진 하드웨어 디버깅, 미성숙 실리콘,
   새 배선의 DDR 타이밍 산출이다. 검증된 양산품이라 "소프트웨어가 아니라 보드가 틀렸다"를
   밝히는 경험은 나오지 않는다. 출처는 `roadmap.md`.

## 툴체인

| 항목 | 값 | 상태 |
|---|---|---|
| R5 패스 크로스 컴파일러 | `arm-linux-gnueabihf-` | 검증됨 |
| A53 패스 크로스 컴파일러 | `aarch64-linux-gnu-` | 검증됨 |
| U-Boot | `beagleboard/u-boot`, `v2026.01-am62-pocketbeagle2` | 검증됨 |
| TF-A | `PLAT=k3 SPD=opteed ARCH=aarch64` (BL31) | 검증됨 |
| OP-TEE | BL32 | 검증됨 |
| binman | apt의 pyelftools 0.27은 너무 낡다. venv가 필요하다 | 검증됨 |
| 빌드 환경 | WSL2 Ubuntu-22.04. venv는 경로를 옮기면 깨진다 | 검증됨 |

## 디버깅 경로

| 경로 | 상태 |
|---|---|
| UART 콘솔 | 검증됨. `main_uart6` @ `0x02860000`, `ttyS2,115200n8` |
| USB DFU | 검증됨. SD를 뽑은 상태에서 VID:PID `0451:6165`로 열거된다 |
| WSL로 USB 넘기기 | 검증됨. `usbipd-win`으로 attach |
| JTAG | 미확보 |

DFU는 SD를 건드리지 않는 구제 경로다.

### JTAG

확보하지 않았다.

- 케이블은 다리 있는 Tag-Connect 10핀 `TC2050-IDC-050` 계열이다. 보드에 정렬 구멍 3개가 있다.
- 프로브 쪽 핀 배치가 표준 ARM Cortex-10과 7·9·10번이 다르다. TI XDS 계열은 TI용 어댑터로 연결한다.
- 판단: UART와 DFU가 둘 다 상태를 못 보여주는 구간에서 실제로 막혔을 때 확보한다.
  로드맵 3단계(DDR)가 UART 출력 이전 구간이라 그때가 유력하다.

## 진행 상황

브링업 1단계에서 1a부터 1e까지 완료했다. 1f(SD에 처음 쓰기)는 미착수다.
1c에서 1e까지는 AI가 수행한 구간이라 1a부터 손으로 재현하는 트랙이 따로 있다.
계획은 `~/pb2-repro/PLAN.md`. 이 트랙은 착수 전이고 RP2040 트랙을 먼저 진행 중이다.

미해결 항목이 하나 있다. 자체 빌드 `tiboot3.bin`이 문서 기록 282,506 B보다 860 B 크다.
실제 값은 283,366 B다. 재현 트랙 1b에서 결판낸다.

## 쓰이는 기기

- [devices/audio-player.md](../devices/audio-player.md). 브링업 완료 후 전환할 실사용 기기
- 오디오 이펙터. `applications.md`에 계획이 있다. 플레이어 다음 순서이며 미착수다
