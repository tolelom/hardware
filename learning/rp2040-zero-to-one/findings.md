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
| 5 | UARTLCR_H (0x4003402c) | 8비트, FIFO 켬. IBRD·FBRD는 LCR_H를 쓸 때 반영되므로 뒤에 씀 |
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

이 순서는 SDK `hardware_flash`의 `flash_do_cmd`와 같습니다. 코드는 `tools/jedec-id/main.c`이고 `make APP=tools/jedec-id`로 빌드합니다.

| 자료 | flash 칩 |
| --- | --- |
| 회로도 v1.3 U3 | GD25Q16CEIG (GigaDevice, 0xC8) |
| 칩이 보낸 JEDEC ID | `85 60 15`. 제조사 0x85 Puya, 0x60 P25Q 계열, 0x15 = 2MB |

0x85 6015를 P25Q16H로 대응시킨 근거는 Xradio의 flash 지원 목록과 Puya 칩을 읽은 공개 기록이고, Puya 공식 데이터시트로는 확인하지 않았습니다.

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

.rodata 끝을 4바이트로 맞추지 않으면 .data의 LMA가 문자열 길이만큼 어긋납니다. 첫 빌드에서 LMA가 4의 배수가 아니어서 `flash.ld`의 ASSERT가 링크를 멈췄습니다. Cortex-M0+는 비정렬 `ldr`에서 HardFault를 내므로 복사 루프가 첫 워드에서 멈춥니다.

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

벡터 테이블은 시스템 예외 16칸과 IRQ 32칸으로 192바이트입니다. VTOR은 bit 31:8만 쓰므로 0x10000100(256바이트 정렬)에 둡니다. `start.S`는 핸들러 이름을 모두 `.weak`와 `.thumb_set name, hang`으로 선언하고, C에서 같은 이름의 함수(`isr_irq0`)를 정의하면 링커가 그 주소를 테이블에 넣습니다. 정의하지 않은 칸은 hang을 가리켜, 예상하지 못한 인터럽트가 오면 그 자리에서 멈춥니다. Cortex-M0+는 예외 진입 때 r0~r3, r12, lr, pc, xPSR을 하드웨어로 스택에 쌓으므로 핸들러를 평범한 C 함수로 쓸 수 있습니다.

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
