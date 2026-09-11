# XIAO nRF52840 Sense CircuitPython 트랙

학습 트랙. CircuitPython으로 Sense의 IMU와 마이크를 다루고, 유선 USB HID까지 잇는다.
부품 기준 문서: [parts/xiao-nrf52840.md](../parts/xiao-nrf52840.md)
기기 계획서: [devices/split-controller.md](../devices/split-controller.md). 0, 1단계 판정이 이 트랙과 겹친다.
최종 갱신: 2026-09-11

확인된 것만 적는다. 안 해본 것은 "확인 필요", 모르는 것은 "모름"으로 남긴다.

## 왜 CircuitPython인가

바닥부터 가지 않는다.

1. 레지스터 수준 학습은 rp2040 트랙이 담당한다. 52840에서 반복하면 배우는 내용이 겹친다.
2. 이 보드가 rp2040과 다른 지점은 BLE, IMU, 배터리인데 BLE는 바닥부터가 사실상 불가능하다.
   Nordic 경로는 SoftDevice 바이너리 스택이고 오픈 경로는 Zephyr 스택이다.
   어느 쪽이든 스택 위에서 쓰는 구조다.
3. 컨트롤러 계획서의 펌웨어 1단계가 CircuitPython이다. 여기서 확정한 것이 그대로 이어진다.

더 낮은 층을 직접 다루려면 Zephyr로 내려간다. 그것도 컨트롤러 계획의 기존 경로다.

## 단계

전 단계 USB 전원으로 하고, 지금 있는 부품만 쓴다. 추가 구매 없음.

| # | 단계 | 판정 기준 |
|---|---|---|
| 0 | CircuitPython 설치, 첫 구동 | 직접 올린 코드로 LED가 깜빡이고 시리얼에 출력이 나온다. 공장 펌웨어도 LED를 순환시키므로 LED만으로는 판정하지 않는다 |
| 1 | IMU 존재 확인 | `IMU_SCL`·`IMU_SDA` 버스 스캔에 `0x6a`가 응답한다. parts 문서의 "어느 보드가 Sense인지 I2C 최종 확인 안 함"이 해소된다 |
| 2 | IMU 활용 | 기울기에 따라 RGB LED 색이 바뀌고, 흔들기를 감지한다. 컨트롤러 5단계(IMU 연동)의 선행 학습 |
| 3 | 마이크 입력 | PDM 마이크로 소리 크기를 읽어 LED 밝기가 반응한다 |
| 4 | 유선 USB HID | 커스텀 게임패드 디스크립터로 PC가 게임패드로 인식하고, 점퍼선 입력 1개가 게임패드 테스터에 뜬다. 컨트롤러 1단계 판정과 동일 |

0단계 통과 시 parts 툴체인 표의 CircuitPython을 검증됨으로 갱신한다.
1단계 통과 시 parts 보유 현황의 Sense 판별 항목을 갱신한다.
4단계 통과 시 컨트롤러 계획서의 1단계를 완료로 갱신한다.

## 0단계 절차

문서 기준이다. 실물로 통과하면 이 절을 "확인됨"으로 고친다.

1. circuitpython.org의 `Seeed_XIAO_nRF52840_Sense` 페이지에서 UF2를 받는다.
   2026-09-07 기준 최신 안정판은 10.3.0이고 파일명은
   `adafruit-circuitpython-Seeed_XIAO_nRF52840_Sense-en_US-10.3.0.uf2`다.
2. Sense 보드(USB 시리얼 `602E9B3FB161B682`)를 꽂고 리셋을 두 번 눌러 `XIAO-SENSE` 드라이브를 띄운다.
3. UF2를 드라이브에 복사한다. 보드가 재부팅되고 드라이브 이름이 `CIRCUITPY`로 바뀐다.
4. `CIRCUITPY`에 `code.py`를 둔다. `LED_RED`를 깜빡이며 `print`로 카운터를 찍는다.
5. CDC 시리얼 콘솔에 카운터가 올라가면 통과다. Windows에서는 COM 포트로 잡힌다.

