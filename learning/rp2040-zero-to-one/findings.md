# RP2040 0 to 1 확인 기록

> `notes.md`의 판단과 따로, 칩·보드를 장비로 확인한 사실과 그 도구를 모은 기록입니다. 판단 순서와 무관하게 앞서 나갑니다.

| 항목 | 결과 | 확인 방법 |
| --- | --- | --- |
| 칩 리비전 | B2 | BOOTSEL 모드에서 `picotool info -a` |
| flash 칩 | Puya P25Q16H 계열 2MB. 회로도 v1.3의 GD25Q16CEIG와 다름 | JEDEC ID 읽기 |
| 공장 boot2 | pico-sdk `boot2_w25q080.S`, `PICO_FLASH_SPI_CLKDIV=2` | 바이트 비교 |
| flash 부팅 이미지 | `lab/flash-boot/` | `make check` |
| clk_sys | PLL_SYS 125MHz | FC0 주파수 카운터, PC 시계로 1초 간격 |
| 인터럽트 | TIMER 알람 IRQ 0, 0.5초 간격 | UART 줄 간격 |
| core1 | FIFO 순서로 기동, CPUID 1, 0.25초 간격 LED | UART 줄 간격 |
| NeoPixel | GPIO12 데이터, GPIO11 전원, PIO0 800kHz | 눈으로 색 확인 |
| USB CDC | 1209:0001로 열거, 에코 64 KiB 일치, 1200 baud로 BOOTSEL | `echo_test.py`, `lsusb` |
| UART로 BOOTSEL | "BOOTSEL" 수신 시 bootrom으로 재부팅 | 재부팅 뒤 2e8a:0003 열거 |
| ADC 온도 | 38.8도 안팎, ERR 0. 정확도 미판정 | UART |
| PWM | 1kHz wrap 1000회/초, duty 기대값과 ±6/1000, 색 전환 | INTR 횟수, 핀 입력 샘플, 눈으로 확인 |
| DMA | 4 KiB 복사 10 us, sniffer CRC가 boot2 CRC와 일치, UART TX | UART |
| watchdog | REASON timer/force 구분, scratch 유지, 타임아웃 LOAD / 2 us | UART 줄 간격 |
| flash 기록 | 마지막 섹터에 부팅 순번 누적, 쓰기 1.74 ms | UART, `picotool save` |
| spinlock | lock 없이 7.8% 유실(지연을 흩뜨린 경우), lock으로 0 | UART |
| RTC | 날짜·요일 넘김, 초 간격 1000000 us | TIMER |
| ROSC 난수 | 원래 비트는 1이 46%, von Neumann 보정 뒤 50% | 10만 비트 통계 |
| interpolator | 데이터시트 예제 세 가지의 출력과 일치 | UART |
| PIO UART | 115200 에코 일치, framing 오류 검출 | 호스트 시험 스크립트 |
| DMA로 PWM | chain으로 밝기 표 반복, CPU는 wfi | 표 위치 읽기 |
| ADC + DMA | 500000 sps, 1000 sps | TIMER |
| HardFault | 네 가지 오류 모두 원인 명령의 pc 보고 | `addr2line` |
| SLEEP·DORMANT | SLEEP_EN으로 PWM 클럭 멈춤, DORMANT 3초·5초 모두 TIMER 36 us | PWM CTR, TIMER |
| 문맥 전환 | SysTick·PendSV로 두 작업 500 / 500 | SysTick 횟수, 지역 변수 주소 |
| flash SFDP | 2MB, 1-4-4 읽기 0xeb·dummy 4. 공장 boot2 설정과 같음 | 직접 명령 |
| GPIO 인터럽트 | 엣지 래치, 진입 29 클럭, 같은 우선순위끼리는 끼어들지 않음 | SysTick, 핸들러 순서 기록 |
| watchdog 부팅 | SCRATCH4~7로 SRAM 함수 진입, bootrom이 SCRATCH4를 지움 | scratch에 남긴 값 |
| SRAM·XIP 속도 | 캐시 hit면 SRAM과 같음, 캐시를 끄면 27배, miss는 워드 단위 | SysTick, XIP 카운터 |

## UART


LED 깜빡임으로는 바이트 값을 읽기 어려워 UART를 붙입니다.

| XIAO 핀 (실크) | 칩 핀 | Debug Probe "U" 포트 선 |
| --- | --- | --- |
| D6 (J2 7번) | GPIO0, UART0 TX | 노랑 (프로브 RX) |
| D7 (J4 1번) | GPIO1, UART0 RX | 주황 (프로브 TX) |
| GND (J4 6번) | GND | 검정 |

프로브는 PC에서 `/dev/ttyACM0`으로 보이고, 보드 전원은 XIAO의 USB-C가 줍니다.

부팅 직후 clk_sys는 ROSC라 주파수가 부정확합니다. UART 클럭(clk_peri)을 12MHz XOSC에 직접 연결해 PLL 없이 보드레이트를 만듭니다.

| 순서 | 레지스터 | 값 |
| --- | --- | --- |
| 1 | XOSC_CTRL (0x40024000) | ENABLE 0xfab << 12, FREQ_RANGE 0xaa0. STATUS bit 31 STABLE 대기 |
| 2 | CLK_PERI_CTRL (0x40008048) | 끈 뒤 AUXSRC = 4(XOSC), 그다음 ENABLE(bit 11). AUXSRC는 바꿀 때 글리치가 남 |
| 3 | RESETS | UART0(bit 22) 리셋 해제 |
| 4 | UARTIBRD, UARTFBRD (0x40034024, 028) | 6, 33. 12MHz ÷ (16 × 115200) = 6.5104 |
| 5 | UARTLCR_H (0x4003402c) | 8비트, FIFO 켬. IBRD·FBRD 뒤에 씀. SDK `uart.c` 주석: PL011은 LCR_H를 써야 분주값이 반영됨(데이터시트에는 없음) |
| 6 | UARTCR (0x40034030) | UARTEN, TXE, RXE |
| 7 | GPIO0·1 CTRL | FUNCSEL = 2(UART) |

송신은 UARTFR의 TXFF(bit 5)가 0일 때 UARTDR에 씁니다.

Cortex-M0+에는 나눗셈 명령이 없습니다. `%`나 `/`를 쓰면 gcc가 `__aeabi_uidivmod`를 부르고, `-nostdlib`라 링크가 실패합니다.

## flash 칩

flash에 명령 0x9F를 보내면 JEDEC ID 3바이트(제조사, 메모리 종류, 용량)가 돌아옵니다.

1. bootrom 함수 표(2.8.3)에서 `_connect_internal_flash`('I','F')와 `_flash_exit_xip`('E','X')를 찾아 호출. 0x14에 함수 표, 0x18에 조회 함수의 16비트 주소
2. IO_QSPI GPIO_QSPI_SS_CTRL(0x4001800c)의 OUTOVER(bit 9:8)를 LOW로 해 CS를 내림
3. SSI(0x18000000)의 DR0(0x60)로 `9F 00 00 00`을 보내며 4바이트를 받음. SR(0x28)의 TFNF(bit 1), RFNE(bit 3)로 FIFO 확인
4. OUTOVER를 HIGH로 해 CS를 올림

송수신 루프(동시에 보내 둔 바이트 14개 제한 포함)는 SDK `hardware_flash`의 `flash_do_cmd_cs`와 같습니다. SDK는 앞뒤로 boot2 복사본을 준비하고 XIP를 다시 켜는 단계를 더 하고, 이 코드는 SRAM에서 실행되어 그 단계를 뺐습니다. 코드는 `tools/jedec-id/main.c`이고 `make APP=tools/jedec-id`로 빌드합니다.

| 자료 | flash 칩 |
| --- | --- |
| 회로도 v1.3 U3 | GD25Q16CEIG (GigaDevice). 제조사 코드 0xC8은 로컬 자료로 확인하지 않음 |
| 칩이 보낸 JEDEC ID | `85 60 15`. 제조사 0x85 Puya, 0x60 P25Q 계열, 0x15 = 2MB(2^21). 세 바이트의 해석은 공개 자료 기준이고 Puya 데이터시트로는 확인하지 않음 |

0x85 6015를 P25Q16H로 대응시킨 근거는 Xradio의 flash 지원 목록과 Puya 칩을 읽은 공개 기록입니다.

boot2는 flash 칩에 맞춰 XIP를 설정하므로, SDK 기본 boot2(`boot2_w25q080.S`)가 이 칩에 맞는다는 보장은 없습니다. 공장 펌웨어가 이 보드에서 flash 부팅되므로, 공장 백업의 앞 256바이트는 이 칩에서 동작하는 boot2입니다.

## 공장 boot2

공장 백업의 앞 256바이트를 pico-sdk 2.3.0의 RP2040 boot2 변종 6개와 비교했습니다. 마지막 4바이트 CRC32(0x7a4eb274)는 앞 252바이트로 계산한 값과 같습니다.

| 변종 | 공장 boot2와 다른 바이트 |
| --- | --- |
| `boot2_w25q080.S` (기본값) | 240바이트 중 1개. 0x1c의 `movs r1, #PICO_FLASH_SPI_CLKDIV` 상수가 4 대신 2 |
| `boot2_generic_03h.S` | 76바이트 중 70개 |
| `boot2_at25sf128a.S` | 236바이트 중 146개 |
| `boot2_is25lp080.S` | 216바이트 중 199개 |
| `boot2_w25x10cl.S` | 112바이트 중 106개 |
| `boot2_usb_blinky.S` | 36바이트 중 35개 |

`boot2_w25q080.S`를 `PICO_FLASH_SPI_CLKDIV=2`로 빌드하고 `pad_checksum`으로 채운 256바이트는 공장 boot2와 같습니다. SDK 보드 헤더 `seeed_xiao_rp2040.h`도 `PICO_BOOT_STAGE2_CHOOSE_W25Q080`과 `PICO_FLASH_SPI_CLKDIV 2`를 지정합니다. 공장 펌웨어는 Winbond용 boot2를 SPI 클럭 clk_sys/2로 돌리고, 이 Puya 칩에서 그대로 flash 부팅됩니다.

## boot2 다음에 실행되는 곳

bootrom은 flash 앞 256바이트를 SRAM5로 복사하고 CRC32가 맞으면 그 코드를 실행합니다(데이터시트 2.8.1). SDK의 boot2 링크 주소는 0x20041f00입니다(`boot_stage2.ld`).

