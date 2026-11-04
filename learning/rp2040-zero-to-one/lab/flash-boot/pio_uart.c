/*
 * PIO로 만든 UART 8n1 115200 (데이터시트 3.6.3·3.6.4, pico-examples uart_tx·uart_rx).
 * UART0 대신 PIO0 SM0이 GPIO0(TX), SM1이 GPIO1(RX)을 맡아 받은 바이트를 돌려보낸다.
 * CR을 받으면 지금까지 받은 바이트 수와 framing 오류 수를 보낸다.
 * "BOOTSEL"을 받으면 BOOTSEL 모드로 재부팅한다 (UART0 인터럽트 대신 여기서 확인).
 *
 * 프로그램은 데이터시트 3.4 Table 366 형식으로 직접 인코딩했다.
 * TX (0~3, side-set 1비트 + enable):
 *   0 pull side 1 [7]        0x9fa0
 *   1 set x, 7 side 0 [7]    0xf727
 *   2 out pins, 1            0x6001
 *   3 jmp x-- 2 [6]          0x0642
 * RX (4~12, side-set 없음):
 *   4 wait 0 pin 0           0x2020
 *   5 set x, 7 [10]          0xea27
 *   6 in pins, 1             0x4001
 *   7 jmp x-- 6 [6]          0x0646
 *   8 jmp pin 12             0x00cc
 *   9 irq 4 rel              0xc014  (SM1이면 flag 5)
 *  10 wait 1 pin 0           0x20a0
 *  11 jmp 4                  0x0004
 *  12 push                   0x8020
 */
#include "board.h"

#define PIO0_BASE 0x50200000u
#define PIO0_CTRL REG(PIO0_BASE + 0x000u)
#define PIO0_FSTAT REG(PIO0_BASE + 0x004u)
#define PIO0_TXF(sm) REG(PIO0_BASE + 0x010u + (sm) * 4u)
#define PIO0_RXF(sm) REG(PIO0_BASE + 0x020u + (sm) * 4u)
#define PIO0_IRQ REG(PIO0_BASE + 0x030u)
#define PIO0_INSTR_MEM(n) REG(PIO0_BASE + 0x048u + (n) * 4u)
#define PIO0_SM_CLKDIV(sm) REG(PIO0_BASE + 0x0c8u + (sm) * 0x18u)
#define PIO0_SM_EXECCTRL(sm) REG(PIO0_BASE + 0x0ccu + (sm) * 0x18u)
#define PIO0_SM_SHIFTCTRL(sm) REG(PIO0_BASE + 0x0d0u + (sm) * 0x18u)
#define PIO0_SM_INSTR(sm) REG(PIO0_BASE + 0x0d8u + (sm) * 0x18u)
#define PIO0_SM_PINCTRL(sm) REG(PIO0_BASE + 0x0dcu + (sm) * 0x18u)

#define FSTAT_RXEMPTY(sm) (1u << (8 + (sm)))
#define FSTAT_TXFULL(sm) (1u << (16 + (sm)))
#define EXEC_SIDE_EN (1u << 30)
#define EXEC_JMP_PIN(p) ((uint32_t) (p) << 24)
#define EXEC_WRAP(top, bottom) (((uint32_t) (top) << 12) | ((uint32_t) (bottom) << 7))
#define SHIFT_OUT_RIGHT (1u << 19)
#define SHIFT_IN_RIGHT (1u << 18)
#define PIN_OUT(base, count) ((uint32_t) (base) | ((uint32_t) (count) << 20))
#define PIN_SET(base, count) (((uint32_t) (base) << 5) | ((uint32_t) (count) << 26))
#define PIN_SIDESET(base, count) (((uint32_t) (base) << 10) | ((uint32_t) (count) << 29))
#define PIN_IN(base) ((uint32_t) (base) << 15)
#define PAD_PUE (1u << 3)
#define PAD_PDE (1u << 2)

#define SM_TX 0u
#define SM_RX 1u
#define TX_PIN 0u
#define RX_PIN 1u
#define RX_IRQ_FLAG (1u << 5)
#define UART0_IRQ_BIT (1u << 20)
/* 125MHz / (8 x 115200) = 135.634. 소수부 0.634 x 256 = 162 */
#define CLKDIV_115200 ((135u << 16) | (162u << 8))

