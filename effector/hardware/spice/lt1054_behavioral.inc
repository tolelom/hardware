* LT1054 동작 모델. 25kHz 스위치(온저항 RON)가 CAP+를 VIN과 GND 사이로 흔든다. 손실은 6mA에서 약 0.3V
.param ron_value=25
.subckt lt1054 vin capp gnd params: ron={ron_value}
Vosc osc gnd PULSE(0 1 0 100n 100n 20u 40u)
Boscn oscn gnd V = 1 - V(osc)
S1 vin capp osc gnd SWM
S2 capp gnd oscn gnd SWM
.model SWM SW (RON={ron} ROFF=1e9 VT=0.5 VH=0.1)
.ends
