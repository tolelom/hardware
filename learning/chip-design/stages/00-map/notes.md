# 0단계 지도

> TT SKY130 템플릿으로 8비트 카운터를 시뮬레이션, 합성, Actions 하드닝, GDS 보기까지 한 번 돌린 기록입니다.

- 설계 저장소: https://github.com/tolelom/tt-counter
- 기록 기준 커밋: df06773
- Actions 실행: https://github.com/tolelom/tt-counter/actions/runs/42564078571
- 3D 뷰: https://tolelom.github.io/tt-counter/

## 설계

8비트 카운터. rst_n이 0이면 클록 상승 에지에서 0, 아니면 클록 상승 에지마다 1씩 증가하고 255 다음은 0. 값은 uo_out으로 출력. uio_out, uio_oe는 0.

## 층 표

| 층 | 도구 | 입력 | 출력 | 실행 위치 |
|---|---|---|---|---|
| RTL | 직접 작성 | 동작 정의 | src/tt_um_tolelom_counter.v | 로컬 |
| 시뮬레이션 | Icarus Verilog + cocotb | RTL, test/tb.v, test/test.py | PASS/FAIL, results.xml, tb.fst | 로컬, Actions test |
| lint | Verilator `--lint-only -Wall` | RTL | 경고 목록 | 로컬 |
| 합성(범용 셀) | Yosys `synth` | RTL | 범용 셀 목록과 개수 | 로컬 |
| 합성(sky130 셀) | LibreLane 안의 Yosys | RTL, sky130 셀 라이브러리 | sky130 셀 넷리스트 | Actions gds |
| 배치·배선 | LibreLane | 넷리스트, src/config.json | 셀 위치, 배선, 타이밍 결과 | Actions gds |
| GDS | LibreLane | 배치·배선 결과 | .gds, .lef, .oas | Actions gds |
| 규칙 검사 | TT precheck | GDS, info.yaml | 검사 결과 | Actions precheck |
| 게이트 수준 테스트 | Icarus Verilog + cocotb | 배치·배선 뒤 넷리스트(.v), test/test.py | PASS/FAIL | Actions gl_test |
| 3D 뷰 | TT viewer | GDS | GitHub Pages 페이지 | Actions viewer |

## 셀 수

로컬 합성은 Yosys 0.52 범용 셀, Actions 합성은 sky130_fd_sc_hd 셀입니다.

| 구분 | 로컬 Yosys | Actions sky130 |
|---|---|---|
| 플립플롭 | `$_SDFF_PN0_` 8 | `sky130_fd_sc_hd__dfxtp_2` 8 |
| 논리 게이트 | 16 | 19 |
| 상수 셀 | 없음 | `sky130_fd_sc_hd__conb_1` 16 |
| 합계 | 24 | 43 |

- `$_SDFF_PN0_`: 동기 리셋 플립플롭. 상승 에지(P), 리셋 0일 때 동작(N), 리셋 값 0
- `dfxtp_2`: 리셋 입력이 없는 D 플립플롭. 동기 리셋은 앞단 논리 게이트로 구현
- `conb_1`: 상수 0·1을 내는 셀. uio_out, uio_oe 16비트를 0으로 묶는 데 사용
- sky130 셀 면적 합계: 404.138µm², 그중 플립플롭 170.163µm²(42.11%)

## 배치·배선 결과

| 항목 | 값 |
|---|---|
| 다이 면적 | 17954.7µm² |
| 코어 면적 | 16493.3µm² |
| 표준 셀 사용률 | 4.73% |
| 표준 셀 인스턴스 | 273(논리·플립플롭 43, 탭 셀 225, 클록 버퍼 3, 타이밍 수리 버퍼 2) |
| 채움 셀 | 4329 |
| 클록 주기 | 20ns(src/config.json CLOCK_PERIOD) |
| 셋업 최악 여유 | 14.19ns |
| 셋업·홀드 위반 | 0 |

## 타일 예산

| 항목 | 값 | 출처 |
|---|---|---|
| 타일 크기 | 약 167x108µm | 문서 기준. 템플릿 info.yaml 주석 |
| 타일 용량 | 약 1000 디지털 논리 게이트 | 문서 기준. https://tinytapeout.com/faq/ 의 TT04~TT10 절 |
| 타일당 가격 | 70€. 개발 보드(PCB) 1개 300€, 해외 일반 배송 15€는 별도 | 2026-11-09 https://app.tinytapeout.com/calculator 에서 타일 1, PCB 1로 확인. 셔틀과 참가자 구분 칸은 기본값 |

## 확인한 동작

- 조합 회로(템플릿 덧셈기)는 입력이 바뀐 시점에 출력이 바뀌고, 순차 회로(카운터)는 클록 상승 에지에서만 바뀝니다. GTKWave 파형으로 확인했습니다.
- RTL을 `+ 8'd2`로 바꾸면 테스트 4개 중 3개가 실패합니다. 리셋 직후 0 검사만 통과합니다.
- `wire _unused`에서 uio_in을 빼면 Verilator가 `%Warning-UNUSEDSIGNAL`을 내고 lint가 실패합니다.
- cocotb에서 `ClockCycles` 뒤 `ReadOnly`로 읽으면 RTL 시뮬레이션은 통과하지만 게이트 수준 테스트는 값이 한 클록씩 작게 읽혀 실패합니다. 게이트 수준 시뮬레이션은 셀마다 단위 지연(`-DUNIT_DELAY=#1`)이 있어 상승 에지 시점에는 플립플롭 출력이 아직 바뀌지 않습니다. 읽기를 `FallingEdge`로 옮긴 뒤 두 테스트 모두 통과했습니다.
