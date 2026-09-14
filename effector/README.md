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

## 1단계 동작

- 램프는 이펙트 켬·끔(20ms)에만 있다. 시리얼 명령으로 바꾼 입력 드라이브, 블렌드, 출력, 트림, 컴프레서
  설정은 다음 오디오 블록에서 바로 바뀐다. 설계서 6.3절의 파라미터 램프는 구현되지 않았다.
- 컴프레서는 이펙트가 꺼져 있어도 돈다. `status`의 `gainDb`와 `meter`는 이펙트 꺼짐 상태에서도 감쇠량을 보인다.
- `Compressor`는 `ratio` 하한을 검사하지 않는다. 1 미만이면 압축 곡선이 성립하지 않고 0이면 게인이 발산한다.
  콘솔의 `ratio`는 `ratioFromKnob`을 거쳐 1~20이 된다.
