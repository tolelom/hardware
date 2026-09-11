# 0단계. 직접 올린 코드로 LED를 깜빡이고 시리얼에 출력을 낸다.
# 공장 펌웨어도 RGB LED를 순환시키므로 판정은 시리얼 출력으로 한다.
# 공장 펌웨어는 시리얼로 아무것도 보내지 않는다.
#
# 출력 문자열은 ASCII로 둔다. Windows 시리얼 모니터의 코드 페이지에 따라
# 한글이 깨질 수 있고, 판정 근거가 되는 출력이라 깨지면 안 된다.

import time

import board
import digitalio
import microcontroller

# LED_RED 는 P0.26. 액티브 로우인지는 확인 안 함.
# 토글이라 극성이 반대여도 깜빡이는 것은 같다.
led = digitalio.DigitalInOut(board.LED_RED)
led.direction = digitalio.Direction.OUTPUT

uid = "".join("%02X" % b for b in microcontroller.cpu.uid)
board_id = getattr(board, "board_id", "unknown")

count = 0
while True:
    # 헤더를 주기적으로 다시 찍는다. 부팅 뒤에 터미널을 붙이면 첫 출력을 놓친다.
    if count % 20 == 0:
        print("---")
        print("board_id:", board_id)
        print("cpu.uid:", uid)

    led.value = not led.value
    print("tick", count)
    count += 1
    time.sleep(0.5)
