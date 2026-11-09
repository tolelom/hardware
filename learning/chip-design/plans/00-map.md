# 0단계 지도 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** TT SKY130 템플릿으로 8비트 카운터를 시뮬레이션, 합성, Actions 하드닝, GDS 보기까지 한 번 돌리고, 흐름의 층마다 입력과 출력을 표로 정리합니다.

**Architecture:** 트랙 문서와 기록은 hardware 저장소 `learning/chip-design/`에 둡니다. TT 설계는 TinyTapeout/ttsky-verilog-template으로 만든 별도 GitHub 저장소 tolelom/tt-counter에 두고, 기준 머신에서 `personal/tt-counter/`로 둡니다. 하드닝은 그 저장소의 GitHub Actions(gds.yaml)가 합니다.

**Tech Stack:** Verilog, cocotb 2.1.0, pytest 8.4.2, Icarus Verilog, Verilator, Yosys, GTKWave, uv(Python 3.11 venv), TT tt-gds-action@ttsky26d.

**Spec:** `learning/chip-design/design.md`

## Global Constraints

- 기준 머신: Ubuntu 데스크톱(Ubuntu 26.04.1 LTS). notes.md의 lint 결과와 셀 수는 이 머신에서 낸 값
- 검증 기준: cocotb 테스트 통과, `verilator --lint-only -Wall` 경고 0, Yosys 합성에서 래치 없음, 셀 수 기록
- 테스트를 먼저 쓰고 RTL을 씀
- notes.md에는 확인한 것만. 해보지 않은 것은 "확인 필요", 모르는 것은 "모름", 문서만 읽은 값은 "문서 기준"
- 파형 파일(VCD, FST)과 빌드 산출물은 커밋하지 않음
- 0단계는 셔틀에 제출하지 않음
- 문서 문체: 합쇼체, em dash 금지
- 커밋: 새 파일은 `git add -- <파일>` 뒤 같은 pathspec으로 커밋, 커밋 뒤 `git show --stat HEAD` 확인, 작성자 `98kimsungmin@naver.com`, 메시지 `[Chip] 한국어 요약`
- 명령은 절대 경로로 씀. 아래에서 `TT=/home/soungmin-kim/Desktop/develop/personal/tt-counter`, `CD=/home/soungmin-kim/Desktop/develop/personal/hardware/learning/chip-design`
- 외부 작업(GitHub 저장소 생성, Pages 설정)은 실행 전에 사용자 확인

## 진행 방식

태스크마다 시작할 때 "이 실습이 답할 질문"을 사용자와 먼저 맞추고, 끝나면 실행 결과(테스트 출력, 파형, 셀 수)를 보여 준 뒤 멈춥니다. 다음 태스크는 사용자가 "다음"이라고 할 때 시작합니다.

## Review Focus

- 클록 에지 직후 읽기: `await ClockCycles(...)` 직후에는 그 에지의 레지스터 갱신이 아직 안 보임. 읽기 전에 `await ReadOnly()`(Task 2)
- 카운트 도중 리셋: 0으로 돌아가고 다시 1부터 세야 함(Task 2 `test_reset_mid_count`)
- 255 다음 값: 0으로 넘어가야 함(Task 2 `test_wraps_at_256`)
- Verilator -Wall 스타일 경고: 파일명과 모듈명이 다르면 DECLFILENAME, 쓰지 않는 입력은 UNUSEDSIGNAL(Task 2 파일명 변경, Task 3 `_unused`)
- gh 계정: 이 머신의 gh는 soungminkim-sg로 로그인되어 있음. tolelom 계정으로 전환한 뒤 저장소를 만들어야 함(Task 4)

---

### Task 1: 도구 설치와 트랙 골격

이 실습이 답할 질문: 이 머신에서 Verilog를 시뮬레이션, lint, 합성하는 도구가 각각 무엇이고 어느 버전인가.

**Files:**
- Create: `learning/chip-design/README.md`
- Create: `learning/chip-design/requirements.txt`
- Create: `learning/chip-design/.gitignore`
- Modify: `README.md` 폴더 표와 학습 트랙 표

