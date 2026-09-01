# XIAO nRF52840 Sense CircuitPython 트랙

학습 트랙. CircuitPython으로 Sense의 IMU와 마이크를 갖고 놀고, 유선 USB HID까지 잇는다.
부품 기준 문서: [parts/xiao-nrf52840.md](../parts/xiao-nrf52840.md)
기기 계획서: [devices/split-controller.md](../devices/split-controller.md). 0, 1단계 판정이 이 트랙과 겹친다.
최종 갱신: 2026-09-01

확인된 것만 적는다. 안 해본 것은 "확인 필요", 모르는 것은 "모름"으로 남긴다.

## 왜 CircuitPython인가

바닥부터 가지 않는다. 2026-09-01에 판단했다.

1. 레지스터 수준 학습은 rp2040 트랙이 담당한다. 52840에서 반복하면 배움이 겹친다.
   동시에 굴릴 개수 제한(2개)도 있다.
2. 이 보드의 가치는 BLE, IMU, 배터리인데 BLE는 바닥부터가 사실상 불가능하다.
   Nordic 경로는 SoftDevice 바이너리 스택이고 오픈 경로는 Zephyr 스택이다.
   어느 쪽이든 스택 위에서 노는 구조다.
3. 컨트롤러 계획서의 펌웨어 1단계가 CircuitPython이다. 여기서 확정한 것이 그대로 이어진다.

저수준 욕심이 나면 Zephyr로 내려간다. 그것도 컨트롤러 계획의 기존 경로다.

## 단계

전 단계 USB 전원, 현 보유물만 쓴다. 추가 구매 없음.

| # | 단계 | 판정 기준 |
|---|---|---|
| 0 | CircuitPython 설치, 첫 구동 | 직접 올린 코드로 LED가 깜빡이고 시리얼에 출력이 나온다. 공장 펌웨어도 LED를 순환시키므로 LED만으로는 판정하지 않는다 |
| 1 | IMU 존재 확인 | I2C 스캔에 LSM6DS3TR-C가 응답한다. parts 문서의 "어느 보드가 Sense인지 I2C 최종 확인 안 함"이 해소된다 |
| 2 | IMU 놀이 | 기울기에 따라 RGB LED 색이 바뀌고, 흔들기를 감지한다. 컨트롤러 5단계(IMU 연동)의 선행 학습 |
| 3 | 마이크 놀이 | PDM 마이크로 소리 크기를 읽어 LED 밝기가 반응한다 |
| 4 | 유선 USB HID | 커스텀 게임패드 디스크립터로 PC가 게임패드로 인식하고, 점퍼선 입력 1개가 게임패드 테스터에 뜬다. 컨트롤러 1단계 판정과 동일 |

0단계 통과 시 parts 툴체인 표의 CircuitPython을 검증됨으로 갱신한다.
1단계 통과 시 parts 보유 현황의 Sense 판별 항목을 갱신한다.
4단계 통과 시 컨트롤러 계획서의 1단계를 완료로 갱신한다.

## 4단계 주의

CircuitPython 표준 `adafruit_hid` 라이브러리에 게임패드가 없다. 키보드와 마우스만 있다.
게임패드는 `boot.py`에서 `usb_hid.Device`로 HID 디스크립터를 직접 등록해야 한다.
이것이 컨트롤러 계획서 "배우는 것" 1번(USB HID 디스크립터)이라 피하지 않고 여기서 배운다.

## 확인 안 된 것

- CircuitPython 보드 이미지는 circuitpython.org에서 받는다. 어떤 버전을 쓸지, UF2가 실제로 올라가는지는 안 해봤다.
- Windows에서 CIRCUITPY 드라이브 인식 여부. 기존 확인은 전부 Ubuntu 호스트에서 했다.
- PDM 마이크의 CircuitPython 지원 범위(`audiobusio.PDMIn`)를 실물로 안 돌려봤다.

## 코드 배치

이 폴더에 단계별로 둔다. CircuitPython은 빌드가 없고 .py 복사가 전부라
rp2040 트랙과 달리 Windows에서 편집하고 실행까지 된다.