boot2가 XIP 설정을 마친 뒤의 동작은 데이터시트가 아니라 SDK 소스 `boot2_helpers/exit_from_boot2.S`에 있습니다.

| 진입 시 lr | 동작 |
| --- | --- |
| 0 (bootrom이 호출) | VTOR = 0x10000100, 그 주소의 첫 워드를 MSP, 둘째 워드로 `bx` |
| 0이 아님 (사용자 코드가 호출) | 호출한 곳으로 복귀 |

공장 덤프의 0x10000100은 SP 0x20042000, reset 0x100001e3입니다. 0x10000100은 하드웨어가 정한 주소가 아니라 SDK boot2의 약속입니다. VTOR은 bit 31:8만 쓰므로 표는 256바이트 정렬이어야 하고, 0x10000100은 이를 만족합니다.

## flash 부팅 이미지

`lab/flash-boot/`는 `src/`, `ld/`와 따로 빌드합니다.

| 파일 | 내용 |
| --- | --- |
| `boot2.S` | 공장 boot2와 같은 256바이트. SDK 라이선스는 `LICENSE-pico-sdk.txt` |
| `flash.ld` | 0x10000000 boot2, 0x10000100 벡터 테이블, 이어서 .text, .rodata, .data 초기값 |
| `start.S` | 벡터 테이블 48칸과 reset. .data 복사, .bss 0 채움, main 호출 |
| `main.c` | UART hello. .data 문자열과 .bss 변수로 시작 코드 동작 확인 |
| `check_flash_uf2.py` | uf2 블록, boot2 CRC, 0x100 벡터, .data 초기값 포함 여부 |

SRAM 빌드와 다른 점은 .data입니다. 실행 주소(VMA)는 SRAM, 초기값의 저장 주소(LMA)는 flash라 `> SRAM AT > FLASH`로 두 주소를 나누고, 시작 코드가 `LOADADDR(.data)`에서 SRAM으로 복사합니다. SP는 boot2가 벡터 테이블에서 이미 설정하므로 시작 코드에서 다시 쓰지 않습니다.

.rodata 끝을 4바이트로 맞추지 않으면 .data의 LMA가 문자열 길이만큼 어긋납니다. 첫 빌드에서 LMA가 4의 배수가 아니어서 `flash.ld`의 ASSERT가 링크를 멈췄습니다. 데이터시트 CCR(UNALIGN_TRP)에 따르면 비정렬 접근은 모두 HardFault라, 그대로 올렸다면 복사 루프의 첫 `ldr`에서 멈췄을 것입니다. 보드에서 돌려 확인하지는 않았습니다.

`.word _reset`처럼 `.thumb_func` 심볼을 가리키는 워드에는 링커가 bit 0(Thumb)을 넣습니다. 빌드 결과 벡터는 reset 0x10000111, hang 0x10000139입니다.

`make check` 결과:

| 항목 | 값 |
| --- | --- |
| boot2 CRC | 0x7a4eb274. 앞 256바이트가 공장 boot2와 같음 |
| 0x10000100 벡터 | SP 0x20042000, reset 0x10000111 |
| 이미지 | 768바이트. .data 초기값 "flash boot "가 0x100002ec |

검사기는 boot2 1바이트 변조와 이미지 끝 부족을 각각 FAIL로 보고합니다.

## 보드에서 flash 부팅

BOOTSEL 모드에서 `make load`로 flash에 쓴 결과입니다. 공장 펌웨어를 덮어썼고, 복구는 비공개 저장소의 `backups/xiao-rp2040/factory-flash.bin`으로 합니다.

| 시험 | 결과 |
| --- | --- |
| `picotool load -x` 직후 UART | `bss ok`, 이어서 `flash boot 0`~`9` 반복 |
| USB-C를 뽑았다 버튼 없이 다시 꽂음 | 빨강 LED 깜빡임, UART에 `flash boot N` 반복 |

전원을 다시 넣었을 때 bootrom → boot2 → 0x10000100 벡터 → `_reset` 순서로 실행되고, .data 문자열이 SRAM에 복사됩니다.

## clk_sys 125MHz

`lab/flash-boot/clock.c`는 clk_sys를 PLL_SYS로 옮기고 주파수 카운터 FC0(2.15.6.2)로 각 클럭을 잽니다. `make MAIN=clock.c`로 빌드합니다.

| 순서 | 동작 | 근거 |
| --- | --- | --- |
| 1 | XOSC 켜고 clk_peri = XOSC, UART 초기화 | UART 분주값 6, 33 유지 |
| 2 | clk_ref SRC = XOSC, CLK_REF_SELECTED 대기 | FC0와 watchdog tick의 기준이 clk_ref |
| 3 | PLL_SYS 리셋 후 REFDIV 1, FBDIV 125, PD·VCOPD 끔, LOCK 대기, POSTDIV 6·2, POSTDIVPD 끔 | 2.18.3. SDK `vcocalc.py 125` 결과와 같음 |
| 4 | clk_sys SRC = clk_ref 확인 후 AUXSRC = PLL_SYS, 그다음 SRC = AUX, CLK_SYS_SELECTED 대기 | AUXSRC는 바꿀 때 글리치 |
| 5 | WATCHDOG TICK = ENABLE, CYCLES 12. TIMER 리셋 해제 | 4.7.2. tick은 clk_ref 12분주 = 1us |

clk_sys의 SRC 기본값이 clk_ref라, clk_ref를 XOSC로 바꾸는 순간 clk_sys도 ROSC에서 12MHz로 바뀝니다.

보드에서 잰 값:

| 클럭 | PLL 전 | PLL 후 |
| --- | --- | --- |
| ROSC | 5541 kHz | |
| XOSC | 12000 kHz | |
| clk_sys | 12000 kHz | 125000 kHz |
| pll_sys | | 125000 kHz |
| clk_ref | | 12001 kHz |
| clk_peri | | 12001 kHz |

TIMER로 1초마다 출력하는 `t=N s` 줄의 간격은 PC 시계로 0.999~1.001초입니다. XIP는 boot2 설정대로 clk_sys/2 = 62.5MHz로 돌고, 이 칩에서 문제없이 실행됩니다.

## 인터럽트

`lab/flash-boot/irq.c`는 TIMER 알람 0(IRQ 0, 데이터시트 2.3.2 Table 80)으로 0.5초마다 LED를 뒤집습니다. main은 `wfi`로 잠들었다가 깨어날 때마다 횟수를 UART로 찍습니다. `make MAIN=irq.c`로 빌드합니다.

벡터 테이블은 시스템 예외 16칸과 IRQ 32칸으로 192바이트입니다. VTOR은 bit 31:8만 쓰므로 0x10000100(256바이트 정렬)에 둡니다. `start.S`는 핸들러 이름을 모두 `.weak`와 `.thumb_set name, hang`으로 선언하고, C에서 같은 이름의 함수(`isr_irq0`)를 정의하면 링커가 그 주소를 테이블에 넣습니다. 정의하지 않은 칸은 hang을 가리켜, 예상하지 못한 인터럽트가 오면 그 자리에서 멈춥니다. Cortex-M0+는 예외 진입 때 r0~r3, r12, lr, pc, xPSR을 하드웨어로 스택에 쌓으므로 핸들러를 평범한 C 함수로 쓸 수 있습니다. 이 동작은 ARMv6-M 아키텍처 규칙이고 RP2040 데이터시트에는 없습니다.

| 순서 | 레지스터 | 값 |
| --- | --- | --- |
| 1 | TIMER INTE (0x40054038) | ALARM_0(bit 0) |
| 2 | NVIC_ICPR (0xe000e280), NVIC_ISER (0xe000e100) | bit 0. 대기 중인 인터럽트를 지우고 켬 |
| 3 | TIMER ALARM0 (0x40054010) | 현재 TIMERAWL + 500000. 쓰면 알람이 걸림 |
| 핸들러 | TIMER INTR (0x40054034) | bit 0에 1을 써서 지움(WC). ALARM0를 다음 시각으로 다시 씀 |

알람은 한 번 울리면 스스로 풀리므로 핸들러에서 매번 다시 겁니다. 다음 시각을 TIMERAWL이 아니라 이전 목표 시각에 더해, 핸들러 진입 지연이 누적되지 않게 합니다.

보드 결과:

| 항목 | 값 |
| --- | --- |
| VTOR | 0x10000100 |
| 테이블의 irq0 칸 | 0x100003b1 (`isr_irq0`, Thumb 비트 포함) |
| `irq N` 줄 간격 (PC 시계) | 0.498~0.503초, 11회 |

## core1

리셋 뒤 core1은 bootrom 안에서 잠들어 있다가 core0이 FIFO로 보낸 순서를 받으면 깨어납니다(데이터시트 2.8.2). `lab/flash-boot/core1.c`는 core1에 LED를 맡기고, core0은 core1이 FIFO로 보낸 값을 UART로 찍습니다. `make MAIN=core1.c`로 빌드합니다.

core0이 보내는 값은 `0, 0, 1, VTOR, SP, entry`입니다. core1의 bootrom은 받은 값을 그대로 돌려보내고, core0은 돌아온 값이 보낸 값과 다르면 처음부터 다시 보냅니다. 0을 보내기 전에는 core0의 RX FIFO를 비우고 `sev`로 core1을 깨웁니다. entry는 Thumb 비트가 붙은 함수 주소이고, C의 함수 포인터 값이 이미 홀수입니다.

| 레지스터 | 주소 | 용도 |
| --- | --- | --- |
| SIO CPUID | 0xd0000000 | 읽는 코어 번호. core0은 0, core1은 1 |
| SIO FIFO_ST | 0xd0000050 | VLD(bit 0) 받을 값 있음, RDY(bit 1) 보낼 자리 있음 |
| SIO FIFO_WR | 0xd0000054 | 상대 코어로 보냄. 쓴 뒤 `sev` |
| SIO FIFO_RD | 0xd0000058 | 상대 코어가 보낸 값. 비어 있으면 `wfe`로 대기 |

core1의 스택은 `.bss`의 1KB 배열이고, 끝 주소를 8바이트 정렬로 둡니다. 8바이트는 ARM 호출 규약(AAPCS)이 함수 경계에서 요구하는 정렬이고, SDK `multicore_launch_core1_with_stack`은 4바이트 배수만 검사합니다. 벡터 테이블은 core0과 같은 0x10000100을 씁니다. UART는 core0만 써서 spinlock 없이 동작합니다.

보드 결과:

