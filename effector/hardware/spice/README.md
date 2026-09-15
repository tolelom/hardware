# 회로 시뮬레이션 (ngspice)

회로 설계 문서 12절의 근거 파일이다. `ngspice -b <파일>`로 돌리면 `meas` 결과가 표준 출력에 나온다.

| 파일 | 무엇을 보나 | 모델의 한계 |
|---|---|---|
| `protection.cir` | 블록 A 백투백 보호. 입력 -18~+18V DC 스윕에서 V9, VS, VG | IRF9540N은 LEVEL 1 PMOS(VTO -3.7V, Rds 0.117Ω 상당)와 바디 다이오드, 제너는 BV 12V 이상 모델 |
| `doubler.cir` | 블록 B 더블러의 레일 평균, 리플, 기동 시간 | LT1054는 25kHz 스위치(온저항 25Ω) 동작 모델. 손실은 6mA에서 약 0.3V가 되게 잡았다 |
| `output_stage.cir` | 블록 E 출력단이 모듈 1Vrms를 받을 때 레일 16.9V와 16.1V에서의 스윙 | op-amp는 `opamp_rr.inc`(개루프 1e4, 1kHz 극점, 레일 안쪽 0.25V 제한) |
| `boot.cir` | 전원 투입 뒤 OUT_C(K1 NO 접점 쪽) 잔류 전압이 -60dBu에 닿는 시각 | 레일이 20ms, 모듈 출력 DC가 100ms에 서는 것으로 가정 |
| `dc_polarity.cir` | 극성 커패시터 C2, C4, C5, C6 양단 DC 전위 | 레일 16.6V, 모듈 출력 DC 1.65V |
| `cable_ti.cir` | TI OPA2992 모델로 출력단 계단 응답. 케이블 용량 0, 1nF, 4.7nF | TI PSpice 모델(SBOMC15) 필요. 아래 참조 |
| `frequency_response.py` | 커플링·종단·DSP DC 차단·코덱 HPF를 합친 20Hz~20kHz 응답 | 1차 구간 곱. SPICE 불필요 |

TI 모델은 재배포하지 않는다. ti.com의 OPA2992 페이지에서 `sbomc15.zip`을 받아 `opa2992.lib`를 이 폴더의
`vendor/`(git 제외)에 두고, 같은 폴더에 `set ngbehavior=psa` 한 줄의 `.spiceinit`을 만든 뒤 돌린다. 이 모델의
잡음 해석은 ngspice에서 쓰지 않는다. 무잡음 저항을 PSpice 전용 `T_ABS`로 지정해 ngspice에서는 잡음이 수백 배
크게 나온다(2026-09-15 확인). 잡음은 회로 설계 12절의 손계산으로 대신한다.

결과는 회로 설계 문서 12절 표에 있다. 실제 부품 모델(제조사 SPICE)로 바꾸면 값이 달라질 수 있으므로 판정은 2단계 측정(T1, T6, T11, T12)이 한다.
