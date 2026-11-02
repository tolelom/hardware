# 베이스 컴프레서 넷리스트 (자동 생성)

gen_schematic.py가 만든다. 손으로 고치지 않는다. 값의 근거는
[schematic.md](../docs/schematic.md).

## 부품별 접속

| 지정자 | 값 | 핀=넷 | 비고 |
|---|---|---|---|
| J3 | DC 9V 센터 마이너스 | 1=GND, 2=VIN_JACK | 핀 1 센터 = GND, 핀 2 슬리브 = +9V |
| FB1 | 600R@100MHz | 1=VIN_JACK, 2=VIN_RAW | 어댑터 선으로 들어오는 고주파와 페달 안 스위칭 잡음(LT1054, 화면)이 밖으로 나가는 것을 막는다. Seed3 Pedal Dev Kit 전원 입력과 같은 구성 |
| Q1 | IRF9540N | 1=VG, 2=VIN_RAW, 3=VS | 역극성 보호. 드레인이 잭 쪽 |
| Q2 | IRF9540N | 1=VG, 2=V9, 3=VS | 과전압 차단. 드레인이 부하 쪽. Q1과 소스끼리 맞댐 |
| R13 | 100k | 1=VG, 2=GND | 공통 게이트 풀다운 |
| Q3 | 2N3906 | 1=VS, 2=OV_B, 3=VG | 과전압이면 게이트를 소스로 끌어올린다 |
| R14 | 10k | 1=VS, 2=OV_B | Q3 에미터-베이스. 평상시 Q3 차단 |
| R15 | 4.7k | 1=OV_B, 2=ZK | Q3 베이스 전류 제한 |
| D3 | 1N4742A 12V | 1=ZK, 2=GND | 임계 약 12.7V |
| D14 | 1N4744A 15V | 1=VS, 2=VG | Q1·Q2 게이트 보호. 캐소드가 소스. 꽂는 순간 Vgs를 15V로 제한 |
| C7 | 1000u 25V | 1=V9, 2=GND | 벌크. 차단 시 VIN 4V까지 약 22ms 유지 |
| U2 | LT1054CP | 8=V9, 3=GND, 5=GND, 2=PUMP, 1=NC, 4=NC, 6=NC, 7=NC | 양전압 더블러. VOUT은 GND, CAP-·FB·OSC·VREF 개방 |
| C16 | 10u 50V | 1=V9, 2=GND | LT1054 입력. 핀 8 가까이 |
| C8 | 10u 50V | 1=PUMPX, 2=PUMP | 펌프. + 쪽이 다이오드 접점 |
| D1 | 1N5819 | 1=PUMPX, 2=V9 | 애노드 9V |
| D2 | 1N5819 | 1=VRAIL_RAW, 2=PUMPX |  |
| C9 | 100u 35V | 1=VRAIL_RAW, 2=GND | 더블러 출력 |
| R16 | 10R | 1=VRAIL_RAW, 2=VRAIL | 레일 필터. 6mA에서 60mV 강하 |
| C10 | 100u 35V | 1=VRAIL, 2=GND | 레일 필터. 차단 159Hz |
| C17 | 100n | 1=VRAIL, 2=GND | U1 핀 8 가까이 |
| R11 | 10k | 1=VRAIL, 2=VBIAS | Vbias 분압 상 |
| R12 | 10k | 1=VBIAS, 2=GND | Vbias 분압 하 |
| C6 | 100u 35V | 1=VBIAS, 2=GND | Vbias. 20Hz에서 80옴 |
| R32 | 3.3R | 1=V9, 2=SEED_VIN | VIN 필터. 150mA에서 0.5V 강하 |
| C11 | 100u 35V | 1=SEED_VIN, 2=GND | VIN 필터. 차단 0.5kHz |
| C12 | 100n | 1=SEED_VIN, 2=GND | 핀 39 가까이 |
| R19 | 47k | 1=V9, 2=VMON | 감시 분압 상. 18V에서 3.16V |
| R20 | 10k | 1=VMON, 2=GND | 감시 분압 하 |
| C14 | 100n | 1=VMON, 2=GND | 시정수 0.8ms |
| R21 | 1k | 1=VMON, 2=SEED_A5_VMON | ADC 핀 직렬 |
| J1 | IN 6.35mm TS | T=IN_TIP, S=GND |  |
| R1 | 2.2M | 1=IN_A, 2=GND | R_pd. K1 NO 접점 쪽 |
| R2 | 10k | 1=IN_A, 2=IN_B | 입력 클램프 전류 제한 |
| C1 | 100n 필름 | 1=IN_B, 2=IN_C | 입력 커플링. R3와 0.9Hz |
| C24 | 100p C0G 50V | 1=IN_C, 2=GND | RF 필터. R2 10k와 159kHz. 버퍼 입력 핀 가까이. Seed3 Pedal Dev Kit의 100Ω/100pF와 같은 목적 |
| R3 | 1.8M | 1=IN_C, 2=VBIAS | R_bias. R1과 병렬 약 1M |
| U1A | OPA2992 | 3=IN_C, 2=BUF_OUT, 1=BUF_OUT | A: 유니티 버퍼 |
| C2 | 47u 50V | 1=BUF_OUT, 2=PAD_IN | 패드 커플링. 0.85Hz. + 쪽이 버퍼 |
| R4 | 3.3k | 1=PAD_IN, 2=SEED_AUDIO_IN1 | 패드 직렬 |
| R5 | 820R | 1=SEED_AUDIO_IN1, 2=GND | 패드 션트. 코덱 5k 포함 -15.1dB |
| C23 | 4.7n | 1=SEED_AUDIO_IN1, 2=GND | 종단. 580Ω과 58kHz |
| C3 | 1u 필름 | 1=SEED_AUDIO_OUT1, 2=OUT_IN | R6와 1.6Hz |
| R6 | 100k | 1=OUT_IN, 2=VBIAS | 출력단 + 입력 바이어스 |
| U1B | OPA2992 | 5=OUT_IN, 6=FB, 7=U1B_OUT | B: 비반전 +15.1dB |
| U1 전원 | OPA2992 | 8=VRAIL, 4=GND | 전원 |
| R7 | 47k | 1=U1B_OUT, 2=FB | 피드백. 게인 1 + 47k/10k = 5.7 |
| R8 | 10k | 1=FB, 2=FB_C |  |
| C4 | 10u 50V | 1=FB_C, 2=GND | DC 게인 1. + 쪽이 R8 |
| R9 | 100R | 1=U1B_OUT, 2=OUT_R | 케이블 용량 분리 |
| C5 | 10u 50V | 1=OUT_R, 2=OUT_C | 출력 커플링. + 쪽이 op-amp |
| R10 | 100k | 1=OUT_C, 2=GND | 방전 R. K1 NO 접점 쪽 |
| J2 | OUT 6.35mm TS | T=OUT_TIP, S=GND |  |
| K1 | TQ2-9V | A1=V9, A2=K1_COIL, 11=IN_TIP, 12=BYPASS, 14=IN_A, 21=OUT_TIP, 22=BYPASS, 24=OUT_C | 코일 꺼짐 = NC = 트루 바이패스. 11·21 COM, 12·22 NC, 14·24 NO |
| D4 | 1N4148 | 1=V9, 2=K1_COIL | 플라이백. 캐소드 9V |
| Q4 | 2N3904 | 1=GND, 2=Q4_B, 3=K1_COIL |  |
| R17 | 2.2k | 1=SEED_D0_K1DRV, 2=Q4_B | 베이스 1.2mA |
| R18 | 10k | 1=Q4_B, 2=GND | 리셋 중 K1 해제 |
| RV1 | B10K Input | 1=SEED_3V3A, 2=SEED_A0_INPUT, 3=GND |  |
| RV2 | B10K Ratio | 1=SEED_3V3A, 2=SEED_A1_RATIO, 3=GND |  |
| RV3 | B10K Att/Rel | 1=SEED_3V3A, 2=SEED_A2_ATTREL, 3=GND |  |
| RV4 | B10K Blend | 1=SEED_3V3A, 2=SEED_A3_BLEND, 3=GND |  |
| RV5 | B10K Output | 1=SEED_3V3A, 2=SEED_A4_OUTPUT, 3=GND |  |
| C18 | 100n | 1=SEED_A0_INPUT, 2=GND |  |
| C19 | 100n | 1=SEED_A1_RATIO, 2=GND |  |
| C20 | 100n | 1=SEED_A2_ATTREL, 2=GND |  |
| C21 | 100n | 1=SEED_A3_BLEND, 2=GND |  |
| C22 | 100n | 1=SEED_A4_OUTPUT, 2=GND |  |
| SW1 | 풋스위치 모멘터리 | 1=SEED_D1_FSW, 2=GND |  |
| R22 | 10k | 1=SEED_3V3D, 2=SEED_D1_FSW | 풀업 |
| C15 | 100n | 1=SEED_D1_FSW, 2=GND | 디바운스 보조 |
| R23 | 470R | 1=SEED_D2_METER1, 2=LED1_A |  |
| D5 | 3mm 미터 1 | 1=GND, 2=LED1_A |  |
| R24 | 470R | 1=SEED_D3_METER2, 2=LED2_A |  |
| D6 | 3mm 미터 2 | 1=GND, 2=LED2_A |  |
| R25 | 470R | 1=SEED_D4_METER3, 2=LED3_A |  |
| D7 | 3mm 미터 3 | 1=GND, 2=LED3_A |  |
| R26 | 470R | 1=SEED_D5_METER4, 2=LED4_A |  |
| D8 | 3mm 미터 4 | 1=GND, 2=LED4_A |  |
| R27 | 470R | 1=SEED_D6_METER5, 2=LED5_A |  |
| D9 | 3mm 미터 5 | 1=GND, 2=LED5_A |  |
| R28 | 470R | 1=SEED_D7_METER6, 2=LED6_A |  |
| D10 | 3mm 미터 6 | 1=GND, 2=LED6_A |  |
| R29 | 470R | 1=SEED_D8_METER7, 2=LED7_A |  |
| D11 | 3mm 미터 7 | 1=GND, 2=LED7_A |  |
| R30 | 470R | 1=SEED_D9_METER8, 2=LED8_A |  |
| D12 | 3mm 미터 8 | 1=GND, 2=LED8_A |  |
| R31 | 470R | 1=SEED_D10_LEDON, 2=LED9_A |  |
| D13 | 3mm 온 | 1=GND, 2=LED9_A |  |
| J4 | Daisy Seed3 2x20 소켓 | 1=SEED_D0_K1DRV, 2=SEED_D1_FSW, 3=SEED_D2_METER1, 4=SEED_D3_METER2, 5=SEED_D4_METER3, 6=SEED_D5_METER4, 7=SEED_D6_METER5, 8=SEED_D7_METER6, 9=SEED_D8_METER7, 10=SEED_D9_METER8, 11=SEED_D10_LEDON, 12=NC, 13=NC, 14=NC, 15=NC, 16=SEED_AUDIO_IN1, 17=NC, 18=SEED_AUDIO_OUT1, 19=NC, 20=GND, 21=SEED_3V3A, 22=SEED_A0_INPUT, 23=SEED_A1_RATIO, 24=SEED_A2_ATTREL, 25=SEED_A3_BLEND, 26=SEED_A4_OUTPUT, 27=SEED_A5_VMON, 28=NC, 29=NC, 30=NC, 31=NC, 32=NC, 33=NC, 34=NC, 35=NC, 36=NC, 37=NC, 38=SEED_3V3D, 39=SEED_VIN, 40=GND | 16 Audio In 1, 18 Audio Out 1, 20 AGND, 21 3V3A, 38 3V3D, 39 VIN, 40 DGND. 12~15(D11~D14)는 비움 |
| PF1 | PWR_FLAG | 1=VIN_RAW |  |
| PF2 | PWR_FLAG | 1=V9 |  |
| PF3 | PWR_FLAG | 1=VRAIL |  |
| PF4 | PWR_FLAG | 1=SEED_3V3A |  |
| PF6 | PWR_FLAG | 1=SEED_3V3D |  |

