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