static const uint16_t program[] = {
    0x9fa0, 0xf727, 0x6001, 0x0642,
    0x2020, 0xea27, 0x4001, 0x0646, 0x00cc, 0xc014, 0x20a0, 0x0004, 0x8020,
};

static void pio_putc(char c) {
    while (PIO0_FSTAT & FSTAT_TXFULL(SM_TX)) {
    }
    PIO0_TXF(SM_TX) = (uint8_t) c;
}

static void pio_puts(const char *s) {
    while (*s) {
        pio_putc(*s++);
    }
}

static void pio_putdec(uint32_t value) {
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char) ('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (n) {
        pio_putc(digits[--n]);
    }
}

static void pio_uart_init(void) {
    unreset(RESETS_PIO0);
    for (uint32_t i = 0; i < sizeof program / sizeof program[0]; i++) {
        PIO0_INSTR_MEM(i) = program[i];
    }

    PIO0_SM_CLKDIV(SM_TX) = CLKDIV_115200;
    PIO0_SM_EXECCTRL(SM_TX) = EXEC_SIDE_EN | EXEC_WRAP(3, 0);
    PIO0_SM_SHIFTCTRL(SM_TX) = SHIFT_OUT_RIGHT;
    PIO0_SM_PINCTRL(SM_TX) = PIN_OUT(TX_PIN, 1) | PIN_SET(TX_PIN, 1)
                             | PIN_SIDESET(TX_PIN, 2);
    PIO0_SM_INSTR(SM_TX) = 0xe001; /* set pins, 1: 쉬는 상태 high */
    PIO0_SM_INSTR(SM_TX) = 0xe081; /* set pindirs, 1 */
    PIO0_SM_INSTR(SM_TX) = 0x0000; /* jmp 0 */

    PIO0_SM_CLKDIV(SM_RX) = CLKDIV_115200;
    PIO0_SM_EXECCTRL(SM_RX) = EXEC_JMP_PIN(RX_PIN) | EXEC_WRAP(12, 4);
    PIO0_SM_SHIFTCTRL(SM_RX) = SHIFT_IN_RIGHT;
    PIO0_SM_PINCTRL(SM_RX) = PIN_IN(RX_PIN);
    PIO0_SM_INSTR(SM_RX) = 0x0004; /* jmp 4 */

    PIO0_CTRL = (1u << SM_TX) | (1u << SM_RX);

    /* SM이 TX를 high로 잡은 뒤 핀을 UART0에서 PIO0으로 넘긴다 */
    PADS_GPIO(RX_PIN) = (PADS_GPIO(RX_PIN) & ~PAD_PDE) | PAD_PUE;
    GPIO_CTRL(TX_PIN) = FUNCSEL_PIO0;
    GPIO_CTRL(RX_PIN) = FUNCSEL_PIO0;
}

int main(void) {
    board_init();
    uart_puts("switching to PIO UART\r\n");
    uart_flush();
    NVIC_ICER = UART0_IRQ_BIT;
    pio_uart_init();
    pio_puts("pio uart ready\r\n");

    static const char magic[] = "BOOTSEL";
    uint32_t matched = 0;
    uint32_t received = 0;
    uint32_t frame_errors = 0;
    while (1) {
        if (PIO0_IRQ & RX_IRQ_FLAG) {
            PIO0_IRQ = RX_IRQ_FLAG;
            frame_errors++;
        }
        if (PIO0_FSTAT & FSTAT_RXEMPTY(SM_RX)) {
            continue;
        }
        char c = (char) (PIO0_RXF(SM_RX) >> 24);
        received++;
        pio_putc(c);
        matched = c == magic[matched] ? matched + 1u : (c == magic[0] ? 1u : 0u);
        if (magic[matched] == '\0') {
            pio_puts("\r\nreboot to BOOTSEL\r\n");
            while (!(PIO0_FSTAT & (1u << (24 + SM_TX)))) {
            }
            sleep_us(2000u);
            reset_to_bootsel();
        }
        if (c == '\r') {
            pio_puts("\n[rx ");
            pio_putdec(received);
            pio_puts(", frame errors ");
            pio_putdec(frame_errors);
            pio_puts("]\r\n");
        }
    }
}
