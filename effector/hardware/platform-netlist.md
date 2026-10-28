# 멀티 이펙터 플랫폼 넷리스트 (자동 생성)

gen_schematic.py가 만든다. 손으로 고치지 않는다. 값의 근거는
[bass-multi-platform-schematic.md](../../devices/bass-multi-platform-schematic.md).

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
| R21 | 1k | 1=VMON, 2=SEED_D20_VMON_ADC | ADC 핀 직렬 |
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
| K1 | TQ2-9V | A1=V9, A2=K1_COIL, 11=IN_TIP, 12=BYPASS, 14=IN_A, 21=OUT_TIP, 22=BYPASS, 24=OUT_C | 코일 꺼짐 = NC = 트루 바이패스. 11·21 COM, 12·22 NC, 14·24 NO |
| D4 | 1N4148 | 1=V9, 2=K1_COIL | 플라이백. 캐소드 9V |
| Q4 | 2N3904 | 1=GND, 2=Q4_B, 3=K1_COIL |  |
| R17 | 2.2k | 1=SEED_D0_K1_DRIVE, 2=Q4_B | 베이스 1.2mA |
| R18 | 10k | 1=Q4_B, 2=GND | 리셋 중 K1 해제 |
| J4 | Daisy Seed3 2x20 소켓 | 1=SEED_D0_K1_DRIVE, 2=SEED_D1_SDMMC1_D3, 3=SEED_D2_SDMMC1_D2, 4=SEED_D3_SDMMC1_D1, 5=SEED_D4_SDMMC1_D0, 6=SEED_D5_SDMMC1_CMD, 7=SEED_D6_SDMMC1_CK, 8=SEED_D7_SPI1_NSS, 9=SEED_D8_SPI1_SCK, 10=NC, 11=SEED_D10_SPI1_MOSI, 12=SEED_D11_I2C1_SCL, 13=SEED_D12_I2C1_SDA, 14=SEED_D13_USART1_TX, 15=SEED_D14_USART1_RX, 16=SEED_AUDIO_IN1, 17=NC, 18=SEED_AUDIO_OUT1, 19=SEED_AUDIO_OUT2, 20=GND, 21=SEED_3V3A, 22=SEED_D15_EXP_ADC, 23=SEED_D16_DISPLAY_BL_PWM, 24=SEED_D17_DISPLAY_DC, 25=SEED_D18_DISPLAY_RST, 26=SEED_D19_MCP_INT_A, 27=SEED_D20_VMON_ADC, 28=SEED_D21_MCP_INT_B, 29=SEED_D22_FS1, 30=SEED_D23_FS2, 31=SEED_D24_FS3, 32=SEED_D25_LED1, 33=SEED_D26_LED2, 34=SEED_D27_LED3, 35=NC, 36=NC, 37=NC, 38=SEED_3V3D, 39=SEED_VIN, 40=GND | 핀 번호는 Seed3 데이터시트 v2.1.0 표 2 물리 핀. 배정은 pinmap.PLATFORM. 17 Audio In 2, D9·D28~D30은 비움. 38 3V3D는 MIDI 회로 전원 |
| J10 | MIDI IN 3.5mm TRS | T=MIDI_IN_SINK, R=MIDI_IN_SRC, S=NC | RP-054: 팁 = DIN 5(싱크), 링 = DIN 4(소스), 슬리브 = DIN 2(실드). 수신 쪽 실드는 접지하지 않는다 |
| R40 | 220R | 1=MIDI_IN_SRC, 2=OPTO_A | CA-033 그림 2 RB |
| D30 | 1N4148 | 1=OPTO_A, 2=MIDI_IN_SINK | CA-033 그림 2 D1(1N914). LED 역병렬, 역전압 보호 |
| U11 | H11L1 | 1=OPTO_A, 2=MIDI_IN_SINK, 3=NC, 4=SEED_D14_USART1_RX, 5=GND, 6=SEED_3V3D | H11L1M 데이터시트: VCC 3~15V, IF(on) 최대 1.6mA. 1 애노드, 2 캐소드, 4 출력(오픈 컬렉터), 5 GND, 6 VCC. Daisy Pod Rev5와 같은 계열 |
| R41 | 270R | 1=SEED_3V3D, 2=SEED_D14_USART1_RX | 출력 풀업. Daisy Pod Rev5 R64와 같은 값 |
| C40 | 100n | 1=SEED_3V3D, 2=GND | U11 핀 6 가까이 |
| J11 | MIDI OUT 3.5mm TRS | T=MIDI_OUT_SINK, R=MIDI_OUT_SRC, S=GND | RP-054. 송신 쪽 실드는 접지 |
| R42 | 33R 0.5W | 1=3V3_AUX, 2=MIDI_OUT_SRC | CA-033 그림 1 RA, 3.3V ±5% 값. TS 플러그가 링을 GND에 붙이면 100mA(0.33W)가 흐르므로 Seed3 3V3D가 아니라 2A 보조 레일에서 받는다 |
| R43 | 10R | 1=SEED_D13_USART1_TX, 2=MIDI_OUT_SINK | CA-033 그림 1 RC, 3.3V ±5% 값 |
| J12 | EXP 6.35mm TRS | T=EXP_TIP, R=EXP_RING, S=GND | 팁 와이퍼, 링 기준, 슬리브 GND |
| R50 | 10k | 1=SEED_3V3A, 2=EXP_RING | 링 기준 직렬. 꽂힌 페달의 최대값을 R/(R+10k)로 낮춰 뺀 상태(전체 눈금)와 구분한다. 10k 페달 50%, 100k 90.9%, 250k 96.2%(문턱 97%) |
| R51 | 100k | 1=SEED_3V3A, 2=EXP_TIP | 팁 풀업. 뺐을 때 ADC가 전체 눈금 = 꽂힘 감지 |
| R52 | 1k | 1=EXP_TIP, 2=SEED_D15_EXP_ADC | ADC 직렬, 정전기 전류 제한 |
| C50 | 100n | 1=SEED_D15_EXP_ADC, 2=GND | R52와 1.6kHz |
| J13 | microSD 푸시푸시 소켓 | 1=SEED_D2_SDMMC1_D2, 2=SEED_D1_SDMMC1_D3, 3=SEED_D5_SDMMC1_CMD, 4=3V3_AUX, 5=SD_CLK, 6=GND, 7=SEED_D4_SDMMC1_D0, 8=SEED_D3_SDMMC1_D1, 9=GND | SD 물리 계층 규격: CMD·DAT 풀업 필요, CLK는 풀업 없음 |
| R53 | 33R | 1=SEED_D6_SDMMC1_CK, 2=SD_CLK | CLK 직렬. 반사 완화 |
| R54 | 47k | 1=3V3_AUX, 2=SEED_D5_SDMMC1_CMD | 풀업 |
| R55 | 47k | 1=3V3_AUX, 2=SEED_D4_SDMMC1_D0 | 풀업 |
| R56 | 47k | 1=3V3_AUX, 2=SEED_D3_SDMMC1_D1 | 풀업 |
| R57 | 47k | 1=3V3_AUX, 2=SEED_D2_SDMMC1_D2 | 풀업 |
| R58 | 47k | 1=3V3_AUX, 2=SEED_D1_SDMMC1_D3 | 풀업 |
| C51 | 100n | 1=3V3_AUX, 2=GND | 소켓 VDD 가까이 |
| C52 | 10u | 1=3V3_AUX, 2=GND | 삽입 순간 돌입 전류 |
| J2 | OUT 6.35mm TRS | T=OUT_TIP, R=OUT2_TIP, S=GND | 1호 TS 잭을 TRS로 바꿨다. 링 = 스테레오 자리. 모노 TS 플러그는 링을 GND에 붙이므로 R70 100Ω이 단락 전류를 막는다 |
| C70 | 1u 필름 | 1=SEED_AUDIO_OUT2, 2=OUT2_IN | DNP(자리만). C3와 같음 |
| R71 | 100k | 1=OUT2_IN, 2=VBIAS | DNP(자리만). R6와 같음 |
| U3A | OPA2992 | 3=OUT2_IN, 2=OUT2_FB, 1=U3A_OUT | DNP(자리만). U1B와 같음 |
| U3B | OPA2992 | 5=VBIAS, 6=U3B_OUT, 7=U3B_OUT | DNP(자리만). 안 쓰는 반쪽. 버퍼로 VBIAS에 묶는다 |
| U3 전원 | OPA2992 | 8=VRAIL, 4=GND | DNP(자리만). 전원 |
| R72 | 47k | 1=U3A_OUT, 2=OUT2_FB | DNP(자리만). R7과 같음 |
| R73 | 10k | 1=OUT2_FB, 2=OUT2_FB_C | DNP(자리만). R8과 같음 |
| C71 | 10u 50V | 1=OUT2_FB_C, 2=GND | DNP(자리만). C4와 같음 |
| R70 | 100R | 1=U3A_OUT, 2=OUT2_R | DNP(자리만). R9와 같음 |
| C72 | 10u 50V | 1=OUT2_R, 2=OUT2_TIP | DNP(자리만). C5와 같음. 바이패스 때도 링은 K1을 거치지 않는다 |
| R74 | 100k | 1=OUT2_TIP, 2=GND | DNP(자리만). R10과 같음 |
| J20 | 2x10 2.54mm | 1=V9, 2=GND, 3=3V3_AUX, 4=GND, 5=SEED_D8_SPI1_SCK, 6=SEED_D10_SPI1_MOSI, 7=SEED_D7_SPI1_NSS, 8=SEED_D17_DISPLAY_DC, 9=SEED_D18_DISPLAY_RST, 10=SEED_D16_DISPLAY_BL_PWM, 11=SEED_D11_I2C1_SCL, 12=SEED_D12_I2C1_SDA, 13=SEED_D19_MCP_INT_A, 14=SEED_D21_MCP_INT_B, 15=SEED_D22_FS1, 16=SEED_D23_FS2, 17=SEED_D24_FS3, 18=SEED_D25_LED1, 19=SEED_D26_LED2, 20=SEED_D27_LED3 | 조작부 J21과 1:1. 접지는 이 헤더 한 곳에서만 잇는다 |
| PF1 | PWR_FLAG | 1=VIN_RAW |  |
| PF2 | PWR_FLAG | 1=V9 |  |
| PF3 | PWR_FLAG | 1=VRAIL |  |
| PF4 | PWR_FLAG | 1=SEED_3V3A |  |
| PF6 | PWR_FLAG | 1=SEED_3V3D |  |
| J21 | 2x10 2.54mm | 1=V9, 2=GND, 3=3V3_AUX, 4=GND, 5=SEED_D8_SPI1_SCK, 6=SEED_D10_SPI1_MOSI, 7=SEED_D7_SPI1_NSS, 8=SEED_D17_DISPLAY_DC, 9=SEED_D18_DISPLAY_RST, 10=SEED_D16_DISPLAY_BL_PWM, 11=SEED_D11_I2C1_SCL, 12=SEED_D12_I2C1_SDA, 13=SEED_D19_MCP_INT_A, 14=SEED_D21_MCP_INT_B, 15=SEED_D22_FS1, 16=SEED_D23_FS2, 17=SEED_D24_FS3, 18=SEED_D25_LED1, 19=SEED_D26_LED2, 20=SEED_D27_LED3 | 메인 J20과 1:1 |
| FB3 | 600R@100MHz 1A | 1=V9, 2=BUCK_VIN | 입력 파이 필터. 강압기 입력 리플과 백라이트 PWM 전류가 V9를 타고 메인 기판 아날로그단으로 가지 않게 한다 |
| C36 | 47u 25V | 1=V9, 2=GND | 파이 필터 J21 쪽 |
| U10 | TPS562208 | 1=GND, 2=BUCK_SW, 3=BUCK_VIN, 4=BUCK_FB, 5=BUCK_EN, 6=BUCK_BST | VIN 4.5~17V, 2A, 580kHz, 강제 연속 모드(FCCM). 경부하에서도 주파수가 오디오 대역으로 내려가지 않는다. TPS562201과 핀 호환, 같은 데이터시트(SLVSD91D). 심볼은 핀 배치가 같은 TPS562200 |
| C30 | 10u 25V | 1=BUCK_VIN, 2=GND | 입력. 7.2.2.4절: 10µF 이상, 최대 VIN 이상 정격 |
| C31 | 100n 25V | 1=BUCK_VIN, 2=GND | U10 핀 3 바로 옆 |
| R80 | 100k | 1=BUCK_VIN, 2=BUCK_EN | EN을 VIN 쪽으로. EN 절대 최대 19V |
| C32 | 100n | 1=BUCK_BST, 2=BUCK_SW | 부트스트랩. 7.2.2.5절 |
| L10 | 3.3u | 1=BUCK_SW, 2=3V3_BUCK | 표 7-2: 3.3V 출력 3.3µH(최대 4.7µH). 포화 3A 이상 |
| C33 | 22u 10V | 1=3V3_BUCK, 2=GND | 출력. 표 7-2 20~68µF |
| C34 | 22u 10V | 1=3V3_BUCK, 2=GND | 출력. C33과 합 44µF |
| R81 | 33.2k 1% | 1=3V3_BUCK, 2=BUCK_FB | 표 7-2 R1. 0.768 × (1 + 33.2/10) = 3.32V |
| R82 | 10k 1% | 1=BUCK_FB, 2=GND | 표 7-2 R2 |
| FB2 | 600R@100MHz 1A | 1=3V3_BUCK, 2=3V3_AUX | 스위칭 잡음을 조작부 안에 가둔다. 정격 1A 이상 |
| C35 | 10u 10V | 1=3V3_AUX, 2=GND | 페라이트 뒤 |
| PF7 | PWR_FLAG | 1=3V3_AUX |  |
| J22 | 화면 모듈 16핀 | 1=GND, 2=3V3_AUX, 3=SEED_D8_SPI1_SCK, 4=SEED_D10_SPI1_MOSI, 5=SEED_D18_DISPLAY_RST, 6=SEED_D17_DISPLAY_DC, 7=SEED_D7_SPI1_NSS, 8=DISPLAY_BLK, 9=NC, 10=NC, 11=NC, 12=NC, 13=NC, 14=NC, 15=NC, 16=NC | 데이터시트 2절: 1 GND, 2 VDD(2.5~3.6V), 3 SCL, 4 SDA, 5 RST, 6 DC, 7 CS, 8 BLK, 9~16 NC. 4선 SPI 전용. 커넥터 피치·형식은 기구 도면 확인 필요 |
| R60 | 100R | 1=SEED_D16_DISPLAY_BL_PWM, 2=DISPLAY_BLK | BLK 직렬. 링잉 완화 |
| R61 | 100k | 1=DISPLAY_BLK, 2=GND | 부팅 중 백라이트 꺼짐 |
| C60 | 10u 10V | 1=3V3_AUX, 2=GND | 백라이트 LED 6직렬 2병렬, VF 19.2V 40mA. 3.3V 쪽 약 0.27A(승압 효율 85% 가정) |
| C61 | 100n | 1=3V3_AUX, 2=GND | J22 핀 2 가까이 |
| U20 | MCP23017-E/SO | 1=ENC3_A, 2=ENC3_B, 3=ENC3_SW, 4=ENC4_A, 5=ENC4_B, 6=ENC4_SW, 7=NC, 8=NC, 9=3V3_AUX, 10=GND, 11=NC, 12=SEED_D11_I2C1_SCL, 13=SEED_D12_I2C1_SDA, 14=NC, 15=GND, 16=GND, 17=GND, 18=U20_RESET, 19=NC, 20=SEED_D19_MCP_INT_A, 21=ENC1_A, 22=ENC1_B, 23=ENC1_SW, 24=ENC2_A, 25=ENC2_B, 26=ENC2_SW, 27=NC, 28=NC | I2C 0x20. 데이터시트 DS20001952C 표 2-1 핀. INTA에 두 포트를 묶는다(IOCON.MIRROR). 누름 입력은 내부 풀업(GPPU) |
| R98 | 10k | 1=3V3_AUX, 2=U20_RESET | RESET 풀업 |
| C93 | 100n | 1=3V3_AUX, 2=GND | U20 핀 9 가까이 |
| U21 | MCP23017-E/SO | 1=NAV_A, 2=NAV_B, 3=NAV_SW, 4=NC, 5=NC, 6=NC, 7=NC, 8=NC, 9=3V3_AUX, 10=GND, 11=NC, 12=SEED_D11_I2C1_SCL, 13=SEED_D12_I2C1_SDA, 14=NC, 15=3V3_AUX, 16=GND, 17=GND, 18=U21_RESET, 19=NC, 20=SEED_D21_MCP_INT_B, 21=ENC5_A, 22=ENC5_B, 23=ENC5_SW, 24=ENC6_A, 25=ENC6_B, 26=ENC6_SW, 27=NC, 28=NC | I2C 0x21. 데이터시트 DS20001952C 표 2-1 핀. INTA에 두 포트를 묶는다(IOCON.MIRROR). 누름 입력은 내부 풀업(GPPU) |
| R99 | 10k | 1=3V3_AUX, 2=U21_RESET | RESET 풀업 |
| C94 | 100n | 1=3V3_AUX, 2=GND | U21 핀 9 가까이 |
| R96 | 4.7k | 1=3V3_AUX, 2=SEED_D11_I2C1_SCL | I2C 풀업 |
| R97 | 4.7k | 1=3V3_AUX, 2=SEED_D12_I2C1_SDA | I2C 풀업 |
| SW20 | EC11E 누름 ENC1 | A=ENC1_A_RAW, B=ENC1_B_RAW, C=GND, S1=ENC1_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R100 | 10k | 1=3V3_AUX, 2=ENC1_A_RAW | ALPS 권장 필터 풀업 |
| R101 | 10k | 1=ENC1_A_RAW, 2=ENC1_A | ALPS 권장 필터 직렬 |
| C100 | 10n | 1=ENC1_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R102 | 10k | 1=3V3_AUX, 2=ENC1_B_RAW | ALPS 권장 필터 풀업 |
| R103 | 10k | 1=ENC1_B_RAW, 2=ENC1_B | ALPS 권장 필터 직렬 |
| C101 | 10n | 1=ENC1_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW21 | EC11E 누름 ENC2 | A=ENC2_A_RAW, B=ENC2_B_RAW, C=GND, S1=ENC2_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R104 | 10k | 1=3V3_AUX, 2=ENC2_A_RAW | ALPS 권장 필터 풀업 |
| R105 | 10k | 1=ENC2_A_RAW, 2=ENC2_A | ALPS 권장 필터 직렬 |
| C102 | 10n | 1=ENC2_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R106 | 10k | 1=3V3_AUX, 2=ENC2_B_RAW | ALPS 권장 필터 풀업 |
| R107 | 10k | 1=ENC2_B_RAW, 2=ENC2_B | ALPS 권장 필터 직렬 |
| C103 | 10n | 1=ENC2_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW22 | EC11E 누름 ENC3 | A=ENC3_A_RAW, B=ENC3_B_RAW, C=GND, S1=ENC3_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R108 | 10k | 1=3V3_AUX, 2=ENC3_A_RAW | ALPS 권장 필터 풀업 |
| R109 | 10k | 1=ENC3_A_RAW, 2=ENC3_A | ALPS 권장 필터 직렬 |
| C104 | 10n | 1=ENC3_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R110 | 10k | 1=3V3_AUX, 2=ENC3_B_RAW | ALPS 권장 필터 풀업 |
| R111 | 10k | 1=ENC3_B_RAW, 2=ENC3_B | ALPS 권장 필터 직렬 |
| C105 | 10n | 1=ENC3_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW23 | EC11E 누름 ENC4 | A=ENC4_A_RAW, B=ENC4_B_RAW, C=GND, S1=ENC4_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R112 | 10k | 1=3V3_AUX, 2=ENC4_A_RAW | ALPS 권장 필터 풀업 |
| R113 | 10k | 1=ENC4_A_RAW, 2=ENC4_A | ALPS 권장 필터 직렬 |
| C106 | 10n | 1=ENC4_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R114 | 10k | 1=3V3_AUX, 2=ENC4_B_RAW | ALPS 권장 필터 풀업 |
| R115 | 10k | 1=ENC4_B_RAW, 2=ENC4_B | ALPS 권장 필터 직렬 |
| C107 | 10n | 1=ENC4_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW24 | EC11E 누름 ENC5 | A=ENC5_A_RAW, B=ENC5_B_RAW, C=GND, S1=ENC5_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R116 | 10k | 1=3V3_AUX, 2=ENC5_A_RAW | ALPS 권장 필터 풀업 |
| R117 | 10k | 1=ENC5_A_RAW, 2=ENC5_A | ALPS 권장 필터 직렬 |
| C108 | 10n | 1=ENC5_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R118 | 10k | 1=3V3_AUX, 2=ENC5_B_RAW | ALPS 권장 필터 풀업 |
| R119 | 10k | 1=ENC5_B_RAW, 2=ENC5_B | ALPS 권장 필터 직렬 |
| C109 | 10n | 1=ENC5_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW25 | EC11E 누름 ENC6 | A=ENC6_A_RAW, B=ENC6_B_RAW, C=GND, S1=ENC6_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R120 | 10k | 1=3V3_AUX, 2=ENC6_A_RAW | ALPS 권장 필터 풀업 |
| R121 | 10k | 1=ENC6_A_RAW, 2=ENC6_A | ALPS 권장 필터 직렬 |
| C110 | 10n | 1=ENC6_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R122 | 10k | 1=3V3_AUX, 2=ENC6_B_RAW | ALPS 권장 필터 풀업 |
| R123 | 10k | 1=ENC6_B_RAW, 2=ENC6_B | ALPS 권장 필터 직렬 |
| C111 | 10n | 1=ENC6_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW26 | EC11E 누름 NAV | A=NAV_A_RAW, B=NAV_B_RAW, C=GND, S1=NAV_SW, S2=GND | ALPS EC11E: A·C·B 한쪽, 스위치 두 핀 반대쪽. 무한 회전으로 쓴다 |
| R124 | 10k | 1=3V3_AUX, 2=NAV_A_RAW | ALPS 권장 필터 풀업 |
| R125 | 10k | 1=NAV_A_RAW, 2=NAV_A | ALPS 권장 필터 직렬 |
| C112 | 10n | 1=NAV_A, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| R126 | 10k | 1=3V3_AUX, 2=NAV_B_RAW | ALPS 권장 필터 풀업 |
| R127 | 10k | 1=NAV_B_RAW, 2=NAV_B | ALPS 권장 필터 직렬 |
| C113 | 10n | 1=NAV_B, 2=GND | ALPS 권장 필터 0.01µF. 10k와 16kHz |
| SW10 | 풋스위치 1 모멘터리 | 1=SEED_D22_FS1, 2=GND |  |
| R90 | 10k | 1=3V3_AUX, 2=SEED_D22_FS1 | 풀업. 1호 R22와 같음 |
| C90 | 100n | 1=SEED_D22_FS1, 2=GND | 디바운스 보조. 1호 C15와 같음 |
| R93 | 470R | 1=SEED_D25_LED1, 2=FSLED1_A | 약 2.8mA |
| D20 | 3mm 풋스위치 1 | 1=GND, 2=FSLED1_A |  |
| SW11 | 풋스위치 2 모멘터리 | 1=SEED_D23_FS2, 2=GND |  |
| R91 | 10k | 1=3V3_AUX, 2=SEED_D23_FS2 | 풀업. 1호 R22와 같음 |
| C91 | 100n | 1=SEED_D23_FS2, 2=GND | 디바운스 보조. 1호 C15와 같음 |
| R94 | 470R | 1=SEED_D26_LED2, 2=FSLED2_A | 약 2.8mA |
| D21 | 3mm 풋스위치 2 | 1=GND, 2=FSLED2_A |  |
| SW12 | 풋스위치 3 모멘터리 | 1=SEED_D24_FS3, 2=GND |  |
| R92 | 10k | 1=3V3_AUX, 2=SEED_D24_FS3 | 풀업. 1호 R22와 같음 |
| C92 | 100n | 1=SEED_D24_FS3, 2=GND | 디바운스 보조. 1호 C15와 같음 |
| R95 | 470R | 1=SEED_D27_LED3, 2=FSLED3_A | 약 2.8mA |
| D22 | 3mm 풋스위치 3 | 1=GND, 2=FSLED3_A |  |