| 항목 | 값 |
| --- | --- |
| core0 CPUID | 0 |
| core1 CPUID (core1이 FIFO로 보냄) | 1 |
| core1 SP, entry | 0x20000400, 0x10000445 |
| `core1 toggles N` 줄 간격 (PC 시계) | 0.249~0.255초, 12회 |

## NeoPixel (PIO)

회로도 v1.3의 RGB1은 DIN이 GPIO12(NEOPIXEL), VCC가 GPIO11(NEO_PWR)에 직접 연결됩니다. LED 전원을 GPIO가 주므로 GPIO11을 high로 두어야 켜집니다. 패드 구동 세기 설정의 최댓값이 12mA라 색 값을 16/255로 낮게 둡니다. 16은 LED 전류를 계산해 정한 값이 아니라 보수적으로 고른 값입니다. GPIO11 패드는 DRIVE = 12mA(2.19.6.3)로 설정합니다.

`lab/flash-boot/neopixel.c`는 PIO0 SM0으로 WS2812 신호를 만듭니다. `make MAIN=neopixel.c`로 빌드합니다. PIO 프로그램은 pico-sdk `pico_status_led/ws2812.pio`(T1 3, T2 3, T3 4)를 데이터시트 3.4 Table 366의 형식으로 직접 인코딩했습니다. side-set 1비트가 Delay/side-set 필드의 bit 12, delay가 bit 11:8입니다.

| 주소 | 명령 | 인코딩 |
| --- | --- | --- |
| 0 | `out x, 1 side 0 [3]` | 0x6321 |
| 1 | `jmp !x 3 side 1 [2]` | 0x1223 |
| 2 | `jmp 0 side 1 [2]` | 0x1200 |
| 3 | `nop side 0 [2]` (`mov y, y`) | 0xa242 |

한 비트는 10사이클(8MHz에서 1.25us)입니다. `out`이 low 4사이클, `jmp !x`가 high 3사이클이고, 비트가 1이면 `jmp`가 high 3사이클을 더해 high 6·low 4(750ns·500ns), 0이면 `nop`이 low 3사이클을 더해 high 3·low 7(375ns·875ns)입니다.

| 레지스터 | 값 |
| --- | --- |
| SM0_CLKDIV | INT 15, FRAC 160. 125MHz / 15.625 = 8MHz, 비트당 10사이클 = 800kHz |
| SM0_EXECCTRL | WRAP_TOP 3, WRAP_BOTTOM 0, SIDE_EN 0 |
| SM0_SHIFTCTRL | FJOIN_TX, PULL_THRESH 24, OUT_SHIFTDIR 0(왼쪽), AUTOPULL |
| SM0_PINCTRL | SIDESET_COUNT 1, SIDESET_BASE 12, SET_COUNT 1, SET_BASE 12 |
| SM0_INSTR | `set pindirs, 1`(0xe081)로 GPIO12를 출력으로, `jmp 0`(0x0000) |
| GPIO12 CTRL | FUNCSEL 6(PIO0) |

TXF0에는 `(G << 24) | (R << 16) | (B << 8)`을 씁니다. 왼쪽 시프트라 OSR의 bit 31부터 나갑니다. 초록, 빨강, 파랑 순서는 SDK 예제와 같고, WS2812 데이터시트로는 확인하지 않았으며 보드에서 색이 맞게 나오는 것으로 확인했습니다.

보드 결과:

| 항목 | 결과 |
| --- | --- |
| FDEBUG TXSTALL(bit 24) | 매 색마다 1. SM이 24비트를 내보낸 뒤 빈 FIFO에서 멈춤 |
| NeoPixel 색 (눈으로 확인) | 1초마다 빨강, 초록, 파랑, 흰색, 꺼짐 순서. 빨강과 초록이 바뀌지 않음 |

## USB CDC

`lab/flash-boot/usb_cdc.c`는 USB CDC ACM 장치입니다. 받은 바이트를 그대로 돌려보냅니다. 데이터시트 4.1과 그 안에 실린 pico-examples `dev_lowlevel` 발췌를 따르고, 인터럽트 대신 main 루프에서 USB INTS를 확인합니다. `make MAIN=usb_cdc.c`로 빌드합니다.

| 항목 | 값 |
| --- | --- |
| clk_usb | PLL_USB 12MHz × 100 / 5 / 5 = 48MHz, CLK_USB_CTRL AUXSRC 0 |
| clk_sys | 125MHz. RP2040-E16에 따라 USB를 쓰려면 48MHz보다 빨라야 함 (4.1.2.1) |
| VID:PID | 1209:0001 (pid.codes 시험용 번호) |
| 인터페이스 | 0: CDC 통신(ACM), EP1 IN interrupt. 1: CDC 데이터, EP2 OUT·IN bulk 64바이트 |
| DPRAM (0x50100000) | 0x00 SETUP 8바이트, EP 제어 0x08~, 버퍼 제어 0x80~, EP0 버퍼 0x100, EP1 IN 0x180, EP2 OUT 0x1c0, EP2 IN 0x200 |

초기화 순서는 USBCTRL 리셋 해제, DPRAM 0 채움, USB_MUXING = TO_PHY | SOFTCON, USB_PWR = VBUS_DETECT | VBUS_DETECT_OVERRIDE_EN, MAIN_CTRL = CONTROLLER_EN, SIE_CTRL = EP0_INT_1BUF, 엔드포인트 제어 기록, 마지막으로 SIE_CTRL PULLUP_EN입니다. D+ 풀업을 켜는 순간 호스트가 장치 연결을 감지합니다(4.1.2.10). VBUS 감지 신호는 GPIO에 연결하지 않고 USB_PWR로 강제로 켭니다. 보드에 VBUS 감지 배선이 있는지는 회로도로 확인하지 않았습니다.

버퍼 제어 레지스터는 clk_sys와 clk_usb가 다르면 AVAILABLE 비트를 나머지보다 늦게 써야 합니다(4.1.2.7.4). 값을 AVAILABLE 없이 한 번 쓰고 빈 루프 4회(125MHz에서 약 190ns로 추정)를 기다린 뒤 AVAILABLE을 포함해 다시 씁니다. 데이터시트에는 대기 길이가 없고, 4회는 시험에서 동작한 값입니다.

제어 전송에서 처리하는 요청:

| 요청 | 처리 |
| --- | --- |
| GET_DESCRIPTOR 장치·설정·문자열 | EP0 IN으로 64바이트씩. 설정 디스크립터 67바이트는 64 + 3 |
| GET_DESCRIPTOR device qualifier (0x06) | STALL. EP_STALL_ARM과 버퍼 제어 STALL을 함께 세움(4.1.2.8.1). full speed 전용 장치가 STALL로 답하는 것은 USB 2.0 규격 기억에 따른 것이고, 호스트는 3회 다시 묻고 넘어감 |
| SET_ADDRESS | 길이 0 IN으로 상태 단계를 끝낸 뒤 ADDR_ENDP를 바꿈 |
| SET_CONFIGURATION | EP2 데이터 PID를 DATA0으로 되돌리고 EP2 OUT을 받을 준비 |
| SET_LINE_CODING (0x21 0x20) | EP0 OUT으로 7바이트를 받고 길이 0 IN으로 상태 단계 |
| SET_CONTROL_LINE_STATE 등 데이터 없는 OUT 요청 | 길이 0 IN |

SETUP 뒤의 데이터·상태 단계는 DATA1부터 시작합니다. IN 데이터 단계가 끝나면 호스트의 길이 0 OUT 상태 패킷을 받도록 EP0 OUT을 엽니다.

연결 직후 UART 로그에 찍힌 요청 순서입니다. 장치 디스크립터 64바이트 요청, 버스 리셋, SET_ADDRESS, 장치 디스크립터 18바이트, device qualifier 3회(STALL), 설정 디스크립터 9바이트와 67바이트, 문자열 0·2·1·3, SET_CONFIGURATION, SET_LINE_CODING 순서로 진행합니다.

### 64바이트 패킷과 길이 0 패킷

첫 버전에서는 64바이트를 보내면 돌려받지 못했습니다. 그 데이터는 다음 시험 데이터 앞에 붙어서 늦게 나왔습니다. 64바이트를 보낸 뒤 길이 0 패킷을 한 번 더 보내고 나서 다음 OUT을 받도록 고치자 64바이트도 바로 돌아왔습니다. 원인은 bulk 전송이 최대 크기보다 짧은 패킷으로 끝을 알린다는 USB 규칙과, 호스트 읽기 요청이 64바이트보다 크게 걸려 있어 다음 패킷을 기다렸기 때문으로 봅니다. USB 규격과 Linux cdc_acm 소스로는 확인하지 않았습니다.

`lab/flash-boot/echo_test.py`의 결과입니다(호스트 Linux, 포트 `/dev/serial/by-id/usb-tolelom_RP2040_0to1_CDC_0001-if00`).

| 길이 | 결과 | 시간 |
| --- | --- | --- |
| 5, 63, 64, 65, 128바이트 | 일치 | 각 0.6 ms |
| 1024바이트 | 일치 | 4.5 ms |
| 64 KiB 무작위 | 일치 | 261.7 ms, 245 KiB/s |

full speed(12Mbps) 한계보다 느린 이유는 패킷 하나를 돌려보낸 뒤에야 다음을 받는 구조 때문으로 추정하고, 재 보지는 않았습니다.

### 버튼 없이 BOOTSEL로

호스트가 CDC 포트를 1200 baud로 설정하면(SET_LINE_CODING) 상태 단계를 끝낸 뒤 bootrom `_reset_to_usb_boot`('U','B', 2.8.3)를 `(0, 0)`으로 부릅니다. watchdog으로 리셋되어 BOOTSEL 모드로 다시 뜹니다. `make bootsel`이 `stty -F <포트> 1200`을 실행하고, 그 뒤 `make load`로 올립니다. usb_cdc.c가 돌고 있을 때만 쓸 수 있습니다.

## 공통 코드 (board.c)

`lab/flash-boot/board.c`와 `board.h`는 아래 절의 프로그램이 함께 쓰는 초기화와 출력 함수입니다. 소스에 `#include "board.h"`가 있으면 Makefile이 board.c를 같이 링크합니다. 그 전에 만든 main.c, clock.c, irq.c, core1.c, neopixel.c, usb_cdc.c는 각자 함수를 갖고 있어 board.c를 쓰지 않습니다.

