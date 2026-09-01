#include "uart.h"

#define REG(addr) (*(volatile unsigned int *)(addr))

#define RESETS_BASE    0x4000c000u
#define CLOCKS_BASE    0x40008000u
#define IO_BANK0_BASE  0x40014000u
#define UART0_BASE     0x40034000u

#define RESETS_RESET      (RESETS_BASE + 0x00)
#define RESETS_RESET_DONE (RESETS_BASE + 0x08)
#define RESETS_UART0      (1u << 22)

#define CLK_PERI_CTRL     (CLOCKS_BASE + 0x48)
#define CLK_PERI_ENABLE   (1u << 11)   /* AUXSRC 리셋값 0 이 clk_sys 다 */

#define IO_BANK0_GPIO0_CTRL (IO_BANK0_BASE + 0x04)   /* 0x04 + 0*8 */
#define IO_BANK0_GPIO1_CTRL (IO_BANK0_BASE + 0x0c)   /* 0x04 + 1*8 */
#define FUNCSEL_UART        2u                       /* GPIO0/1 의 F2 */

#define UART_DR     (UART0_BASE + 0x000)
#define UART_FR     (UART0_BASE + 0x018)
#define UART_IBRD   (UART0_BASE + 0x024)
#define UART_FBRD   (UART0_BASE + 0x028)
#define UART_LCR_H  (UART0_BASE + 0x02c)
#define UART_CR     (UART0_BASE + 0x030)

#define UART_FR_TXFF       (1u << 5)   /* 송신 FIFO 가 꽉 찼다 */
#define UART_LCR_H_WLEN_8  (3u << 5)
#define UART_LCR_H_FEN     (1u << 4)   /* FIFO 사용 */
#define UART_CR_UARTEN     (1u << 0)
#define UART_CR_TXE        (1u << 8)
#define UART_CR_RXE        (1u << 9)

/* clk_peri 125MHz 에서 1 Mbaud.
   125000000 / (16 x 1000000) = 7.8125
   IBRD = 7, FBRD = round(0.8125 x 64) = 52
   실제 125000000 / (16 x 7.8125) = 1000000 이라 오차가 없다.
   115200 은 IBRD 67, FBRD 52 였고 오차 0.006% 였다. */
#define UART_IBRD_1M  7u
#define UART_FBRD_1M  52u


void uart_init(void) {
    REG(CLK_PERI_CTRL) = CLK_PERI_ENABLE;

    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_UART0;
    while ((REG(RESETS_RESET_DONE) & RESETS_UART0) == 0) {
    }

    REG(IO_BANK0_GPIO0_CTRL) = FUNCSEL_UART;
    REG(IO_BANK0_GPIO1_CTRL) = FUNCSEL_UART;

    REG(UART_IBRD) = UART_IBRD_1M;
    REG(UART_FBRD) = UART_FBRD_1M;

    REG(UART_LCR_H) = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;

    REG(UART_CR) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_putc(char c) {
    while (REG(UART_FR) & UART_FR_TXFF) {
    }
    REG(UART_DR) = (unsigned char) c;
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        s++;
    }
}