**Interfaces:**
- Produces: `$CD/.venv`(cocotb 2.1.0, pytest 8.4.2). 이후 태스크는 `source $CD/.venv/bin/activate` 뒤 `make`를 실행

- [ ] **Step 1: apt 패키지 설치(사용자 sudo 필요)**

Run: `sudo apt install iverilog verilator yosys gtkwave`
Expected: 설치 완료. 확인한 후보 버전은 iverilog 12.0-3, verilator 5.032-1, yosys 0.52-2, gtkwave 3.3.126-1

- [ ] **Step 2: 버전 확인**

Run: `iverilog -V 2>&1 | head -1; verilator --version; yosys -V; gtkwave --version | head -1`
Expected: 네 줄 모두 버전 출력

- [ ] **Step 3: requirements.txt 작성과 venv 생성**

`requirements.txt` 내용은 템플릿 test/requirements.txt와 같게 `pytest==8.4.2`, `cocotb==2.1.0` 두 줄.

Run: `uv venv --python 3.11 $CD/.venv && uv pip install -p $CD/.venv -r $CD/requirements.txt && $CD/.venv/bin/python -c "import cocotb; print(cocotb.__version__)" && ls "$($CD/.venv/bin/cocotb-config --libpython)"`
Expected: `2.1.0`과 libpython .so 경로. .so가 없으면 uv가 받은 Python 빌드에 공유 라이브러리가 없다는 뜻이므로 멈추고 사용자와 Python 출처를 정함

- [ ] **Step 4: .gitignore 작성**

`.venv/`, `*.vcd`, `*.fst`, `sim_build/`, `results.xml`, `__pycache__/`

- [ ] **Step 5: README.md 작성과 저장소 README 표에 행 추가**

트랙 README: 트랙 소개 한 문단, 설계서 링크(단계표는 설계서 2절 링크로 대신함), 도구 표(이름, 설치 명령, Step 2에서 확인한 버전), venv 만드는 명령.
저장소 README 학습 트랙 표: `| 칩 설계 | Tiny Tapeout(SKY130) | [chip-design](learning/chip-design/) |`. 폴더 표의 `learning/` 설명에 `chip-design/`도 추가.

- [ ] **Step 6: Commit**

```bash
cd /home/soungmin-kim/Desktop/develop/personal/hardware
git add -- README.md learning/chip-design/README.md learning/chip-design/requirements.txt learning/chip-design/.gitignore
git -c user.email=98kimsungmin@naver.com commit -m "[Chip] 0단계: 도구 설치 기록과 트랙 골격" -- README.md learning/chip-design/README.md learning/chip-design/requirements.txt learning/chip-design/.gitignore
git show --stat HEAD
```

### Task 2: 템플릿 받기와 카운터 테스트 먼저 쓰기

이 실습이 답할 질문: TT 템플릿에서 테스트는 어떤 순서로 돌고(Makefile → iverilog → tb.v → cocotb test.py), 테스트가 RTL을 어떻게 건드리는가. 클록 에지 직후와 ReadOnly 뒤에 읽은 값이 왜 다른가.

이 태스크는 저장소를 GitHub에 만들기 전에 로컬에서만 진행합니다.

**Files:**
- Create: `$TT/`(템플릿 내용 복사, `git init`)
- Modify: develop `.gitignore`
- Rename: `$TT/src/project.v` → `$TT/src/tt_um_tolelom_counter.v`
- Modify: `$TT/test/test.py`, `$TT/test/tb.v`, `$TT/test/Makefile`(PROJECT_SOURCES), `$TT/info.yaml`, `$TT/docs/info.md`

**Interfaces:**
- Produces: 모듈 `tt_um_tolelom_counter`, 파일 `src/tt_um_tolelom_counter.v`. 포트는 템플릿 그대로(`ui_in[7:0]` in, `uo_out[7:0]` out, `uio_in[7:0]` in, `uio_out[7:0]` out, `uio_oe[7:0]` out, `ena`, `clk`, `rst_n`). 동작: rst_n=0이면 클록 에지에서 uo_out=0, 아니면 clk 상승 에지마다 uo_out+1(8비트에서 넘침). uio_out=0, uio_oe=0