## 넷별 핀

| 넷 | 핀 |
|---|---|
| BUF_OUT | U1.2, U1.1, C2.1 |
| BYPASS | K1.12, K1.22 |
| FB | U1.6, R7.2, R8.1 |
| FB_C | R8.2, C4.1 |
| GND | J3.1, R13.2, D3.2, C7.2, U2.3, U2.5, C16.2, C9.2, C10.2, C17.2, R12.2, C6.2, C11.2, C12.2, R20.2, C14.2, J1.S, R1.2, C24.2, R5.2, C23.2, U1.4, C4.2, R10.2, J2.S, Q4.1, R18.2, RV1.3, RV2.3, RV3.3, RV4.3, RV5.3, C18.2, C19.2, C20.2, C21.2, C22.2, SW1.2, C15.2, D5.1, D6.1, D7.1, D8.1, D9.1, D10.1, D11.1, D12.1, D13.1, J4.20, J4.40 |
| IN_A | R1.1, R2.1, K1.14 |
| IN_B | R2.2, C1.1 |
| IN_C | C1.2, C24.1, R3.1, U1.3 |
| IN_TIP | J1.T, K1.11 |
| K1_COIL | K1.A2, D4.2, Q4.3 |
| LED1_A | R23.2, D5.2 |
| LED2_A | R24.2, D6.2 |
| LED3_A | R25.2, D7.2 |
| LED4_A | R26.2, D8.2 |
| LED5_A | R27.2, D9.2 |
| LED6_A | R28.2, D10.2 |
| LED7_A | R29.2, D11.2 |
| LED8_A | R30.2, D12.2 |
| LED9_A | R31.2, D13.2 |
| OUT_C | C5.2, R10.1, K1.24 |
| OUT_IN | C3.2, R6.1, U1.5 |
| OUT_R | R9.2, C5.1 |
| OUT_TIP | J2.T, K1.21 |
| OV_B | Q3.2, R14.2, R15.1 |
| PAD_IN | C2.2, R4.1 |
| PUMP | U2.2, C8.2 |
| PUMPX | C8.1, D1.1, D2.2 |
| Q4_B | Q4.2, R17.2, R18.1 |
| SEED_3V3A | RV1.1, RV2.1, RV3.1, RV4.1, RV5.1, J4.21, PF4.1 |
| SEED_3V3D | R22.1, J4.38, PF6.1 |
| SEED_A0_INPUT | RV1.2, C18.1, J4.22 |
| SEED_A1_RATIO | RV2.2, C19.1, J4.23 |
| SEED_A2_ATTREL | RV3.2, C20.1, J4.24 |
| SEED_A3_BLEND | RV4.2, C21.1, J4.25 |
| SEED_A4_OUTPUT | RV5.2, C22.1, J4.26 |
| SEED_A5_VMON | R21.2, J4.27 |
| SEED_AUDIO_IN1 | R4.2, R5.1, C23.1, J4.16 |
| SEED_AUDIO_OUT1 | C3.1, J4.18 |
| SEED_D0_K1DRV | R17.1, J4.1 |
| SEED_D10_LEDON | R31.1, J4.11 |
| SEED_D1_FSW | SW1.1, R22.2, C15.1, J4.2 |
| SEED_D2_METER1 | R23.1, J4.3 |
| SEED_D3_METER2 | R24.1, J4.4 |
| SEED_D4_METER3 | R25.1, J4.5 |
| SEED_D5_METER4 | R26.1, J4.6 |
| SEED_D6_METER5 | R27.1, J4.7 |
| SEED_D7_METER6 | R28.1, J4.8 |
| SEED_D8_METER7 | R29.1, J4.9 |
| SEED_D9_METER8 | R30.1, J4.10 |
| SEED_VIN | R32.2, C11.1, C12.1, J4.39 |
| U1B_OUT | U1.7, R7.1, R9.1 |
| V9 | Q2.2, C7.1, U2.8, C16.1, D1.2, R32.1, R19.1, K1.A1, D4.1, PF2.1 |
| VBIAS | R11.2, R12.1, C6.1, R3.2, R6.2 |
| VG | Q1.1, Q2.1, R13.1, Q3.3, D14.2 |
| VIN_JACK | J3.2, FB1.1 |
| VIN_RAW | FB1.2, Q1.2, PF1.1 |
| VMON | R19.2, R20.1, C14.1, R21.1 |
| VRAIL | R16.2, C10.1, C17.1, R11.1, U1.8, PF3.1 |
| VRAIL_RAW | D2.1, C9.1, R16.1 |
| VS | Q1.3, Q2.3, Q3.1, R14.1, D14.1 |
| ZK | R15.2, D3.1 |
