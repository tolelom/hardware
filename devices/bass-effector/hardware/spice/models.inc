* 모델 카드. gen_spice.py 생성물이 쓰는 이름이다. 값의 출처는 spice/README.md
.model D1N5819 D (IS=1e-8 N=1.05 RS=0.05)
.model D1N4148 D (IS=2.52n N=1.752 RS=0.568 BV=100 IBV=100u)
.model DZ12 D (IS=1e-12 BV=12 IBV=1m N=1)
.model DZ15 D (IS=1e-12 BV=15 IBV=1m N=1)
.model DLEDR D (IS=1e-20 N=1.9 RS=10)
.model Q2N3906 PNP (IS=1e-14 BF=200 VAF=50)
.model Q2N3904 NPN (IS=1e-14 BF=200 VAF=100)
.model PMOSQ PMOS (LEVEL=1 VTO=-3.7 KP=1.36 LAMBDA=0.001)
.model DBODY D (IS=1e-12 N=1.2 RS=0.02)
.subckt IRF9540N d g s
M1 d g s s PMOSQ W=1u L=1u
Db d s DBODY
.ends
