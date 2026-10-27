# 베이스 컴프레서 펌웨어

설계는 [../devices/bass-effector-design.md](../devices/bass-effector-design.md), 단계는
[../devices/bass-effector.md](../devices/bass-effector.md)에 있다.

## 구성

| 경로 | 내용 |
|---|---|
| `lib/effector_core/` | 신호 처리와 시리얼 콘솔. 플랫폼 헤더가 없어 PC에서 테스트한다 |
| `src/main.cpp` | Daisy Seed3 층. libDaisy 오디오 콜백과 USB CDC |
| `lib/libDaisy/` | libDaisy 서브모듈. 고정 커밋 265aae3e(v8.1.0에서 19커밋 뒤), 하위 서브모듈 6개 포함 약 309MB |
| `test/` | 네이티브 테스트 |
| `tools/static_gain.cpp` | 실기기 T9 대조값 계산 |
| `tools/offline_render.cpp`, `wav_file`, `render_pipeline` | DI 녹음을 코어에 통과시키는 오프라인 청음 도구 |
| `hardware/` | KiCad 회로도와 생성기, 넷리스트 |
| `hardware/spice/` | ngspice 시뮬레이션. 결과는 회로 설계 문서 12절 |
| `hardware/gen_spice.py`, `hardware/ngspice_run.py` | BLOCKS에서 SPICE 넷리스트 생성(`hardware/spice/generated/`), ngspice 실행과 결과 읽기 |
| `hardware/spice/noise_budget.py`, `hardware/spice/monte_carlo.py` | 잡음 예산, 공차 몬테카를로 |
| `hardware/enclosure.py`, `hardware/enclosure/` | 케이스 기구 기준과 간섭 검사, 타공 도면·기판 영역 SVG |
| `hardware/tests/` | 설계 검증 도구 테스트 |

## 회로도

`hardware/`에 KiCad 회로도(`bass-compressor.kicad_sch`, KiCad 8 형식)와 그것을 만드는 `gen_schematic.py`,
자동 생성 넷리스트 `netlist.md`가 있다. 값의 근거는 [../devices/bass-effector-schematic.md](../devices/bass-effector-schematic.md).
회로도를 손으로 고치지 않고 생성기의 PARTS 표를 고쳐 다시 만든다(실행법과 ERC 명령은 그 문서 10절).

## 설계 검증 도구

설계는 [../devices/bass-effector-tools-design.md](../devices/bass-effector-tools-design.md). ngspice가 필요하다(`brew install ngspice`).
저장소 루트에서 실행한다.

    python3 effector/hardware/gen_spice.py
    python3 -m unittest discover -s effector/hardware/tests -t effector/hardware
    python3 effector/hardware/spice/noise_budget.py
    python3 effector/hardware/spice/monte_carlo.py --count 200 --seed 1
    python3 effector/hardware/spice/monte_carlo.py --count 200 --seed 1 --conservative
    python3 effector/hardware/enclosure.py --draft

- BLOCKS를 고치면 `gen_spice.py`를 다시 돌린다. 생성물 해시가 다르면 테스트가 실패한다.
- 몬테카를로 200벌은 수 분 걸린다. `--conservative`는 LT1054 동작 모델 손실을 보수 값으로 고정한다.
- `enclosure.py --final`은 미측정 값이 하나라도 있으면 도면을 내지 않고 종료 코드 2로 끝난다.

## 네이티브 테스트

CMake와 g++로 돌린다. 저장소 루트에서 실행한다.

    cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release
    cmake --build effector/build
    ./effector/build/native_tests

## 오프라인 청음

스칼렛 INST, 게인 최소로 녹음한 48kHz WAV(16/24/32비트 PCM 또는 32비트 float, 첫 채널만 씀)를 코어에
통과시킨다. 네이티브 테스트와 같은 빌드로 `effector/build/offline_render`가 생긴다.

    ./effector/build/offline_render di.wav out.wav --dynamic-range 100 --seed 1 \
        --command "input 0.5" --command "ratio 0.6"

- 입력은 -4.7dB로 페달 ADC 레벨(풀스케일 +17.2dBu)에 맞춘 뒤 처리하고, 출력은 +4.7dB로 되돌린다.
  `--command "effect 0"`만 주면 원본과 같은 레벨이다.
- `--dynamic-range`는 T7과 같은 정의(125~8000Hz, 풀스케일 사인 RMS 기준)의 모듈 다이내믹 레인지다.
  백색 가우스 잡음을 프로세서 앞에 더한다. 생략하면 노이즈를 넣지 않는다.
- `--command`는 시리얼 콘솔 명령 그대로다. 파라미터를 바꾸지 않은 명령이 있으면 오류로 끝난다.
- 실행하면 `recordingFloorDynamicRangeDb`를 출력한다. 녹음에서 가장 조용한 0.5초 창을 같은 정의로 환산한
  값이고, 이보다 큰 `--dynamic-range`는 녹음 바닥에 묻혀 구분되지 않는다.
- 변형끼리 비교할 때 파일별로 노멀라이즈하지 않는다.

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
