# 베이스 컴프레서 펌웨어

설계는 [../devices/bass-effector-design.md](../devices/bass-effector-design.md), 단계는
[../devices/bass-effector.md](../devices/bass-effector.md)에 있다.

## 구성

| 경로 | 내용 |
|---|---|
| `lib/effector_core/` | 신호 처리와 시리얼 콘솔. 플랫폼 헤더가 없어 PC에서 테스트한다 |
| `src/main.cpp` | Daisy Seed3 층. libDaisy 오디오 콜백과 USB CDC |
| `lib/libDaisy/` | libDaisy 서브모듈(고정 커밋) |
| `test/` | 네이티브 테스트 |
| `tools/static_gain.cpp` | 실기기 T9 대조값 계산 |
| `hardware/` | KiCad 회로도와 생성기, 넷리스트 |

## 회로도

`hardware/`에 KiCad 회로도(`bass-compressor.kicad_sch`, KiCad 8 형식)와 그것을 만드는 `gen_schematic.py`,
자동 생성 넷리스트 `netlist.md`가 있다. 값의 근거는 [../devices/bass-effector-schematic.md](../devices/bass-effector-schematic.md).
회로도를 손으로 고치지 않고 생성기의 PARTS 표를 고쳐 다시 만든다(실행법과 ERC 명령은 그 문서 10절).

## 네이티브 테스트

CMake와 g++로 돌린다. 저장소 루트에서 실행한다.

    cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release
    cmake --build effector/build
    ./effector/build/native_tests

## Seed3 빌드와 업로드

처음 한 번: 서브모듈을 받고 libDaisy를 빌드한다. `arm-none-eabi-gcc`, `make`, `dfu-util`이 필요하다.

    git submodule update --init --recursive --depth 1
    make -C effector/lib/libDaisy -j

펌웨어 빌드와 업로드. 명령은 `effector/`에서 실행한다.

    make
    make program-dfu

`program-dfu`는 Seed3가 DFU 모드일 때만 된다. 동작 중이면 콘솔에서 `dfu`를 입력하고, 응답이 없으면 BOOT를 누른 채
RESET을 눌렀다 뗀다. 펌웨어는 내부 플래시 128KB에 들어가며 현재 약 107KB다. 넘어서면 libDaisy 부트로더로
QSPI에서 실행하는 방식(`APP_TYPE = BOOT_QSPI`)으로 바꾼다.

## 시리얼 콘솔

USB CDC(115200 무관, 가상 시리얼)로 붙는다. Linux에서는 `/dev/ttyACM0`이고 `screen /dev/ttyACM0`로 연다.
`status`를 입력하면 현재 상태 한 줄이 나온다. 알 수 없는 명령을 입력하면 명령 목록이 나온다. `dfu`는
부트로더로 재시작한다.

## 1단계 동작

- 오디오는 48kHz, 블록 48샘플(1ms). 입력 1채널을 처리해 출력 두 채널에 같이 내보낸다.
- 램프는 이펙트 켬·끔(20ms)에만 있다. 시리얼 명령으로 바꾼 입력 드라이브, 블렌드, 출력, 트림, 컴프레서
  설정은 다음 오디오 블록에서 바로 바뀐다. 설계서 6.3절의 파라미터 램프는 구현되지 않았다.
- 컴프레서는 이펙트가 꺼져 있어도 돈다. `status`의 `gainDb`와 `meter`는 이펙트 꺼짐 상태에서도 감쇠량을 보인다.
- `Compressor`는 `ratio` 하한을 검사하지 않는다. 1 미만이면 압축 곡선이 성립하지 않고 0이면 게인이 발산한다.
  콘솔의 `ratio`는 `ratioFromKnob`을 거쳐 1~20이 된다.
- 파라미터 적용과 테스트 톤 설정은 인터럽트를 잠깐 막고 복사한다. 콘솔 입력은 USB 인터럽트가 링 버퍼에 넣고
  메인 루프가 꺼내 해석한다.