| 함수 | 내용 |
| --- | --- |
| `board_init` | IO·PADS 리셋 해제, RGB2 세 색을 SIO high로 꺼 둠, XOSC, PLL_SYS 125MHz, clk_peri = XOSC, watchdog tick 1us, TIMER, UART0 115200 |
| `clocks_48mhz_init` | PLL_USB 48MHz, clk_usb와 clk_adc를 PLL_USB로 |
| `uart_flush` | UARTFR BUSY(bit 3)가 0이 될 때까지 대기 |
| `isr_irq20` | UART0 수신 인터럽트. "BOOTSEL"이 연속으로 들어오면 bootrom `_reset_to_usb_boot` 호출 |
| `__aeabi_uidivmod` 등 4개 | SIO 하드웨어 나눗셈기(2.3.1.5)로 `/`, `%` 처리 |

처음 board_init은 LED 핀을 건드리지 않았습니다. dma_pwm.c를 돌리는 동안 쓰지 않는 파랑(GPIO25)이 켜져 보여, 임시 프로그램으로 GPIO25를 세 상태로 4초씩 바꿔 보았습니다. 리셋 직후 값은 PADS 0x56(IE, DRIVE 4mA, PDE, SCHMITT), GPIO CTRL FUNCSEL 0x1f(연결 없음)입니다.

| GPIO25 상태 | 파랑 (눈으로 확인) |
| --- | --- |
| 구동 없음, pull-down (리셋 상태) | 켜짐 |
| 구동 없음, pull 없음 | 판정 안 함 |
| SIO high 구동 | 꺼짐 |

구동하지 않는 핀의 pull-down이 active low LED에 전류를 흘린 것으로 봅니다. pull 없음 상태가 high 구동과 같은지는 눈으로 구분하지 않았습니다. 세 색을 board_init에서 high로 구동하도록 바꾼 뒤 dma_pwm.c에서 파랑이 꺼진 것을 확인했습니다.

UART로 "BOOTSEL"을 보내 재부팅하는 방식은 USB CDC가 없는 프로그램에서도 버튼 없이 다시 올리기 위한 것입니다. Debug Probe의 UART 포트에 문자열을 쓰면 BOOTSEL 모드로 다시 뜹니다.

gcc는 M0+에서 `/`와 `%`를 `__aeabi_uidiv` 같은 함수 호출로 바꿉니다. `-nostdlib`라 libgcc의 구현이 없으므로 board.c가 같은 이름으로 정의합니다. 피제수와 제수를 쓰고 CSR READY(bit 0)를 기다린 뒤 나머지, 몫 순서로 읽습니다. 몫을 마지막에 읽는 순서는 SDK를 따른 것입니다.

`uart_flush`는 처음에 TXFE(FIFO 비었음)를 기다렸습니다. watchdog 시험에서 리셋 직전에 보낸 줄의 `\r\n`이 사라졌고, BUSY를 기다리도록 바꾼 뒤 사라지지 않았습니다. 데이터시트 4.2의 UARTFR 설명대로 BUSY는 마지막 바이트의 정지 비트까지 시프트 레지스터에서 나가야 0이 됩니다.

## ADC 온도 센서

`lab/flash-boot/adc_temp.c`는 ADC 채널 4(칩 내장 온도 센서)를 64회 읽어 평균하고 1초마다 온도로 바꿔 보냅니다. `make MAIN=adc_temp.c`로 빌드합니다.

| 항목 | 값 |
| --- | --- |
| clk_adc | PLL_USB 48MHz. ADC는 48MHz를 요구함 (4.9.2) |
| ADC CS (0x4004c000) | EN(bit 0), TS_EN(bit 1), START_ONCE(bit 2), READY(bit 8) 대기, ERR(bit 9), AINSEL(bit 14:12) = 4 |
| 전압 환산 | 결과 × 3.3V / 4096. ADC_AVDD를 3.3V로 가정 |
| 온도 환산 | T = 27 - (V - 0.706) / 0.001721 (4.9.5) |

보드 결과는 raw 850 안팎, 0.6856V, 38.8도 안팎이고 ERR는 0입니다. 기준 온도계로 비교하지 않았고 센서를 보정하지 않았으며 ADC_AVDD를 재지 않았으므로 정확도는 판정하지 않습니다.

## PWM (RGB2 밝기)

`lab/flash-boot/pwm.c`는 RGB2 세 색을 PWM으로 밝기 조절합니다. `make MAIN=pwm.c`로 빌드합니다.

| LED | GPIO | PWM (4.5.2 Table 515) |
| --- | --- | --- |
| 초록 | 16 | 슬라이스 0 A |
| 빨강 | 17 | 슬라이스 0 B |
| 파랑 | 25 | 슬라이스 4 B |

출력은 카운터가 CC보다 작을 때 high이고 주기는 TOP + 1 카운트입니다(4.5.2.1). LED가 active low라 CSR의 A_INV·B_INV로 출력을 뒤집어 CC가 클수록 밝게 합니다. 슬라이스 레지스터는 0x40050000 + 슬라이스 × 0x14에서 CSR, DIV, CTR, CC, TOP 순서입니다.

먼저 DIV INT 125(1MHz 카운트), TOP 999(1kHz), 초록 CC 250, 빨강 CC 750으로 두고 1초 동안 INTR의 wrap 플래그를 세고, SIO GPIO_IN으로 핀을 20만 번 읽어 high 비율을 셉니다. 출력이 뒤집혀 있으므로 핀 high 비율의 기대값은 초록 750/1000, 빨강 250/1000입니다. 이어서 DIV 1, TOP 4095(약 30.5kHz)로 빨강, 초록, 파랑 순서로 2초씩 밝아졌다 어두워집니다.

| 항목 | 결과 |
| --- | --- |
| wrap 횟수 | 1000회/초 |
| GPIO16 high 비율 | 746/1000 |
| GPIO17 high 비율 | 244/1000 |
| 숨쉬기 패턴 (눈으로 확인) | 빨강, 초록, 파랑 순서로 전환. 깜빡임 안 보임 |

## DMA

`lab/flash-boot/dma.c`는 DMA 채널 0으로 세 가지를 합니다. `make MAIN=dma.c`로 빌드합니다. 채널 레지스터는 0x50000000 + 채널 × 0x40에서 READ_ADDR, WRITE_ADDR, TRANS_COUNT, CTRL_TRIG 순서이고, CTRL_TRIG를 쓰면 시작합니다. CHAIN_TO를 자기 채널로 두어 연결을 끕니다(2.5.7 CH0_CTRL_TRIG).

| 시험 | 설정 | 결과 |
| --- | --- | --- |
| SRAM 4 KiB 복사 | 워드, 읽기·쓰기 주소 증가, TREQ 0x3f(제한 없음) | DMA 10 us, CPU 루프 143 us, 내용 일치 |
| boot2 CRC | flash 0x10000000부터 252바이트, 바이트 단위, SNIFF_EN. SNIFF_CTRL CALC 0(CRC-32), 초기값 0xffffffff | 0x7a4eb274. boot2 끝 4바이트와 같음 |
| UART TX | 39바이트를 UART0 DR로, 쓰기 주소 고정, TREQ 20(DREQ_UART0_TX), UARTDMACR TXDMAE | 줄이 그대로 나옴. 채널이 끝나기까지 605 us |

복사 10us는 1024워드를 클럭당 1워드로 옮길 때의 8.2us(125MHz)에 가깝습니다. UART는 TX FIFO가 32바이트라, 채널은 마지막 바이트를 FIFO에 넣는 순간 끝납니다. 39 - 32 = 7바이트가 115200 baud로 나갈 시간 7 × 86.8us = 608us가 잰 값과 맞습니다.

sniffer CRC는 bootrom이 boot2를 검사하는 CRC와 같은 값이 나왔으므로, bootrom의 CRC도 다항식 0x04c11db7, MSB 우선, 초기값 0xffffffff, 최종 반전 없음입니다.

## watchdog

`lab/flash-boot/watchdog.c`는 부팅마다 REASON과 SCRATCH0의 부팅 횟수를 보내고 횟수를 1 늘린 뒤, 짝수 회차는 CTRL.TRIGGER로, 홀수 회차는 먹이를 끊어 타임아웃으로 리셋합니다. 4회 리셋한 뒤 멈춥니다. `make MAIN=watchdog.c`로 빌드합니다.

| 레지스터 | 주소 | 용도 |
| --- | --- | --- |
| CTRL | 0x40058000 | TRIGGER(bit 31), ENABLE(bit 30), TIME(bit 23:0) |
| LOAD | 0x40058004 | 쓰면 카운터를 다시 채움 |
| REASON | 0x40058008 | TIMER(bit 0), FORCE(bit 1) |
| SCRATCH0, 1 | 0x4005800c, 010 | 부팅 횟수와 그 보수. 보수가 맞지 않으면 0부터 셈 |
| PSM WDSEL | 0x40010008 | watchdog이 리셋할 블록. 리셋값 0. ROSC·XOSC를 뺀 0x1fffc |

PSM WDSEL의 리셋값이 0이라 설정하지 않으면 watchdog이 울려도 리셋되는 블록이 없습니다. SDK `_watchdog_enable`과 같이 ROSC와 XOSC만 빼고 모두 켭니다. SCRATCH4~7은 bootrom이 부팅 때 검사하는 자리라 쓰지 않습니다(2.8.1.1).

| 항목 | 결과 |
| --- | --- |
| REASON | TRIGGER 뒤 0x2(force), 타임아웃 뒤 0x1(timer) |
| 부팅 횟수 | 리셋 4회 동안 0, 1, 2, 3, 4로 이어짐 |
| LOAD 1000000 | 먹이를 끊고 약 0.5초 뒤 리셋 (`alive 400 ms`까지 찍힘) |
| LOAD 1500000 | 약 0.75초 뒤 리셋 (`alive 700 ms`까지 찍힘) |
| CTRL 원값 | LOAD와 경과 시간에 관계없이 0x4007a120 (TIME 500000) |

리셋까지의 시간이 LOAD / 2 us라 RP2040-E1(카운터가 tick당 2씩 줄어듦, 4.7.3)과 맞습니다. CTRL.TIME은 카운트다운 중에도 값이 변하지 않았습니다. 데이터시트는 TIME을 리셋까지 남은 tick / 2라고 설명하므로 설명과 다르고, 원인은 확인하지 못했습니다.

