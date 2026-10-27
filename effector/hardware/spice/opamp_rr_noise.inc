* opamp_rr.inc의 동작에 입력 잡음원을 더한 opamp_model. en: 입력 전압 잡음(V/rtHz), in: 입력 전류 잡음(A/rtHz)
* OPA2992 데이터시트 1kHz 7nV/rtHz, 10kHz 4.4nV/rtHz, 60fA/rtHz. 잡음원은 동작점과 분리된 1k 저항의 열잡음
.subckt opamp_model inp inn out vcc vee params: en=7n in=60f
.param inoise_1k = {sqrt(4 * 1.380649e-23 * 300.15 / 1000)}
Rvn nv 0 1k
Vvn nv 0 0
Hvn inp inpn Vvn {en / inoise_1k}
Rip np 0 1k
Vip np 0 0
Fip 0 inpn Vip {in / inoise_1k}
Rim nm 0 1k
Vim nm 0 0
Fim 0 inn Vim {in / inoise_1k}
B1 a 0 V = 1e4 * (V(inpn) - V(inn))
R1 a b 1k noisy=0
C1 b 0 159n
B2 c 0 V = max(min(V(b), V(vcc) - 0.25), V(vee) + 0.25)
R2 c out 50 noisy=0
.ends