## 넷별 핀

| 넷 | 핀 |
|---|---|
| 3V3_AUX | R42.1, J13.4, R54.1, R55.1, R56.1, R57.1, R58.1, C51.1, C52.1, J20.3, J21.3, FB2.2, C35.1, PF7.1, J22.2, C60.1, C61.1, U20.9, R98.1, C93.1, U21.15, U21.9, R99.1, C94.1, R96.1, R97.1, R100.1, R102.1, R104.1, R106.1, R108.1, R110.1, R112.1, R114.1, R116.1, R118.1, R120.1, R122.1, R124.1, R126.1, R90.1, R91.1, R92.1 |
| 3V3_BUCK | L10.2, C33.1, C34.1, R81.1, FB2.1 |
| BUCK_BST | U10.6, C32.1 |
| BUCK_EN | U10.5, R80.2 |
| BUCK_FB | U10.4, R81.2, R82.1 |
| BUCK_SW | U10.2, C32.2, L10.1 |
| BUCK_VIN | FB3.2, U10.3, C30.1, C31.1, R80.1 |
| BUF_OUT | U1.2, U1.1, C2.1 |
| BYPASS | K1.12, K1.22 |
| DISPLAY_BLK | J22.8, R60.2, R61.1 |
| ENC1_A | U20.21, R101.2, C100.1 |
| ENC1_A_RAW | SW20.A, R100.2, R101.1 |
| ENC1_B | U20.22, R103.2, C101.1 |
| ENC1_B_RAW | SW20.B, R102.2, R103.1 |
| ENC1_SW | U20.23, SW20.S1 |
| ENC2_A | U20.24, R105.2, C102.1 |
| ENC2_A_RAW | SW21.A, R104.2, R105.1 |
| ENC2_B | U20.25, R107.2, C103.1 |
| ENC2_B_RAW | SW21.B, R106.2, R107.1 |
| ENC2_SW | U20.26, SW21.S1 |
| ENC3_A | U20.1, R109.2, C104.1 |
| ENC3_A_RAW | SW22.A, R108.2, R109.1 |
| ENC3_B | U20.2, R111.2, C105.1 |
| ENC3_B_RAW | SW22.B, R110.2, R111.1 |
| ENC3_SW | U20.3, SW22.S1 |
| ENC4_A | U20.4, R113.2, C106.1 |
| ENC4_A_RAW | SW23.A, R112.2, R113.1 |
| ENC4_B | U20.5, R115.2, C107.1 |
| ENC4_B_RAW | SW23.B, R114.2, R115.1 |
| ENC4_SW | U20.6, SW23.S1 |
| ENC5_A | U21.21, R117.2, C108.1 |
| ENC5_A_RAW | SW24.A, R116.2, R117.1 |
| ENC5_B | U21.22, R119.2, C109.1 |
| ENC5_B_RAW | SW24.B, R118.2, R119.1 |
| ENC5_SW | U21.23, SW24.S1 |
| ENC6_A | U21.24, R121.2, C110.1 |
| ENC6_A_RAW | SW25.A, R120.2, R121.1 |
| ENC6_B | U21.25, R123.2, C111.1 |
| ENC6_B_RAW | SW25.B, R122.2, R123.1 |
| ENC6_SW | U21.26, SW25.S1 |
| EXP_RING | J12.R, R50.2 |
| EXP_TIP | J12.T, R51.2, R52.1 |
| FB | U1.6, R7.2, R8.1 |
| FB_C | R8.2, C4.1 |
| FSLED1_A | R93.2, D20.2 |
| FSLED2_A | R94.2, D21.2 |
| FSLED3_A | R95.2, D22.2 |
| GND | J3.1, R13.2, D3.2, C7.2, U2.3, U2.5, C16.2, C9.2, C10.2, C17.2, R12.2, C6.2, C11.2, C12.2, R20.2, C14.2, J1.S, R1.2, C24.2, R5.2, C23.2, U1.4, C4.2, R10.2, Q4.1, R18.2, J4.20, J4.40, U11.5, C40.2, J11.S, J12.S, C50.2, J13.6, J13.9, C51.2, C52.2, J2.S, U3.4, C71.2, R74.2, J20.2, J20.4, J21.2, J21.4, C36.2, U10.1, C30.2, C31.2, C33.2, C34.2, R82.2, C35.2, J22.1, R61.2, C60.2, C61.2, U20.17, U20.16, U20.15, U20.10, C93.2, U21.17, U21.16, U21.10, C94.2, SW20.C, SW20.S2, C100.2, C101.2, SW21.C, SW21.S2, C102.2, C103.2, SW22.C, SW22.S2, C104.2, C105.2, SW23.C, SW23.S2, C106.2, C107.2, SW24.C, SW24.S2, C108.2, C109.2, SW25.C, SW25.S2, C110.2, C111.2, SW26.C, SW26.S2, C112.2, C113.2, SW10.2, C90.2, D20.1, SW11.2, C91.2, D21.1, SW12.2, C92.2, D22.1 |
| IN_A | R1.1, R2.1, K1.14 |
| IN_B | R2.2, C1.1 |
| IN_C | C1.2, C24.1, R3.1, U1.3 |
| IN_TIP | J1.T, K1.11 |
| K1_COIL | K1.A2, D4.2, Q4.3 |
| MIDI_IN_SINK | J10.T, D30.2, U11.2 |
| MIDI_IN_SRC | J10.R, R40.1 |
| MIDI_OUT_SINK | J11.T, R43.2 |
| MIDI_OUT_SRC | J11.R, R42.2 |
| NAV_A | U21.1, R125.2, C112.1 |
| NAV_A_RAW | SW26.A, R124.2, R125.1 |
| NAV_B | U21.2, R127.2, C113.1 |
| NAV_B_RAW | SW26.B, R126.2, R127.1 |
| NAV_SW | U21.3, SW26.S1 |
| OPTO_A | R40.2, D30.1, U11.1 |
| OUT2_FB | U3.2, R72.2, R73.1 |
| OUT2_FB_C | R73.2, C71.1 |
| OUT2_IN | C70.2, R71.1, U3.3 |
| OUT2_R | R70.2, C72.1 |
| OUT2_TIP | J2.R, C72.2, R74.1 |
| OUT_C | C5.2, R10.1, K1.24 |
| OUT_IN | C3.2, R6.1, U1.5 |
| OUT_R | R9.2, C5.1 |
| OUT_TIP | K1.21, J2.T |
| OV_B | Q3.2, R14.2, R15.1 |
| PAD_IN | C2.2, R4.1 |
| PUMP | U2.2, C8.2 |
| PUMPX | C8.1, D1.1, D2.2 |
| Q4_B | Q4.2, R17.2, R18.1 |
| SD_CLK | J13.5, R53.2 |
| SEED_3V3A | J4.21, R50.1, R51.1, PF4.1 |
| SEED_3V3D | J4.38, U11.6, R41.1, C40.1, PF6.1 |
| SEED_AUDIO_IN1 | R4.2, R5.1, C23.1, J4.16 |
| SEED_AUDIO_OUT1 | C3.1, J4.18 |
| SEED_AUDIO_OUT2 | J4.19, C70.1 |
| SEED_D0_K1_DRIVE | R17.1, J4.1 |
| SEED_D10_SPI1_MOSI | J4.11, J20.6, J21.6, J22.4 |
| SEED_D11_I2C1_SCL | J4.12, J20.11, J21.11, U20.12, U21.12, R96.2 |
| SEED_D12_I2C1_SDA | J4.13, J20.12, J21.12, U20.13, U21.13, R97.2 |
| SEED_D13_USART1_TX | J4.14, R43.1 |
| SEED_D14_USART1_RX | J4.15, U11.4, R41.2 |
| SEED_D15_EXP_ADC | J4.22, R52.2, C50.1 |
| SEED_D16_DISPLAY_BL_PWM | J4.23, J20.10, J21.10, R60.1 |
| SEED_D17_DISPLAY_DC | J4.24, J20.8, J21.8, J22.6 |
| SEED_D18_DISPLAY_RST | J4.25, J20.9, J21.9, J22.5 |
| SEED_D19_MCP_INT_A | J4.26, J20.13, J21.13, U20.20 |
| SEED_D1_SDMMC1_D3 | J4.2, J13.2, R58.2 |
| SEED_D20_VMON_ADC | R21.2, J4.27 |
| SEED_D21_MCP_INT_B | J4.28, J20.14, J21.14, U21.20 |
| SEED_D22_FS1 | J4.29, J20.15, J21.15, SW10.1, R90.2, C90.1 |
| SEED_D23_FS2 | J4.30, J20.16, J21.16, SW11.1, R91.2, C91.1 |
| SEED_D24_FS3 | J4.31, J20.17, J21.17, SW12.1, R92.2, C92.1 |
| SEED_D25_LED1 | J4.32, J20.18, J21.18, R93.1 |
| SEED_D26_LED2 | J4.33, J20.19, J21.19, R94.1 |
| SEED_D27_LED3 | J4.34, J20.20, J21.20, R95.1 |
| SEED_D2_SDMMC1_D2 | J4.3, J13.1, R57.2 |
| SEED_D3_SDMMC1_D1 | J4.4, J13.8, R56.2 |
| SEED_D4_SDMMC1_D0 | J4.5, J13.7, R55.2 |
| SEED_D5_SDMMC1_CMD | J4.6, J13.3, R54.2 |
| SEED_D6_SDMMC1_CK | J4.7, R53.1 |
| SEED_D7_SPI1_NSS | J4.8, J20.7, J21.7, J22.7 |
| SEED_D8_SPI1_SCK | J4.9, J20.5, J21.5, J22.3 |
| SEED_VIN | R32.2, C11.1, C12.1, J4.39 |
| U1B_OUT | U1.7, R7.1, R9.1 |
| U20_RESET | U20.18, R98.2 |
| U21_RESET | U21.18, R99.2 |
| U3A_OUT | U3.1, R72.1, R70.1 |
| U3B_OUT | U3.6, U3.7 |
| V9 | Q2.2, C7.1, U2.8, C16.1, D1.2, R32.1, R19.1, K1.A1, D4.1, J20.1, PF2.1, J21.1, FB3.1, C36.1 |
| VBIAS | R11.2, R12.1, C6.1, R3.2, R6.2, R71.2, U3.5 |
| VG | Q1.1, Q2.1, R13.1, Q3.3, D14.2 |
| VIN_JACK | J3.2, FB1.1 |
| VIN_RAW | FB1.2, Q1.2, PF1.1 |
| VMON | R19.2, R20.1, C14.1, R21.1 |
| VRAIL | R16.2, C10.1, C17.1, R11.1, U1.8, U3.8, PF3.1 |
| VRAIL_RAW | D2.1, C9.1, R16.1 |
| VS | Q1.3, Q2.3, Q3.1, R14.1, D14.1 |
| ZK | R15.2, D3.1 |