- [ ] **Step 1: 템플릿 받기와 develop에서 제외**

Run: `git clone --depth 1 https://github.com/TinyTapeout/ttsky-verilog-template.git $TT && rm -rf $TT/.git && git -C $TT init -b main`
Expected: src/, test/, docs/, info.yaml, .github/workflows/{gds,docs,test,fpga}.yaml 존재

develop `.gitignore`에 `/personal/tt-counter/`를 더하고 커밋합니다.

```bash
cd /home/soungmin-kim/Desktop/develop
git -c user.email=98kimsungmin@naver.com commit -m "[Chore] personal에 tt-counter 클론 자리 추가" -- .gitignore
git show --stat HEAD
```

- [ ] **Step 2: 템플릿 테스트를 그대로 한 번 돌림**

Run: `source $CD/.venv/bin/activate && cd $TT/test && make -B`
Expected: `test_project` PASS(템플릿 덧셈기, 20+30=50). 파형 tb.fst 생성. `gtkwave $TT/test/tb.fst $TT/test/tb.gtkw`로 열어 사용자와 같이 봄

- [ ] **Step 3: 이름과 메타데이터 바꾸기**

- `git -C $TT mv src/project.v src/tt_um_tolelom_counter.v`(Verilator DECLFILENAME 경고 방지)
- 모듈 이름 `tt_um_example` → `tt_um_tolelom_counter`: 파일 안 module 선언, `test/tb.v` 인스턴스, `info.yaml` top_module
- `info.yaml` source_files와 `test/Makefile` PROJECT_SOURCES를 `tt_um_tolelom_counter.v`로
- `info.yaml`: title `8-bit counter`, author `tolelom`, description `8-bit counter on the dedicated outputs`, language `Verilog`, clock_hz `50000000`(src/config.json CLOCK_PERIOD 20ns와 같음), tiles `1x1`, pinout uo[0]~uo[7]에 `count[0]`~`count[7]`. description이 비면 gds 작업이 실패함
- `docs/info.md`: How it works에 "rst_n이 낮으면 0, 아니면 클록마다 1씩 증가해 uo_out에 출력", How to test에 "rst_n을 낮췄다 올린 뒤 uo_out이 클록마다 증가하는지 확인"을 영어로. 템플릿 문장("Explain how your project works" 등)이 남으면 docs 작업이 실패함

- [ ] **Step 4: 실패하는 테스트 작성(test/test.py)**

템플릿의 `test_project`를 지우고 아래 네 개로 바꿈. 임포트는 `from cocotb.triggers import ClockCycles, ReadOnly`. 공통 준비 함수 `async def reset(dut)`: 클록 10us 시작, ena=1, ui_in=0, uio_in=0, rst_n=0, `ClockCycles(dut.clk, 10)`, rst_n=1.

규칙: 쓰기는 `ClockCycles` 직후에, 읽기는 `await ReadOnly()` 뒤에 합니다. ReadOnly 뒤에는 다음 `ClockCycles`를 기다리기 전까지 쓰지 않습니다.

```python
@cocotb.test()
async def test_reset_clears(dut):
    # reset 뒤 ReadOnly
    assert dut.uo_out.value == 0
    assert dut.uio_oe.value == 0
    assert dut.uio_out.value == 0

@cocotb.test()
async def test_counts_up(dut):
    # reset 뒤 ClockCycles 5, ReadOnly
    assert dut.uo_out.value == 5

@cocotb.test()
async def test_wraps_at_256(dut):
    # reset 뒤 ClockCycles 255, ReadOnly에서 255
    assert dut.uo_out.value == 255
    # ClockCycles 1, ReadOnly에서 0
    assert dut.uo_out.value == 0

@cocotb.test()
async def test_reset_mid_count(dut):
    # reset 뒤 ClockCycles 7, rst_n=0, ClockCycles 2, rst_n=1, ReadOnly에서 0
    assert dut.uo_out.value == 0
    # ClockCycles 3, ReadOnly에서 3
    assert dut.uo_out.value == 3
```

