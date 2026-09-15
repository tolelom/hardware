* 단순 op-amp 모델. OPA2992 대용: 개루프 1e4, 1kHz 극점(GBW 10MHz), 출력 저항 50옴, 레일 안쪽 0.25V에서 제한.
.subckt opamp_rr inp inn out vcc vee
B1 a 0 V = 1e4 * (V(inp) - V(inn))
R1 a b 1k
C1 b 0 159n
B2 c 0 V = max(min(V(b), V(vcc) - 0.25), V(vee) + 0.25)
R2 c out 50
.ends