부트로더 요건은 충족한다. CircuitPython 8.2.0 이상이 UF2 부트로더 0.6.1 이상을 요구하는데
보유 보드 둘 다 0.6.1이다. 값은 parts 문서에 있다.

## 1단계 함정

Sense의 IMU는 외부 핀의 기본 I2C(`board.I2C()`, P0.04/P0.05)가 아니라
`IMU_SDA`·`IMU_SCL`(P0.07/P0.27) 별도 버스에 달려 있고, `IMU_PWR`(P1.08)을 HIGH로
올려야 전원이 들어간다. 기본 버스를 스캔하면 아무것도 안 나오며 그것은 논-Sense라는
증거가 아니다. LSM6DS3TR-C의 7비트 주소는 `0x6a`다. SA0가 GND에 묶여 있어 바꿀 수
없고, Seeed 위키 예제와 Zephyr 디바이스 트리가 같은 값을 쓴다(2026-09-07 확인).

```python
import board, busio, digitalio, time
pwr = digitalio.DigitalInOut(board.IMU_PWR)
pwr.direction = digitalio.Direction.OUTPUT
pwr.value = True
time.sleep(0.1)
i2c = busio.I2C(board.IMU_SCL, board.IMU_SDA)
while not i2c.try_lock():
    pass
print([hex(a) for a in i2c.scan()])
i2c.unlock()
```

논-Sense 보드에서 같은 코드를 돌려 빈 목록이 나오면 두 보드의 판별이 양쪽에서 끝난다.
핀 이름의 출처는 parts 문서의 CircuitPython 핀 절이다.

## 3단계 함정

마이크도 `MIC_PWR`(P1.10)을 HIGH로 켜야 한다. 안 켜면 `audiobusio.PDMIn`이 소리에 반응하지
않고 상수만 채운다. CircuitPython 이슈 #6602가 이 원인으로 닫혔다. 핀은 `PDM_CLK`, `PDM_DATA`.

## 4단계 주의

CircuitPython 표준 `adafruit_hid` 라이브러리에 게임패드가 없다. 키보드와 마우스만 있다.
게임패드는 `boot.py`에서 `usb_hid.Device`로 HID 디스크립터를 직접 등록해야 한다.
이것이 컨트롤러 계획서 "배우는 것" 1번(USB HID 디스크립터)이라 여기서 배운다.

## 확인 안 된 것

- UF2가 실제로 올라가는지. 위 절차는 문서 기준이다.
- Windows에서 CIRCUITPY 드라이브와 COM 포트 인식 여부. 기존 확인은 전부 Ubuntu 호스트에서 했다.
- `audiobusio.PDMIn`을 이 보드에서 돌려보지 않았다. 지원 자체는 보드 정의에 PDM 핀이 있어 문서 기준으로는 된다.

## 코드 배치

이 폴더에 단계별로 둔다. CircuitPython은 빌드가 없고 .py 복사가 전부라
rp2040 트랙과 달리 Windows에서 편집하고 실행까지 된다.

보드가 실행하는 파일 이름은 `code.py`로 고정이라 단계별 폴더 안에 그 이름으로 둔다.
폴더째 `CIRCUITPY`에 복사하면 이름을 고칠 일이 없다.

| 경로 | 단계 |
|---|---|
| `stage0/code.py` | 0. LED 토글과 시리얼 카운터. `board_id`와 `cpu.uid`를 주기적으로 다시 찍는다 |
| `stage1/code.py` | 1. `IMU_PWR`을 올리고 IMU 버스를 스캔해 `0x6a`를 판정한다 |

두 스크립트의 출력 문자열은 ASCII다. 판정 근거가 시리얼 출력이고 Windows 시리얼
모니터의 코드 페이지에 따라 한글이 깨질 수 있다.

1단계 스크립트는 스캔 예외도 결과로 받는다. 논-Sense 보드에서 계획이 기대하는 것은
빈 목록이지만, 내부 버스에 풀업이 없으면 `busio.I2C`가 예외를 낼 수도 있다.
어느 쪽이 실제인지는 돌려봐야 안다.