`picotool load -x` 직후 첫 부팅의 REASON도 0x1(timer)입니다. picotool이 watchdog으로 재부팅시키기 때문으로 보며, picotool 소스로는 확인하지 않았습니다. 리셋 뒤 첫 줄 앞에 공백 한 글자가 붙어 나오는 일이 있었습니다. 리셋 중 TX 핀 상태가 바뀌어 수신 쪽이 한 글자로 읽은 것으로 추정합니다.

## flash 부팅 기록

`lab/flash-boot/flash_counter.c`는 flash 마지막 섹터(offset 0x1ff000, 4 KiB)에 부팅 기록을 남깁니다. `make MAIN=flash_counter.c`로 빌드합니다. 섹터를 워드 1024개로 보고 부팅마다 첫 빈 워드(0xffffffff)에 그 순번을 씁니다. flash 쓰기는 비트를 1에서 0으로만 바꾸므로, 빈 워드에 쓰는 동안은 섹터를 지우지 않습니다. 섹터가 차거나 순번이 어긋나 있으면 지우고 0부터 씁니다.

flash를 지우고 쓰는 동안 XIP로 읽을 수 없으므로, 그 구간을 SRAM 함수 `flash_update`로 두고 인터럽트를 끕니다. 순서는 데이터시트 2.8.3.1.3의 예와 같습니다.

| 순서 | bootrom 함수 | 내용 |
| --- | --- | --- |
| 1 | `_connect_internal_flash` ('I','F') | QSPI 패드를 SSI에 연결 |
| 2 | `_flash_exit_xip` ('E','X') | XIP에서 나와 직렬 명령 모드로 |
| 3 | `_flash_range_erase` ('R','E') | 섹터가 찼을 때만. 4096바이트, 블록 명령 0xd8 |
| 4 | `flash_range_program` ('R','P') | 256바이트 페이지. 쓸 워드 외에는 0xff |
| 5 | `_flash_flush_cache` ('F','C') | XIP 캐시 비움, CS 강제 해제 |
| 6 | SRAM에 복사해 둔 boot2 호출 | 빠른 XIP 설정으로 복귀. boot2는 lr이 0이 아니면 돌아옴 |

함수 포인터, boot2 사본, 페이지 버퍼는 모두 SRAM에 있고, 디스어셈블로 `flash_update` 안에서 flash 주소를 부르지 않는 것을 확인했습니다. SRAM 함수는 `.ramfunc` 섹션으로 두고 링커 스크립트가 `.data`에 넣어 시작 코드가 복사합니다. flash에서 SRAM까지는 `bl`이 닿지 않으므로 `long_call` 속성으로 부릅니다.

| 시험 | 결과 |
| --- | --- |
| 세 번 연속 올려 실행 | 기록 0 → 1 → 2 → 3. 쓰기 1회 1.74 ms |
| BOOTSEL에서 `picotool save -r 0x101ff000 0x10200000` | 워드 0, 1, 2 뒤로 모두 0xffffffff |
| 워드 4개로 줄인 임시 빌드로 섹터 채움 | `sector full, erase` 뒤 0부터 기록. 지우기와 쓰기 19.0 ms |

`picotool load`는 이미지 범위만 쓰므로 다시 올려도 마지막 섹터의 기록이 남습니다.

| 전원 시험 | 결과 |
| --- | --- |
| interp.c가 올라간 상태로 USB-C를 뽑았다 꽂음 | 그 뒤 `picotool save`로 읽은 섹터가 워드 0, 1 그대로 |
| flash_counter.c를 올리고 USB-C를 뽑았다 꽂음 | 버튼 없이 부팅해 `previous boots recorded 3`, 4번째 기록 |

## 두 코어와 spinlock

`lab/flash-boot/spinlock.c`는 core1을 깨운 뒤 두 코어가 공유 변수를 각각 10만 번 1씩 올립니다. lock 없이 한 번, SPINLOCK0으로 감싸서 한 번입니다. 이어서 두 코어가 UART에 10줄씩 쓰며 한 줄 전체를 SPINLOCK1로 감쌉니다. `make MAIN=spinlock.c`로 빌드합니다.

spinlock은 SIO 0xd0000100 + 번호 × 4입니다. 읽어서 0이 아니면 잡은 것이고 0이면 이미 잡혀 있으며, 아무 값이나 쓰면 풉니다(2.3.1.3). M0+에는 원자적 읽기-수정-쓰기 명령이 없어 `shared = shared + 1`은 읽기, 더하기, 쓰기 세 명령입니다.

| 조건 | lock 없음 | SPINLOCK0 |
| --- | --- | --- |
| flash에서 실행 | 200000 / 200000, 17.6 ms | 200000 / 200000, 54.4 ms |
| SRAM에서 실행 | 200000 / 200000, 13.6 ms | 200000 / 200000, 69.6 ms |
| SRAM, 반복마다 LCG로 0~7회 지연 | 184481 / 200000, 59.2 ms | 200000 / 200000, 105.2 ms |

두 코어의 시작·끝 시각을 TIMER로 기록해 세 조건 모두 두 코어가 같은 구간에서 돌았음을 확인했습니다. 같은 코드를 같은 순간 시작한 처음 두 조건에서는 lock 없이도 유실이 없었습니다. 두 코어의 박자가 고정되어 읽기와 쓰기 사이에 상대 코어의 접근이 끼지 않은 것으로 봅니다. core1에만 규칙적인 지연을 넣었을 때는 지연 위치에 따라 2040회 유실과 유실 없음이 갈렸습니다. 코어마다 다른 씨앗의 LCG로 지연을 흩뜨리자 15519회(7.8%)가 샜고, 두 번 실행해 같은 숫자가 나왔습니다.

UART 20줄은 두 코어가 한 줄씩 번갈아 썼고 섞인 줄이 없습니다.

## RTC

`lab/flash-boot/rtc.c`는 RTC를 2026-11-04 수 23:59:57로 맞추고 초가 바뀔 때마다 날짜와 TIMER 간격을 보냅니다. `make MAIN=rtc.c`로 빌드합니다.

| 항목 | 값 |
| --- | --- |
| clk_rtc | XOSC 12MHz / 256 = 46875Hz. CLK_RTC_CTRL AUXSRC 3, CLK_RTC_DIV INT 256 |
| CLKDIV_M1 | 46874. 1초 기준 (4.8.4) |
| 설정 | CTRL 0으로 끄고 RTC_ACTIVE가 0이 되길 기다림, SETUP_0·1 기록, CTRL LOAD, CTRL ENABLE, RTC_ACTIVE 대기 |
| 읽기 | RTC_0을 먼저 읽고 RTC_1. RTC_0을 읽을 때 RTC_1이 같은 순간으로 잡힘 (4.8.5.3) |

| 항목 | 결과 |
| --- | --- |
| 날짜 넘김 | 11-04 Wed 23:59:59 다음 11-05 Thu 00:00:00 |
| 초 간격 | 첫 간격 991176~999926 us, 그 뒤 1000000 us |
| 켠 직후 첫 읽기 | 1969-12-16 Mon 09:00:00. 50 us 뒤부터 설정값 |
| 설정한 57초 | 읽히지 않음. 첫 유효값이 58초 |

clk_rtc와 TIMER가 모두 XOSC에서 나오므로 간격이 정확히 1000000us로 나옵니다. RTC 쓰기는 clk_rtc 2주기(약 43us) 뒤 반영된다는 4.8.4 NOTE가 켠 직후의 이전 값과 맞아, 설정 뒤 100us를 기다린 다음 읽습니다. 57초를 건너뛰는 이유는 확인하지 못했습니다.

## ROSC 난수 비트

`lab/flash-boot/rosc_random.c`는 ROSC RANDOMBIT(0x4006001c bit 0, 2.17.5)를 읽기 간격별로 10만 번 읽어 1의 비율과 이웃 비트가 바뀐 비율을 1000분율로 보냅니다. `make MAIN=rosc_random.c`로 빌드합니다. 데이터시트는 이 비트가 보안 용도의 난수 요건을 만족하지 않는다고 밝힙니다.

| 읽기 사이 빈 루프 | 1의 비율 | 바뀐 비율 |
| --- | --- | --- |
| 0 | 456~467 | 74~113 |
| 10 | 462~463 | 320, 458, 682, 739 (실행마다 다름) |
| 100 | 462~467 | 493~498 |
| 100 + von Neumann 보정 | 500 | 500~501 |

연달아 읽으면 같은 비트가 이어지고, 간격을 100으로 늘려도 1이 46% 안팎으로 치우칩니다. von Neumann 보정은 두 비트를 읽어 01이면 0, 10이면 1을 내고 같으면 버립니다. 보정 뒤에는 두 비율 모두 500이 됐습니다. 두 비율만 본 것이라 난수 품질을 판정한 것은 아닙니다.

## interpolator

`lab/flash-boot/interp.c`는 SIO INTERP0(0xd0000080~)으로 데이터시트 2.3.1.6에 실린 pico-examples `hello_interp`의 세 예제를 레지스터로 재현하고, 데이터시트에 적힌 출력과 대조합니다. `make MAIN=interp.c`로 빌드합니다. SDK `interp_default_config`에 해당하는 CTRL 값은 SHIFT 0, MASK_LSB 0, MASK_MSB 31입니다.

| 예제 | 설정 | 결과 |
| --- | --- | --- |
| 9단 곱셈표 | ACCUM0 0, BASE0 9. POP_LANE0을 읽을 때마다 ACCUM0 += 9 | 9 18 27 … 90 |
| 니블 마스킹 | ACCUM0 0x1234abcd, MASK 4i..4i+3, ACCUM0_ADD 읽기(BASE 없이 shift·mask 값) | 0000000d ~ 10000000, SIGNED는 fffffffd ~ 10000000 |
| blend | LANE0 BLEND, BASE0 500, BASE1 1000, ACCUM1 = 255 × i / 6, PEEK_LANE1 | 500 582 666 748 832 914 998 |

세 예제 모두 데이터시트의 출력과 같습니다. blend의 끝값이 1000이 아니라 998인 것은 alpha가 최대일 때 BASE0 + 255/256 × (BASE1 - BASE0)이기 때문입니다.

## PIO UART

`lab/flash-boot/pio_uart.c`는 UART0를 끄고 PIO0의 SM0(송신, GPIO0)과 SM1(수신, GPIO1)로 8n1 115200 UART를 만듭니다. 받은 바이트를 돌려보내고, CR을 받으면 받은 바이트 수와 framing 오류 수를 보냅니다. `make MAIN=pio_uart.c`로 빌드합니다.

