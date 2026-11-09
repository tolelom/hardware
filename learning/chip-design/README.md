# 칩 설계

> 디지털 논리부터 시작해 CPU와 NPU를 Verilog로 설계하고, 작은 설계를 Tiny Tapeout 셔틀로 실리콘까지 받는 학습 트랙입니다.

목표, 범위, 단계는 [설계서](design.md)에 있습니다. 단계표는 설계서 2절입니다.

## 도구

기준 머신은 Ubuntu 26.04.1 LTS 데스크톱입니다.

| 도구 | 용도 | 설치 | 버전 |
|---|---|---|---|
| Icarus Verilog | 시뮬레이션 | `sudo apt install iverilog` | 12.0 |
| Verilator | lint | `sudo apt install verilator` | 5.032 |
| Yosys | 합성 | `sudo apt install yosys` | 0.52 |
| GTKWave | 파형 보기 | `sudo apt install gtkwave` | 3.3.126 |
| cocotb | 테스트벤치 | 아래 venv | 2.1.0 |
| pytest | cocotb 테스트 실행 | 아래 venv | 8.4.2 |

## Python 환경

cocotb는 이 폴더의 venv에 둡니다. 버전은 TT 템플릿의 test/requirements.txt와 같게 [requirements.txt](requirements.txt)에 고정합니다.

```bash
uv venv --python 3.11 .venv
uv pip install -p .venv -r requirements.txt
source .venv/bin/activate
```