관찰 실습: `test_counts_up`에서 ReadOnly를 잠깐 빼고 돌려 4가 읽히는 것을 확인한 뒤 되돌립니다.

- [ ] **Step 5: 테스트가 실패하는지 확인**

Run: `source $CD/.venv/bin/activate && cd $TT/test && make -B`
Expected: `test_reset_clears` PASS, 나머지 3개 FAIL(템플릿 덧셈기는 0+0=0), make 종료 코드 0이 아님

이 태스크는 실패한 상태로 끝냅니다. 사용자와 실패 메시지를 같이 읽고 멈춥니다.

### Task 3: 카운터 RTL과 로컬 검증

이 실습이 답할 질문: 플립플롭 하나가 Verilog로 어떻게 쓰이고, Yosys가 그것을 어떤 셀로 바꾸는가.

**Files:**
- Modify: `$TT/src/tt_um_tolelom_counter.v`
- Create: `$TT/Makefile`

**Interfaces:**
- Consumes: Task 2의 모듈 이름, 파일 이름, 동작 정의, 테스트 네 개
- Produces: 루트 Makefile 타깃
  - `test`: `cd test && $(MAKE) -B`(`make -C`는 test/Makefile이 쓰는 `$(PWD)`를 바꾸지 않아 소스를 못 찾음)
  - `lint`: `verilator --lint-only -Wall src/tt_um_tolelom_counter.v`
  - `synth`: `yosys -p "read_verilog src/tt_um_tolelom_counter.v; synth -top tt_um_tolelom_counter; check -assert; select -assert-none t:\$$_DLATCH*; stat"`. `check -assert`는 다중 드라이버, 구동 안 된 선, 조합 루프를 잡고, 래치는 `select -assert-none`이 잡음

- [ ] **Step 1: 카운터 구현**

`always @(posedge clk)` 안에서 `!rst_n`이면 8비트 레지스터를 0, 아니면 `+ 8'd1`. `uo_out`에 레지스터 연결. 쓰지 않는 입력은 `wire _unused = &{ena, ui_in, uio_in, 1'b0};`로 바꿈(템플릿 묶음에는 ui_in, uio_in이 없음).

- [ ] **Step 2: 테스트 통과 확인**

Run: `source $CD/.venv/bin/activate && make -C $TT test`
Expected: 4개 PASS. gtkwave로 리셋 해제와 255 → 0 구간을 사용자와 같이 봄

- [ ] **Step 3: lint**

Run: `make -C $TT lint`
Expected: 경고 0, 종료 코드 0

- [ ] **Step 4: 합성**

Run: `make -C $TT synth`
Expected: 종료 코드 0. `stat`에 `$_SDFF_PN0_` 8개와 게이트 셀(`$_XOR_`, `$_AND_` 등). 셀 수를 Task 5 notes.md에 옮김

- [ ] **Step 5: 일부러 래치 만들기**

`reg [7:0] latch_tmp;`와 else 없는 `always @(*) if (ui_in[0]) latch_tmp = ui_in;`를 임시로 넣고 `uio_out`을 `latch_tmp`에 임시 연결(연결하지 않으면 합성이 지워서 래치가 안 보임). `make -C $TT synth`가 `select -assert-none`에서 실패하는 것을 사용자와 확인한 뒤 되돌림. 되돌린 뒤 Step 2~4 다시 통과

- [ ] **Step 6: 첫 커밋**

새 저장소의 첫 커밋이라 pathspec 대신 전체를 추가합니다. 템플릿 .gitignore에 파형과 빌드 산출물이 이미 들어 있으므로 `git -C $TT status --short`에 `.fst`, `sim_build`, `results.xml`이 없는지 먼저 확인합니다.

```bash
git -C $TT add -A
git -C $TT -c user.email=98kimsungmin@naver.com commit -m "[Chip] 8비트 카운터와 테스트"
git -C $TT show --stat HEAD
```

### Task 4: GitHub 저장소와 Actions 하드닝

이 실습이 답할 질문: Actions가 RTL을 받아 GDS를 내기까지 어떤 작업(gds, precheck, gl_test, viewer)을 거치고, 각 작업의 결과물은 무엇인가.