프로그램은 데이터시트 3.6.3·3.6.4에 실린 pico-examples `uart_tx`와 `uart_rx`(framing 검사 포함)를 Table 366 형식으로 직접 인코딩했습니다. 두 프로그램 모두 한 비트를 SM 8사이클로 보내고 받으므로 SM 클럭은 115200 × 8입니다.

| 주소 | 명령 | 인코딩 |
| --- | --- | --- |
| 0 | `pull side 1 [7]` | 0x9fa0 |
| 1 | `set x, 7 side 0 [7]` | 0xf727 |
| 2 | `out pins, 1` | 0x6001 |
| 3 | `jmp x-- 2 [6]` | 0x0642 |
| 4 | `wait 0 pin 0` | 0x2020 |
| 5 | `set x, 7 [10]` | 0xea27 |
| 6 | `in pins, 1` | 0x4001 |
| 7 | `jmp x-- 6 [6]` | 0x0646 |
| 8 | `jmp pin 12` | 0x00cc |
| 9 | `irq 4 rel` | 0xc014 |
| 10 | `wait 1 pin 0` | 0x20a0 |
| 11 | `jmp 4` | 0x0004 |
| 12 | `push` | 0x8020 |

| 레지스터 | SM0 (TX) | SM1 (RX) |
| --- | --- | --- |
| CLKDIV | INT 135, FRAC 162. 125MHz / 135.63 = 921.6kHz | 같음 |
| EXECCTRL | SIDE_EN, WRAP 0~3 | JMP_PIN 1, WRAP 4~12 |
| SHIFTCTRL | OUT_SHIFTDIR 오른쪽(LSB 먼저) | IN_SHIFTDIR 오른쪽, 바이트는 RXF의 bit 31:24 |
| PINCTRL | OUT·SET 0 1개, SIDESET 0 2비트(enable 포함) | IN_BASE 1 |

SM0을 켜고 `set pins, 1`로 TX를 high로 둔 뒤에 GPIO0·1의 FUNCSEL을 UART(2)에서 PIO0(6)으로 바꿉니다. GPIO1은 pico-examples와 같이 pull-up을 켭니다. 수신 SM의 `irq 4 rel`은 SM1에서 IRQ flag 5가 되고(3.4.9, 하위 2비트에 SM 번호를 4로 나눈 나머지로 더함), main 루프가 PIO IRQ 레지스터(0x50200030)의 bit 5를 세고 지웁니다.

| 시험 (호스트 Linux, `/dev/ttyACM0`) | 결과 |
| --- | --- |
| 115200으로 53바이트 | 에코 일치, `[rx 53, frame errors 0]` |
| 115200으로 출력 가능한 문자 380바이트 | 에코 일치 |
| 38400으로 32바이트를 보낸 뒤 115200으로 CR | framing 오류 24 |
| "BOOTSEL" 전송 (`to_bootsel.sh`) | BOOTSEL 모드로 재부팅 |

## DMA로 PWM 밝기 반복

`lab/flash-boot/dma_pwm.c`는 PWM 밝기 표를 DMA가 계속 옮기게 하고 CPU는 `wfi`로 잠듭니다. `make MAIN=dma_pwm.c`로 빌드합니다.

| 구성 | 설정 |
| --- | --- |
| PWM 슬라이스 0 | LED 구동. DIV 1, TOP 4095(30.5kHz), A_INV·B_INV |
| PWM 슬라이스 1 | 핀 연결 없음. DIV 15, TOP 65535로 127.2Hz wrap. DMA 박자용 |
| DMA 채널 0 | 밝기 표 256워드 → 슬라이스 0 CC. 읽기 주소 증가, TREQ 25(DREQ_PWM_WRAP1), CHAIN_TO 1 |
| DMA 채널 1 | 표 주소 1워드 → 채널 0의 CH0_AL3_READ_ADDR_TRIG(0x5000003c). TREQ 0x3f, 별칭 1의 CTRL로 설정만 함 |

채널 0이 256워드를 옮기고 끝나면 채널 1이 시작되어 표 시작 주소를 채널 0의 READ_ADDR 트리거 별칭에 씁니다. 이 쓰기가 채널 0을 다시 띄우고, TRANS_COUNT는 마지막으로 쓴 256으로 다시 채워집니다(2.5.1.2). 별칭 배치는 2.5.2.1 Table 118입니다. 표의 CC 값은 초록(A)과 빨강(B)이 반 바퀴 어긋난 삼각파의 제곱이라 두 색이 번갈아 밝아집니다.

| 항목 | 결과 |
| --- | --- |
| 0.5초마다 읽은 채널 0 표 위치 | 63~64칸씩 진행. 127.2Hz × 0.5초 = 63.6 |
| 한 바퀴 | 2003~2501 ms 간격으로 위치가 처음으로 돌아감(0.5초 단위로 읽은 값). 계산값 256 / 127.2Hz = 2.01초 |
| 6초 뒤 CPU `wfi` | DMA와 PWM만으로 계속 동작 |
| 색 변화 (눈으로 확인) | 빨강과 초록이 번갈아 밝아짐, 깜빡임 안 보임 |

## ADC 연속 변환과 DMA

`lab/flash-boot/adc_dma.c`는 온도 센서 채널을 CS START_MANY로 연속 변환하고, FIFO에 결과가 들어올 때마다 DREQ_ADC(36)로 DMA가 16비트씩 SRAM으로 옮깁니다. 1만 개를 받는 데 걸린 시간을 TIMER로 잽니다. `make MAIN=adc_dma.c`로 빌드합니다.

| 레지스터 | 값 |
| --- | --- |
| ADC FCS (0x4004c008) | EN(bit 0), DREQ_EN(bit 3), THRESH 1(bit 27:24). OVER(bit 11)로 넘침 확인 |
| ADC DIV (0x4004c010) | INT(bit 23:8). 0이면 변환이 끝나자마자 다음 변환, n이면 n + 1사이클마다 (4.9.2.2) |
| DMA 채널 0 | 읽기 FIFO(0x4004c00c) 고정, 쓰기 증가, 16비트, TREQ 36 |

| DIV | 1만 개에 걸린 시간 | 샘플링 속도 | raw 평균, 최소, 최대 | FIFO 넘침 |
| --- | --- | --- | --- | --- |
| 0 | 20002 us | 500000 sps | 852, 852, 860 | 없음 |
| 47999 | 10000001 us | 1000 sps | 850, 850, 853 | 없음 |

clk_adc 48MHz에서 변환 하나가 96사이클이라는 데이터시트 값(500ksps)과, DIV 47999가 1ksps라는 데이터시트 예와 같습니다.

## HardFault 보고

`lab/flash-boot/fault.c`는 HardFault를 일부러 내고, 예외 진입 때 스택에 쌓인 r0~r3, r12, lr, pc, xPSR을 UART로 보냅니다. 보고한 뒤 watchdog으로 재부팅해 SCRATCH0/1에 둔 순번으로 다음 경우를 실행합니다. `make MAIN=fault.c`로 빌드합니다.

핸들러 `isr_hardfault`는 naked 함수로, EXC_RETURN(lr) bit 2가 1이면 PSP, 0이면 MSP를 r0에 넣고 C 함수 `hardfault_report`로 갑니다. 쌓이는 레지스터의 순서와 EXC_RETURN의 의미는 ARMv6-M 아키텍처 규칙이고 RP2040 데이터시트에는 없습니다.

| 경우 | 쌓인 pc | `addr2line` | 단서 |
| --- | --- | --- | --- |
| 정렬 안 된 주소에서 `ldr` | 0x10000232 | `case_unaligned` | `ldr r2, [r3]`, r3 = 0x20000201 |
| 0x30000000 읽기 | 0x10000248 | `case_unmapped` | `ldr r2, [r3]`, r3 = 0x30000000 |
| bit 0이 0인 주소로 `blx` | 0x10000254 | `target` | xPSR의 T(bit 24)가 0 |
| MPU 접근 금지 영역에 쓰기 | 0x10000290 | `case_mpu` | `strb r3, [r2, #16]`, r2 = 0x20000100 |

MPU는 영역 0을 256바이트 정렬 배열에 두고 RASR SIZE 7(2^8 = 256바이트), XN, AP 000으로 설정했습니다. AP 000이 접근 금지라는 것은 ARMv6-M 규칙이고, 데이터시트 2.4.6에는 AP 필드 위치만 있습니다. MPU_CTRL은 ENABLE과 PRIVDEFENA를 켜서 나머지 주소는 기본 메모리 맵을 따릅니다.

0x30000000은 데이터시트 주소 맵에 블록이 없는 구역이고, 읽기가 HardFault로 이어졌습니다.

## SLEEP과 DORMANT

`lab/flash-boot/sleep_dormant.c`는 두 저전력 상태를 차례로 시험합니다. `make MAIN=sleep_dormant.c`로 빌드합니다. 전류는 재지 않았으므로 전력이 줄었는지는 판정하지 않고, 클럭이 멈췄는지만 봅니다.

SLEEP은 두 코어가 모두 WFI/WFE이고 DMA가 쉴 때 들어가며, 그동안 클럭 게이트가 WAKE_ENx 대신 SLEEP_ENx를 따릅니다(2.11.2). 핀에 연결하지 않은 PWM 슬라이스 2를 125MHz / 255로 돌리고, TIMER 알람 0으로 50ms 뒤 깨어나 그동안 CTR이 움직인 양을 셉니다. core1은 bootrom 안에서 WFE로 잠들어 있습니다.

| SCR.SLEEPDEEP | SLEEP_EN0 | 잠든 시간 | PWM CTR 이동 |
| --- | --- | --- | --- |
| 0 | 모두 1 | 50002 us | 24512 |
| 0 | CLK_SYS_PWM(bit 17)만 0 | 50000 us | 0 |
| 1 | 모두 1 | 50000 us | 24510 |
| 1 | CLK_SYS_PWM만 0 | 50000 us | 1 |

PWM 클럭이 돌면 50ms에 24510카운트입니다. SLEEP_EN0에서 PWM을 끄면 SLEEPDEEP과 관계없이 카운터가 멈췄으므로, 이 조건에서는 `wfi`만으로 SLEEP 상태가 됐습니다. 데이터시트의 hello_sleep 설명은 deep sleep을 켜지만, 클럭 게이트가 바뀌는 데 SLEEPDEEP이 필요하지는 않았습니다.

