# 1단계. IMU 존재 확인. IMU_SCL / IMU_SDA 버스에서 0x6a 가 응답하면 Sense 다.
#
# 함정 둘.
#   기본 I2C(board.SDA / board.SCL, P0.04/P0.05)에는 IMU 가 없다. 거기서 빈 결과가
#   나오는 것은 논-Sense 의 증거가 아니다.
#   IMU_PWR(P1.08)을 HIGH 로 올려야 전원이 들어간다.
#
# 논-Sense 보드에서 같은 코드를 돌려 0x6a 가 없으면 두 보드의 판별이 끝난다.
# 그때 busio.I2C 가 풀업 없음으로 예외를 낼 수도 있어 예외도 결과로 받는다.

import time

import board
import busio
import digitalio
import microcontroller

LSM6DS3TR_C = 0x6A

pwr = digitalio.DigitalInOut(board.IMU_PWR)
pwr.direction = digitalio.Direction.OUTPUT
pwr.value = True
time.sleep(0.1)

uid = "".join("%02X" % b for b in microcontroller.cpu.uid)


def scan_imu_bus():
    i2c = busio.I2C(board.IMU_SCL, board.IMU_SDA)
    while not i2c.try_lock():
        pass
    addrs = i2c.scan()
    i2c.unlock()
    i2c.deinit()
    return addrs


while True:
    print("---")
    print("cpu.uid:", uid)
    try:
        addrs = scan_imu_bus()
        print("IMU bus:", [hex(a) for a in addrs])
        if LSM6DS3TR_C in addrs:
            print("verdict: 0x6a responded -> Sense")
        else:
            print("verdict: no 0x6a -> non-Sense, or IMU_PWR did not take")
    except Exception as err:
        print("IMU bus raised:", repr(err))
        print("verdict: no pull-up on the internal bus -> likely non-Sense")
    time.sleep(2)