외부 작업이라 Step 2 전에 사용자 확인을 받습니다.

**Files:**
- 없음(원격 설정과 push)

**Interfaces:**
- Consumes: Task 3 커밋
- Produces: github.com/tolelom/tt-counter(public), gds·test·docs 워크플로 실행 결과

- [ ] **Step 1: gh 계정 전환**

Run: `gh auth status`
Expected: tolelom 계정 확인. 없으면 사용자가 `! gh auth login`으로 추가하고 `gh auth switch -u tolelom`

- [ ] **Step 2: 저장소 생성과 push(사용자 확인 후)**

Run: `cd $TT && gh repo create tolelom/tt-counter --public --source . --push`
Expected: main 브랜치 push, Actions 탭에 gds·test·docs 실행 시작

- [ ] **Step 3: Pages 소스 설정(사용자 확인 후)**

viewer 작업이 GitHub Pages에 배포하므로 Pages 소스를 "GitHub Actions"로 둠.
Run: `gh api -X POST repos/tolelom/tt-counter/pages -f build_type=workflow`
Expected: 201. 이미 있으면 `gh api -X PUT repos/tolelom/tt-counter/pages -f build_type=workflow`

- [ ] **Step 4: 워크플로 결과 확인**

Run: `gh run list -R tolelom/tt-counter --limit 5` 그리고 실패 시 `gh run view <id> -R tolelom/tt-counter --log-failed`
Expected: gds 워크플로의 gds, precheck, gl_test, viewer 모두 성공. test, docs 워크플로 성공. Pages 설정 전에 돈 첫 실행의 viewer가 실패했으면 `gh run rerun <id> -R tolelom/tt-counter --failed`

- [ ] **Step 5: GDS 보기**

viewer가 배포한 Pages 주소(`gh api repos/tolelom/tt-counter/pages -q .html_url`)를 브라우저로 열어 3D 뷰를 사용자와 같이 봄. gds 작업 요약에 나오는 셀 사용량과 셀 종류(sky130 표준 셀 이름)를 Task 5 표에 옮김

### Task 5: 층 표와 타일 예산 기록

이 실습이 답할 질문: 흐름의 층마다 무엇이 들어가고 무엇이 나오며, 로컬 Yosys 셀 수와 Actions의 sky130 셀 수는 왜 다른가.

**Files:**
- Create: `learning/chip-design/stages/00-map/notes.md`

**Interfaces:**
- Consumes: Task 1 버전, Task 3 로컬 셀 수, Task 4 Actions 요약

- [ ] **Step 1: notes.md 작성**

들어갈 내용:
- TT 저장소 링크(https://github.com/tolelom/tt-counter)와 커밋 해시
- 층 표: 층(RTL, 시뮬레이션, 합성, 배치·배선, GDS) | 도구 | 입력 | 출력 | 어디서 돌았나(로컬/Actions)
- 셀 수 비교: 로컬 Yosys 범용 셀(`$_SDFF_PN0_` 등) 수와 Actions sky130 셀 수
- 타일 예산: 타일 크기(템플릿 info.yaml 기준 약 167x108um), 용량(FAQ 기준 약 1000 게이트, TT04~TT10 절의 문장), 타일당 가격(https://app.tinytapeout.com/calculator 를 브라우저로 열어 확인한 값). 모두 "문서 기준"으로 표기하고 URL을 붙임
- 확인 못 한 것은 "확인 필요"로 남김

- [ ] **Step 2: 문체 검사**

Run: `/home/soungmin-kim/Desktop/develop/tools/doc-lint.sh $CD/stages/00-map/notes.md`
Expected: `위반 없음`

- [ ] **Step 3: Commit**

```bash
cd /home/soungmin-kim/Desktop/develop/personal/hardware
git add -- learning/chip-design/stages/00-map/notes.md
git -c user.email=98kimsungmin@naver.com commit -m "[Chip] 0단계: 층 표와 타일 예산 기록" -- learning/chip-design/stages/00-map/notes.md
git show --stat HEAD
```