DORMANT에서는 모든 발진기가 멈추고 GPIO 이벤트나 RTC 인터럽트로 깨어납니다(2.11.3). 순서는 다음과 같습니다.

| 순서 | 동작 |
| --- | --- |
| 1 | clk_sys SRC를 clk_ref(XOSC 12MHz)로 바꾸고 CLK_SYS_SELECTED 대기 |
| 2 | PLL_SYS PWR = 0x2d(VCOPD, POSTDIVPD, DSMPD, PD). DORMANT는 PLL을 멈추지 않음 (2.11.3) |
| 3 | ROSC CTRL ENABLE = 0xd1e(끔) |
| 4 | IO_BANK0 INTR0 GPIO1_EDGE_LOW를 지우고 DORMANT_WAKE_INTE0(0x40014160)의 bit 6을 켬 |
| 5 | XOSC DORMANT(0x40024008) = 0x636f6d61. 여기서 멈춤 |
| 6 | 깨어나면 XOSC STATUS STABLE 대기, 깨움 설정과 INTR 지움, ROSC CTRL ENABLE = 0xfab(켬) |

깨우는 신호는 Debug Probe가 GPIO1(UART RX)로 보내는 바이트의 시작 비트(falling edge)입니다. PC가 dormant 진입 줄을 받고 3초 뒤와 5초 뒤 0x00을 보낸 두 경우 모두 깨어났고, TIMER는 두 경우 모두 36us만 늘었습니다. TIMER의 1us tick은 clk_ref에서 나오므로 발진기가 멈춘 동안 TIMER도 멈춘 것입니다. 깨어난 뒤에는 clk_sys가 12MHz로 남습니다.

처음 버전은 6번에서 ROSC를 다시 켜지 않았습니다. 그 상태로 UART "BOOTSEL"을 보내자 보드가 USB 장치로 다시 나타나지 않았고, USB-C를 뽑았다 꽂은 뒤 부팅 직후 "BOOTSEL"을 반복해 보내 BOOTSEL 모드로 되돌렸습니다. bootrom 재부팅은 watchdog 리셋이고, SDK 방식대로 WDSEL에서 ROSC와 XOSC를 빼면 리셋 뒤에도 ROSC가 꺼진 채 남는데 리셋 직후 clk_ref와 clk_sys는 ROSC에서 시작하기 때문으로 봅니다. bootrom `_reset_to_usb_boot`의 WDSEL 값은 확인하지 않았습니다. ROSC를 다시 켜도록 고친 뒤에는 같은 시험 뒤 BOOTSEL 전환이 됐습니다.

## SysTick·PendSV 문맥 전환

`lab/flash-boot/threads.c`는 SysTick 1ms마다 PendSV를 걸고, PendSV 핸들러가 두 작업의 r4~r11과 PSP를 바꿔 번갈아 실행합니다. `make MAIN=threads.c`로 빌드합니다.

| 레지스터 | 주소 | 값 |
| --- | --- | --- |
| SYST_RVR | 0xe000e014 | 124999. clk_sys 125MHz에서 1ms |
| SYST_CSR | 0xe000e010 | ENABLE, TICKINT, CLKSOURCE(프로세서 클럭) |
| SHPR3 | 0xe000ed20 | PRI_15(SysTick)와 PRI_14(PendSV)를 가장 낮은 3으로 |
| ICSR | 0xe000ed04 | SysTick 핸들러가 PENDSVSET(bit 28)을 씀 |

예외 진입 때 하드웨어가 r0~r3, r12, lr, pc, xPSR을 PSP에 쌓으므로 PendSV는 나머지 r4~r11만 그 아래에 저장합니다. M0+의 `stmia`는 r0~r7만 다루므로 r8~r11은 r4~r7로 옮겨 저장합니다. 저장한 PSP를 작업 표에 넣고 다른 작업의 PSP를 꺼내 같은 순서로 되돌린 뒤, EXC_RETURN(lr)으로 돌아가면 다른 작업이 이어서 실행됩니다. 하드웨어가 쌓는 레지스터 순서와 EXC_RETURN 0xfffffffd의 의미는 ARMv6-M 규칙입니다.

main은 PSP를 stack0 끝으로 두고 CONTROL.SPSEL(bit 1)을 켠 뒤 작업 0 함수를 부릅니다. 작업 1은 하드웨어 프레임 8워드(pc = 작업 함수, xPSR = 0x01000000)와 저장분 8워드를 stack1에 미리 만들어 둡니다.

| 항목 | 결과 |
| --- | --- |
| SysTick 때 돌던 작업 (1초마다, 5회) | 작업 0 500회, 작업 1 500회 |
| 작업 0 지역 변수 주소 | 0x20000804. stack0 0x20000428~0x20000828 안 |
| 작업 1 지역 변수 주소 | 0x20000424. stack1 0x20000028~0x20000428 안 |

첫 버전에서는 main 함수 안에서 PSP로 바꾼 뒤 같은 함수에서 계속 일했고, 지역 변수 주소가 0x20000824로 stack0 밖에 나왔습니다. 컴파일러는 함수의 지역 변수를 SP 기준으로 잡는데 함수 중간에 SP가 바뀌었기 때문입니다. 스택을 바꾼 직후 새 함수를 부르도록 고쳤습니다. 또 작업 0이 작업 1의 카운터를 0으로 되돌리는 방식은 작업 1의 읽기와 쓰기 사이에 전환이 끼면 덮어써져 동작하지 않았고, 이전 값과의 차이로 세도록 바꿨습니다.

SIO 나눗셈기는 작업 0만 씁니다. 두 작업이 함께 쓰면 전환 때 나눗셈기 상태도 저장해야 합니다(board.c 주석, 2.3.1.5).

## flash SFDP와 고유 ID

`lab/flash-boot/flash_id.c`는 flash 칩에 명령을 직접 보내 JEDEC ID(0x9f), SFDP(0x5a), 고유 ID(0x4b)를 읽습니다. `make MAIN=flash_id.c`로 빌드합니다. 전송 방식은 `tools/jedec-id/main.c`와 같고, flash 부팅 기록처럼 SRAM 함수에서 인터럽트를 끄고 실행한 뒤 SRAM에 복사한 boot2로 XIP를 되살립니다. 마지막에 XIP로 읽은 0x100000fc가 0x7a4eb274여서 XIP 복귀를 확인했습니다.

| 명령 | 보낸 바이트 | 받은 값 |
| --- | --- | --- |
| JEDEC ID | 0x9f | 85 60 15 |
| SFDP | 0x5a, 주소 3바이트 0, dummy 1바이트 | 256바이트 (아래) |
| 고유 ID | 0x4b, dummy 4바이트 | 16바이트. 앞 7바이트가 ASCII "AP4R680", 나머지 9바이트는 공개 저장소라 적지 않음 |

SFDP 해석은 JESD216의 필드 배치를 따른 것이고 규격 원문과 Puya 데이터시트로는 확인하지 않았습니다.

| 위치 | 값 | 해석 |
| --- | --- | --- |
| 0x00 | 53 46 44 50 | 서명 "SFDP" |
| 0x04 | 00 01 01 | SFDP 1.0, 파라미터 헤더 2개 |
| 0x08 | 00 00 01 09 30 00 00 ff | 기본 파라미터 표(ID 0x00) 1.0, 9 dword, 위치 0x30 |
| 0x10 | 85 00 01 03 60 00 00 ff | 제조사 0x85(Puya, JEDEC ID 첫 바이트와 같음) 표 1.0, 3 dword, 위치 0x60 |
| 0x30 dword 1 | 0xfff920e5 | 4KB 지우기 명령 0x20, 3바이트 주소, 1-1-2·1-2-2·1-4-4·1-1-4 읽기 지원 |
| 0x34 dword 2 | 0x00ffffff | 용량 0x00ffffff + 1 = 16 Mbit = 2MB |
| 0x38 dword 3 | 0x6b08eb44 | 1-4-4 읽기 0xeb, mode 2클럭, dummy 4클럭. 1-1-4 읽기 0x6b, dummy 8클럭 |
| 0x3c dword 4 | 0xbb803b08 | 1-1-2 읽기 0x3b, dummy 8클럭. 1-2-2 읽기 0xbb, mode 4클럭 |
| 0x4c dword 8 | 0x520f200c | 지우기 단위 4KB(0x20), 32KB(0x52) |
| 0x50 dword 9 | 0x8108d810 | 지우기 단위 64KB(0xd8), 256바이트(0x81) |

용량 2MB는 JEDEC ID의 0x15(2^21바이트)와 같습니다. 공장 boot2의 원본인 pico-sdk `boot2_w25q080.S`는 CMD_READ 0xeb, MODE_CONTINUOUS_READ 0xa0(4선에서 8비트 = 2클럭), WAIT_CYCLES 4로 읽기를 설정하고, 이 값이 칩이 SFDP로 밝힌 1-4-4 읽기 조건과 같습니다. 회로도와 다른 Puya 칩에서 공장 boot2가 동작하는 것과 맞습니다. 0x60의 Puya 표 3 dword(0x23003600, 0x6477f99e, 0xffffe8d9)는 해석하지 않았습니다.

## GPIO 인터럽트와 우선순위

`lab/flash-boot/gpio_irq.c`는 파랑 LED 핀 GPIO25를 SIO 출력으로 0과 1로 바꿔 같은 핀에 엣지를 만들고, IO_BANK0 인터럽트(IRQ 13)를 봅니다. `make MAIN=gpio_irq.c`로 빌드합니다. 외부 배선은 쓰지 않습니다. 패드 리셋값 0x56에 IE(bit 6)가 들어 있어 출력 중에도 같은 핀의 입력을 읽습니다.

처음에는 핀을 구동하지 않으려고 GPIO26 CTRL의 INOVER(bit 17:16)로 입력만 바꿨지만 인터럽트에 반영되지 않았습니다.

| GPIO26 상태 | STATUS INFROMPAD | STATUS INTOPERI | INTR EDGE_HIGH | INTR LEVEL_HIGH |
| --- | --- | --- | --- | --- |
| 오버라이드 없음, 풀다운 | 0 | 0 | 0 | 0 |
| INOVER = HIGH | 0 | 1 | 0 | 0 |

INOVER는 주변장치로 가는 입력(INTOPERI)만 바꾸고 인터럽트 감지는 패드 입력을 따릅니다. 인터럽트 쪽에는 별도의 IRQOVER(bit 29:28)가 있습니다(2.19.6.1 GPIO_CTRL).

