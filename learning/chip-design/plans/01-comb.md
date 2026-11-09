# 1단계 조합 회로 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 반가산기에서 32비트 RV32I ALU까지 조합 회로를 하나씩 테스트 먼저 쓰고 만들어, 셀 수를 기록합니다.

**Architecture:** 모든 파일은 hardware 저장소 `learning/chip-design/stages/01-comb/`에 둡니다. RTL은 `rtl/<모듈>.v`(파일 하나에 모듈 하나), 테스트는 `test/test_<모듈>.py`입니다. 단계 Makefile 하나가 `make test|lint|synth MOD=<모듈>`로 모든 모듈을 다룹니다. TT 템플릿은 쓰지 않고 cocotb가 모듈을 직접 구동합니다.

**Tech Stack:** Verilog, cocotb 2.1.0, Icarus Verilog 12.0, Verilator 5.032, Yosys 0.52, GTKWave.

**Spec:** `learning/chip-design/design.md`

## Global Constraints

- 검증 기준: cocotb 테스트 통과, `verilator --lint-only -Wall` 경고 0, Yosys 합성에서 래치 없음, 셀 수 기록
- 테스트를 먼저 쓰고 RTL을 씀. 테스트가 RTL 없이 실패하는 것을 확인한 뒤 RTL을 씀
- RTL과 테스트는 사용자가 씀. Claude는 포트, 검사할 경우, 쓸 문법을 먼저 보이고 작성 뒤 함께 읽음
- 실습마다 시작할 때 이 실습이 답할 질문과 흐름에서의 위치를 맞추고, 끝나면 결과를 보인 뒤 멈춤
- 모든 RTL 파일 첫 줄 `` `default_nettype none ``
- notes.md에는 확인한 것만. 셀 수는 기준 머신(Ubuntu, Yosys 0.52 `synth -flatten`) 값
- 파형·빌드 산출물은 커밋하지 않음(트랙 .gitignore가 `sim_build/`, `*.fst`, `results.xml`, `__pycache__/`를 제외)
- 커밋: 새 파일은 `git add -- <파일>` 뒤 같은 pathspec으로 커밋, `git show --stat HEAD` 확인, 작성자 `98kimsungmin@naver.com`, 메시지 `[Chip] 1단계: ...`
- 명령의 `S=/home/soungmin-kim/Desktop/develop/personal/hardware/learning/chip-design/stages/01-comb`, venv는 `source $S/../../.venv/bin/activate`

## Review Focus

- 뺄셈 넘침: 0 - 1은 0xFFFFFFFF로 감겨야 함(Task 7 `test_sub_wraps`)
- SLT와 SLTU: 0xFFFFFFFF < 1은 부호 있는 비교에서 참, 부호 없는 비교에서 거짓(Task 7 `test_slt_signed_vs_unsigned`)
- SRA 부호 채움: 0x80000000 >>> 4는 0xF8000000(Task 7 `test_sra_fills_sign`)
- 시프트 양: b의 하위 5비트만 씀. b = 33이면 1만큼 시프트(Task 7 `test_shift_uses_low_5_bits`)
- 정의 안 된 op: 출력 0이고 래치가 생기지 않아야 함(Task 7 `test_unknown_op_is_zero`와 `make synth`)

---

### Task 1: 단계 골격

이 실습이 답할 질문: 모듈이 여러 개일 때 테스트·lint·합성을 어떻게 한 명령으로 돌리나.

Claude가 만들고 사용자는 명령 사용법만 확인합니다.

**Files:**
- Create: `stages/01-comb/Makefile`

**Interfaces:**
- Produces: `make test MOD=<m>`(cocotb, `test/test_<m>.py`, 파형 `sim_build/<m>.fst`), `make lint MOD=<m>`(`rtl/<m>.v`를 최상위로, `-Irtl`로 하위 모듈 탐색), `make synth MOD=<m>`(`rtl/*.v` 전부 읽고 `synth -flatten -top <m>`, `check -assert`, 래치 있으면 실패, `stat` 출력). MOD를 빼면 오류로 멈춤

- [ ] **Step 1: Makefile 작성**

cocotb `Makefile.sim`을 include하고 `SIM=icarus`, `WAVES=1`, `COCOTB_TOPLEVEL=$(MOD)`, `COCOTB_TEST_MODULES=test_$(MOD)`, `PYTHONPATH`에 `test/`. `test` 타깃은 내부 `sim` 타깃을 `-B`로 부름. 스크래치 폴더에서 full_adder로 시험해 동작을 확인한 형태.

- [ ] **Step 2: MOD 없이 실행**

Run: `make -C $S test`
Expected: `MOD를 지정` 오류로 멈춤

- [ ] **Step 3: Commit**

```bash
cd /home/soungmin-kim/Desktop/develop/personal/hardware
git add -- learning/chip-design/stages/01-comb/Makefile
git -c user.email=98kimsungmin@naver.com commit -m "[Chip] 1단계: 단계 Makefile" -- learning/chip-design/stages/01-comb/Makefile
git show --stat HEAD
```

### Task 2: 반가산기와 진리표 전수 테스트

이 실습이 답할 질문: 입력이 몇 개 안 되는 회로는 어떻게 빠짐없이 검사하나. 게이트 두 개가 1비트 덧셈이 되는 이유는 무엇인가.

**Files:**
- Create: `stages/01-comb/test/test_half_adder.py`
- Create: `stages/01-comb/rtl/half_adder.v`

**Interfaces:**
- Produces: `module half_adder(input wire a, input wire b, output wire sum, output wire carry)`. sum = a XOR b, carry = a AND b

- [ ] **Step 1: 테스트 작성**

`test_truth_table`: `itertools.product((0, 1), repeat=2)`로 네 경우를 돌며 입력을 넣고 `await Timer(1, unit="ns")` 뒤 `sum == (a + b) % 2`, `carry == (a + b) // 2`.

새 문법: 클록이 없는 회로는 `Timer`로 시간을 조금 흘린 뒤 읽음.

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=half_adder`
Expected: iverilog가 `half_adder`를 못 찾아 실패

- [ ] **Step 3: RTL 작성**

`assign` 두 줄.

- [ ] **Step 4: 검증**

Run: `make -C $S test MOD=half_adder && make -C $S lint MOD=half_adder && make -C $S synth MOD=half_adder`
Expected: PASS, lint 경고 0, `$_XOR_` 1과 `$_AND_` 1

- [ ] **Step 5: Commit**

`[Chip] 1단계: 반가산기`, 파일 두 개 pathspec.

### Task 3: 전가산기와 모듈 계층

이 실습이 답할 질문: 만들어 둔 모듈을 부품처럼 끼워 더 큰 모듈을 어떻게 만드나.

**Files:**
- Create: `stages/01-comb/test/test_full_adder.py`
- Create: `stages/01-comb/rtl/full_adder.v`

**Interfaces:**
- Consumes: Task 2 `half_adder`
- Produces: `module full_adder(input wire a, input wire b, input wire cin, output wire sum, output wire cout)`. 반가산기 두 개와 OR 하나로 구성

- [ ] **Step 1: 테스트 작성**

`test_truth_table`: 여덟 경우 전수. `sum == (a + b + cin) % 2`, `cout == (a + b + cin) // 2`.

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=full_adder`
Expected: 실패

- [ ] **Step 3: RTL 작성**

새 문법: 인스턴스 `half_adder h0 (.a(a), .b(b), .sum(s1), .carry(c1));`, 내부 선 `wire s1, c1, c2;`.

- [ ] **Step 4: 검증**

Run: `make -C $S test MOD=full_adder && make -C $S lint MOD=full_adder && make -C $S synth MOD=full_adder`
Expected: PASS, lint 경고 0, 셀 5개(`$_XOR_` 2, `$_AND_` 2, `$_OR_` 1)

- [ ] **Step 5: Commit**

`[Chip] 1단계: 전가산기`, 파일 두 개 pathspec.

### Task 4: 8비트 가산기 두 벌 비교

이 실습이 답할 질문: 같은 동작을 부품 연결로 쓴 것과 `+`로 쓴 것은 합성 결과가 같은가. 입력이 너무 많아 전수 검사가 어려울 때는 어떻게 검사하나.

**Files:**
- Create: `stages/01-comb/test/adder8_cases.py`
- Create: `stages/01-comb/test/test_adder8_ripple.py`, `stages/01-comb/test/test_adder8_behav.py`
- Create: `stages/01-comb/rtl/adder8_ripple.v`, `stages/01-comb/rtl/adder8_behav.v`

**Interfaces:**
- Consumes: Task 3 `full_adder`
- Produces: `module adder8_ripple(input wire [7:0] a, input wire [7:0] b, input wire cin, output wire [7:0] sum, output wire cout)`, 같은 포트의 `adder8_behav`. `adder8_cases.py`에 `async def check_adder8(dut)`: 두 테스트가 같은 검사를 공유

- [ ] **Step 1: 공용 검사와 테스트 두 개 작성**

`check_adder8`: 경계 경우(0+0, 255+1, 255+255+1, 128+128) 뒤 `random.seed(0)`으로 무작위 1000 경우. 기대값은 Python으로 `total = a + b + cin`, `sum == total & 0xFF`, `cout == total >> 8`. 각 test 파일은 `check_adder8(dut)`만 부름.

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=adder8_ripple; make -C $S test MOD=adder8_behav`
Expected: 둘 다 실패

- [ ] **Step 3: RTL 두 벌 작성**

ripple: `full_adder` 여덟 개를 자리올림 선 `wire [8:0] c`로 이음. 새 문법: `genvar`와 `generate for`.
behav: `assign {cout, sum} = a + b + cin;`. 새 문법: 비트 묶기 `{ }`.

- [ ] **Step 4: 검증과 셀 수 비교**

Run: 두 모듈 각각 `make -C $S test MOD=...`, `lint`, `synth`
Expected: 둘 다 PASS와 lint 경고 0. 두 `synth`의 셀 수와 종류를 나란히 기록하고 차이를 사용자와 읽음

- [ ] **Step 5: Commit**

`[Chip] 1단계: 8비트 가산기 두 벌`, 파일 다섯 개 pathspec.

### Task 5: 멀티플렉서와 래치

이 실습이 답할 질문: `always @(*)` 안에서 조합 회로를 쓸 때, 모든 경우에 값을 주지 않으면 무엇이 생기나.

**Files:**
- Create: `stages/01-comb/test/test_mux4.py`
- Create: `stages/01-comb/rtl/mux4.v`

**Interfaces:**
- Produces: `module mux4(input wire [7:0] d0, input wire [7:0] d1, input wire [7:0] d2, input wire [7:0] d3, input wire [1:0] sel, output reg [7:0] y)`. sel 값에 해당하는 d를 y로

- [ ] **Step 1: 테스트 작성**

`test_selects_each_input`: d0~d3에 서로 다른 값(0x11, 0x22, 0x44, 0x88)을 넣고 sel 0~3마다 y 확인.

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=mux4`
Expected: 실패

- [ ] **Step 3: RTL 작성**

새 문법: `always @(*)`, `case (sel)`, `default`. 출력이 `reg`인 이유를 함께 읽음.

- [ ] **Step 4: 검증**

Run: `make -C $S test MOD=mux4 && make -C $S lint MOD=mux4 && make -C $S synth MOD=mux4`
Expected: PASS, lint 경고 0, 래치 없음

- [ ] **Step 5: 일부러 래치 만들기**

case에서 `2'd3` 줄과 `default`를 지우고 `make -C $S synth MOD=mux4`. Expected: `select -assert-none`이 `$_DLATCH_`를 잡아 실패. `make -C $S lint MOD=mux4`도 돌려 Verilator 경고(`LATCH` 또는 `CASEINCOMPLETE`)를 확인. 테스트도 돌려 sel=3에서 y가 직전 값을 유지하는 것을 GTKWave로 확인. 되돌린 뒤 Step 4 다시 통과

- [ ] **Step 6: Commit**

`[Chip] 1단계: 4입력 멀티플렉서`, 파일 두 개 pathspec.

### Task 6: 3→8 디코더

이 실습이 답할 질문: 번호 하나를 받아 여러 선 중 하나만 켜는 회로는 어떻게 만드나. 멀티플렉서와는 어떤 관계인가.

3단계 레지스터 파일에서 "몇 번 레지스터에 쓸지" 고르는 데 이 모양을 씁니다.

**Files:**
- Create: `stages/01-comb/test/test_decoder3to8.py`
- Create: `stages/01-comb/rtl/decoder3to8.v`

**Interfaces:**
- Produces: `module decoder3to8(input wire [2:0] sel, input wire en, output wire [7:0] y)`. en=1이면 y의 sel번째 비트만 1, en=0이면 y=0

- [ ] **Step 1: 테스트 작성**

`test_one_hot`: en=1에서 sel 0~7 전수, `y == 1 << sel`. `test_disabled_is_zero`: en=0에서 sel 0~7 전수, `y == 0`.

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=decoder3to8`
Expected: 실패

- [ ] **Step 3: RTL 작성**

새 문법: 시프트 `8'b1 << sel`, 조건 연산자 `en ? ... : 8'b0`.

- [ ] **Step 4: 검증**

Run: `make -C $S test MOD=decoder3to8 && make -C $S lint MOD=decoder3to8 && make -C $S synth MOD=decoder3to8`
Expected: 2개 PASS, lint 경고 0, 셀 수 기록

- [ ] **Step 5: Commit**

`[Chip] 1단계: 3→8 디코더`, 파일 두 개 pathspec.

### Task 7: 32비트 ALU

이 실습이 답할 질문: CPU가 명령어마다 다른 계산을 할 때, 한 회로가 그 계산들을 어떻게 골라서 하나.

이 ALU는 4a단계 RV32I CPU에서 그대로 씁니다. op는 RV32I의 {funct7[5], funct3}입니다.

**Files:**
- Create: `stages/01-comb/test/test_alu.py`
- Create: `stages/01-comb/rtl/alu.v`

**Interfaces:**
- Produces: `module alu(input wire [31:0] a, input wire [31:0] b, input wire [3:0] op, output reg [31:0] y)`

| op | 이름 | y |
|---|---|---|
| 4'b0000 | ADD | a + b |
| 4'b1000 | SUB | a - b |
| 4'b0001 | SLL | a << b[4:0] |
| 4'b0010 | SLT | 부호 있는 a < b이면 1, 아니면 0 |
| 4'b0011 | SLTU | 부호 없는 a < b이면 1, 아니면 0 |
| 4'b0100 | XOR | a ^ b |
| 4'b0101 | SRL | a >> b[4:0] |
| 4'b1101 | SRA | 부호 있는 a >>> b[4:0] |
| 4'b0110 | OR | a \| b |
| 4'b0111 | AND | a & b |
| 그 외 | | 0 |

- [ ] **Step 1: 테스트 작성**

테스트 파일 안에 Python 기준 모델 `def reference(a: int, b: int, op: int) -> int`(위 표, 결과는 `& 0xFFFFFFFF`). 테스트:
- `test_random_against_reference`: `random.seed(0)`, op마다 무작위 200 경우
- `test_sub_wraps`: SUB 0 - 1 == 0xFFFFFFFF
- `test_slt_signed_vs_unsigned`: a=0xFFFFFFFF, b=1에서 SLT 1, SLTU 0
- `test_sra_fills_sign`: SRA 0x80000000, 4 == 0xF8000000. SRL은 0x08000000
- `test_shift_uses_low_5_bits`: SLL 1, 33 == 2
- `test_unknown_op_is_zero`: op 4'b1001 → 0

- [ ] **Step 2: RTL 없이 실패 확인**

Run: `make -C $S test MOD=alu`
Expected: 실패

- [ ] **Step 3: RTL 작성**

`always @(*)`와 `case (op)`. op 이름은 `localparam`. 새 문법: `$signed()`, `>>>`.

- [ ] **Step 4: 검증**

Run: `make -C $S test MOD=alu && make -C $S lint MOD=alu && make -C $S synth MOD=alu`
Expected: 6개 PASS, lint 경고 0, 래치 없음. 셀 수와 종류 기록

- [ ] **Step 5: Commit**

`[Chip] 1단계: 32비트 ALU`, 파일 두 개 pathspec.

### Task 8: 단계 기록

**Files:**
- Create: `stages/01-comb/notes.md`

- [ ] **Step 1: notes.md 작성**

모듈 표(모듈 | 답한 질문 | 테스트 수 | 셀 수 | 셀 종류), 가산기 두 벌 비교, 래치 실험 결과(합성 메시지, lint 경고, 파형에서 본 동작), ALU 셀 수 중 가장 큰 연산. 확인 못 한 것은 "확인 필요".

- [ ] **Step 2: 문체 검사**

Run: `/home/soungmin-kim/Desktop/develop/tools/doc-lint.sh $S/notes.md`
Expected: `위반 없음`

- [ ] **Step 3: Commit**

`[Chip] 1단계: 기록`, notes.md pathspec.
