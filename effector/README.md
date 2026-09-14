# 베이스 컴프레서 펌웨어

설계는 [../devices/bass-effector-design.md](../devices/bass-effector-design.md), 단계는
[../devices/bass-effector.md](../devices/bass-effector.md)에 있다.

## 네이티브 테스트

PlatformIO 없이 CMake와 g++로 돌린다.

    cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release
    cmake --build effector/build
    ./effector/build/native_tests

## Teensy 빌드와 업로드

PlatformIO로 한다. Ubuntu에서는 `~/.platformio/penv/bin/pio`, 집 Windows에서는 설치한 `pio`를 쓴다.
업로드와 측정은 보드와 스칼렛이 있는 곳에서 한다. 명령은 `effector/`에서 실행한다.

    pio run -e teensy41
    pio run -e teensy41 -t upload
    pio device monitor -b 115200

## 시리얼 명령

`status`를 입력하면 현재 상태 한 줄이 나온다. 알 수 없는 명령을 입력하면 명령 목록이 나온다.