GPIO25 INTR3 비트(bit 7:4)는 인터럽트를 켜지 않고 읽었습니다.

| 동작 | EDGE_HIGH | EDGE_LOW | LEVEL_HIGH | LEVEL_LOW |
| --- | --- | --- | --- | --- |
| 0으로 두고 INTR 지움 | 0 | 0 | 0 | 1 |
| 1로 바꿈 | 1 | 0 | 1 | 0 |
| 다시 0 | 1 | 1 | 0 | 1 |
| INTR에 1을 써서 지움 | 0 | 0 | 0 | 1 |
| 연속한 두 store로 1 → 0 | 1 | 1 | 0 | 1 |

엣지 비트는 지울 때까지 남고 레벨 비트는 현재 값을 따릅니다(2.19.3). SIO store 두 개 사이의 짧은 펄스도 두 엣지가 모두 잡혔습니다.

진입 지연은 SysTick을 인터럽트 없이 clk_sys로 돌려 잽니다. main이 SYST_CVR을 읽고 SIO_GPIO_OUT_SET에 쓴 뒤, 핸들러가 첫 동작으로 SYST_CVR을 읽어 두 값의 차를 냅니다.

| 조건 | 1회째 | 2회째부터 |
| --- | --- | --- |
| `cpsid i` 안에서 엣지를 만들고 `cpsie i` | 480 클럭 | 29 클럭 (7회) |
| 인터럽트를 켠 채 엣지를 만듦 | 177 클럭 | 29 클럭 (3회) |

29클럭에는 main의 CVR 읽기 뒤 명령 4개, Cortex-M0+ 예외 진입, 핸들러의 `push`, `sub sp`, 리터럴 읽기, CVR 읽기가 들어 있습니다. 엣지가 인터럽트 대기까지 가는 시간만 따로 떼어 재지는 않았습니다. 두 조건이 같은 29클럭이므로 `cpsie i` 시점에는 대기가 이미 걸려 있었습니다. 1회째가 긴 것은 핸들러와 루프 코드가 XIP 캐시에 없었기 때문으로 보며, 캐시 상태는 확인하지 않았습니다.

우선순위는 NVIC_IPR(0xe000e400부터, IRQ마다 8비트 중 bit 7:6)에 0~3을 씁니다. GPIO 핸들러는 들어올 때 G, 나갈 때 g를 기록하고, 안에서 NVIC_ISPR로 IRQ 0(TIMER 알람 0)을 대기시킵니다. IRQ 0 핸들러는 T를 기록합니다. "둘 다 대기"는 인터럽트를 끈 채 NVIC_ISPR로 두 IRQ를 함께 대기시킨 뒤 켠 경우입니다.

| GPIO(IRQ 13) | TIMER(IRQ 0) | 시험 | 순서 |
| --- | --- | --- | --- |
| 3 | 0 | GPIO 안에서 TIMER 대기 | GTg |
| 0 | 0 | GPIO 안에서 TIMER 대기 | GgT |
| 0 | 3 | GPIO 안에서 TIMER 대기 | GgT |
| 0 | 0 | 둘 다 대기 | TGg |
| 0 | 3 | 둘 다 대기 | GgT |
| 3 | 0 | 둘 다 대기 | TGg |

우선순위 값이 더 작은 인터럽트만 실행 중인 핸들러에 끼어들었습니다. 값이 같으면 IRQ 번호가 작은 TIMER도 끼어들지 못하고, 둘이 함께 대기 중일 때 먼저 실행되는 순서만 정합니다(2.3.2).

## watchdog 부팅

bootrom은 POR·BOR가 아닌 리셋 뒤 watchdog SCRATCH4~7을 보고, 값이 맞으면 flash 부팅 대신 적힌 주소로 갑니다(2.8.1.1). `lab/flash-boot/watchdog_boot.c`는 SRAM에 둔 함수로 들어간 뒤 그 순간의 상태를 scratch에 남기고, 다시 리셋해 일반 flash 부팅으로 돌아와 그 값을 보냅니다. `make MAIN=watchdog_boot.c`로 빌드합니다.

| 레지스터 | 쓴 값 |
| --- | --- |
| SCRATCH4 (0x4005801c) | 0xb007c0d3 |
| SCRATCH5 | entry ^ -0xb007c0d3 |
| SCRATCH6 | SP 0x20040000 |
| SCRATCH7 | entry 0x20000001 (`.ramfunc`에 둔 `sram_entry`, Thumb 비트 포함) |
| PSM_WDSEL | 0x1fffc (ROSC, XOSC를 뺀 전부) |
| WATCHDOG CTRL | TRIGGER(bit 31) |

`sram_entry`가 실행될 때는 XIP가 설정되지 않은 상태이므로, 역어셈블로 함수 본문과 리터럴 풀이 모두 0x20000000~0x20000087 안에 있고 flash 주소를 부르지 않는 것을 확인했습니다. 함수는 읽은 값을 SCRATCH0~3, 5~7에 쓰고 PSM_WDSEL을 다시 쓴 뒤 TRIGGER로 리셋합니다. bootrom이 SCRATCH4를 지웠으므로 이 리셋은 일반 flash 부팅이 됩니다.

| `sram_entry`가 본 값 | 결과 |
| --- | --- |
| SCRATCH4 | 0x00000000. bootrom이 진입 전에 지움 (2.8.1.1) |
| PSM_WDSEL | 0x0001fffc. 리셋 전에 쓴 값이 남음 |
| SP | 0x20040000. SCRATCH6 값 |
| CLK_SYS_SELECTED, CLK_REF_SELECTED | 0x1, 0x1. clk_sys = clk_ref, clk_ref = ROSC |
| WATCHDOG REASON | 0x2 (FORCE) |
| SIO CPUID | 0 |

PSM_WDSEL에 SRAM0~5가 들어 있었지만 리셋 전에 `.data`로 복사해 둔 `sram_entry` 코드는 리셋 뒤에도 남아 실행됐습니다. PSM_WDSEL 자체도 watchdog 리셋으로 지워지지 않았습니다. 진입 시점에 clk_ref와 clk_sys는 ROSC에서 돌고 있었고, DORMANT 기록에서 ROSC를 꺼 둔 채 재부팅하면 멈춘 현상과 맞는 조건입니다.

리셋 직전 줄과 두 번째 flash 부팅의 첫 줄은 PC 수신 시각으로 약 16ms 떨어져 있었습니다. 이 시간에는 bootrom의 watchdog 부팅, `sram_entry`, 두 번째 bootrom과 boot2, `board_init`이 모두 들어 있습니다.

## SRAM과 XIP 속도

`lab/flash-boot/xip_speed.c`는 같은 본문의 함수를 flash(`.text`)와 SRAM(`.ramfunc`)에 하나씩 두고 SysTick 클럭 수로 잽니다. `make MAIN=xip_speed.c`로 빌드합니다. 재는 함수와 데이터 합 함수는 SRAM에 있어, 캐시를 비워도 재는 코드 자체는 flash를 읽지 않습니다. 인터럽트는 끄고 잽니다. 각 경우 XIP CTR_HIT(0x1400000c)와 CTR_ACC(0x14000010)를 0으로 지운 뒤의 증가분도 봅니다.

코드는 xorshift 200회 루프이고, 역어셈블로 두 사본의 명령이 같은 것을 확인했습니다(0x1c바이트).

| 코드 위치 | 클럭 | XIP hit / 접근 |
| --- | --- | --- |
| SRAM (두 번 모두) | 2211 | 0 / 0 |
| flash, FLUSH 직후 | 2564 | 1196 / 1203 |
| flash, 캐시에 있음 | 2211 | 1203 / 1203 |
| flash, CTRL.EN = 0 | 60157 | 0 / 0 |

캐시에 있는 flash 코드는 SRAM과 같은 클럭으로 돕니다(캐시 hit 1클럭, 2.6.3.1). FLUSH 직후에는 miss 7번에 353클럭이 더 들었습니다. 캐시를 끄면 명령 읽기 1203번에 60157클럭으로, 읽기 한 번에 약 50클럭입니다.

데이터는 flash 앞 8KB(2048워드)를 더합니다.

| 읽은 곳 | 클럭 | XIP hit / 접근 |
| --- | --- | --- |
| SRAM 복사본 | 22543 | 0 / 0 |
| 0x10000000, FLUSH 직후 | 126990 | 0 / 2048 |
| 0x10000000, 캐시에 있음 | 22543 | 2048 / 2048 |
| 0x13000000 (캐시 안 거침), 캐시에 있는 상태 | 122894 | 2048 / 2048 |
| 0x13000000, FLUSH 직후 | 122895 | 0 / 2048 |
| 0x10000000 짝수 워드만, FLUSH 직후 | 63502 | 0 / 1024 |
| 이어서 홀수 워드만 | 60745 | 54 / 1024 |

캐시를 거치지 않는 0x13 alias는 캐시 상태와 관계없이 워드당 약 49클럭 더 걸렸습니다. 다만 그 데이터가 캐시에 있을 때는 CTR_HIT가 접근 수만큼 올랐습니다. 데이터시트는 0x13을 캐시를 완전히 우회하는 alias로 설명하고, CTR_HIT는 캐시 데이터로 처리한 접근을 센다고 적습니다. 시간으로 보면 캐시 데이터를 쓰지 않았고, 카운터가 오른 이유는 확인하지 않았습니다.

데이터시트의 캐시 줄은 8바이트이지만(2.6.3.2), FLUSH 직후 순차로 읽으면 2048워드가 모두 miss였습니다. 짝수 워드만 읽어 줄을 채운 뒤 같은 줄의 홀수 워드를 읽어도 1024번 중 54번만 hit였습니다. 이 설정에서는 miss 한 번이 옆 워드를 캐시에 넣지 않았습니다. 코드 쪽 miss 7번도 0x1c바이트 함수의 워드 수 7과 같습니다. boot2 `boot2_w25q080.S`는 SSI CTRLR1 NDF를 0("single 32b read")으로 두므로 XIP 읽기 한 번이 32비트만 가져오는 것이 원인 후보이지만 확인하지 않았습니다.

캐시를 끈 읽기 약 50클럭은 1-4-4 연속 읽기 한 번(주소 6클럭, mode 2클럭, dummy 4클럭, 데이터 8클럭 = SCK 20클럭, clk_sys/2라 clk_sys 40클럭)에 CS와 SSI 처리 시간을 더한 값으로 보고 있으며, SSI 쪽 시간은 따로 재지 않았습니다.
