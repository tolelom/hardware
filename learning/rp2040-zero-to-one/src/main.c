#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *) (addr))

/* RESETS (2.14) */
#define RESETS_BASE 0x4000c000u
#define RESETS_RESET_CLR REG(RESETS_BASE + 0x3000u + 0x0u)
#define RESETS_RESET_DONE REG(RESETS_BASE + 0x08u)
#define RESETS_IO_BANK0 (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)
#define RESETS_UART0 (1u << 22)

/* XOSC (2.16) */
#define XOSC_BASE 0x40024000u
#define XOSC_CTRL REG(XOSC_BASE + 0x00u)
#define XOSC_STATUS REG(XOSC_BASE + 0x04u)
#define XOSC_CTRL_ENABLE (0xfabu << 12)
#define XOSC_CTRL_RANGE_1_15MHZ 0xaa0u
#define XOSC_STATUS_STABLE (1u << 31)

/* CLOCKS (2.15) */
#define CLOCKS_BASE 0x40008000u
#define CLK_PERI_CTRL REG(CLOCKS_BASE + 0x48u)
#define CLK_PERI_CTRL_ENABLE (1u << 11)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

/* IO_BANK0 (2.19) */
#define IO_BANK0_BASE 0x40014000u
#define GPIO_CTRL(n) REG(IO_BANK0_BASE + 0x04u + (n) * 0x8u)
#define FUNCSEL_UART 2u
#define FUNCSEL_SIO 5u

/* SIO (2.3.1) */
#define SIO_BASE 0xd0000000u
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x1cu)
#define SIO_GPIO_OE_SET REG(SIO_BASE + 0x24u)

/* UART0 (4.2) */
#define UART0_BASE 0x40034000u
#define UART0_DR REG(UART0_BASE + 0x000u)
#define UART0_FR REG(UART0_BASE + 0x018u)
#define UART0_IBRD REG(UART0_BASE + 0x024u)
#define UART0_FBRD REG(UART0_BASE + 0x028u)
#define UART0_LCR_H REG(UART0_BASE + 0x02cu)
#define UART0_CR REG(UART0_BASE + 0x030u)
#define UART_FR_TXFF (1u << 5)
#define UART_LCR_H_WLEN_8 (3u << 5)
#define UART_LCR_H_FEN (1u << 4)
#define UART_CR_UARTEN (1u << 0)
#define UART_CR_TXE (1u << 8)
#define UART_CR_RXE (1u << 9)

/* 12MHz / (16 x 115200) = 6.5104. 정수부 6, 소수부 0.5104 x 64 = 32.67 -> 33 */
#define UART_IBRD_115200 6u
#define UART_FBRD_115200 33u

#define UART_TX_PIN 0u
#define UART_RX_PIN 1u
#define LED_RED 17u
#define BLINK_DELAY 200000u

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
    }
}

static void unreset(uint32_t blocks) {
    RESETS_RESET_CLR = blocks;
    while ((RESETS_RESET_DONE & blocks) != blocks) {
    }
}

static void xosc_start(void) {
    XOSC_CTRL = XOSC_CTRL_ENABLE | XOSC_CTRL_RANGE_1_15MHZ;
    while (!(XOSC_STATUS & XOSC_STATUS_STABLE)) {
    }
}

/* AUXSRC는 바꿀 때 글리치가 나므로 끈 상태에서 바꾼다 */
static void clk_peri_from_xosc(void) {
    CLK_PERI_CTRL = 0;
    delay(10);
    CLK_PERI_CTRL = CLK_PERI_AUXSRC_XOSC;
    CLK_PERI_CTRL = CLK_PERI_AUXSRC_XOSC | CLK_PERI_CTRL_ENABLE;
}

/* IBRD, FBRD는 LCR_H를 쓸 때 반영되므로 LCR_H가 뒤에 온다 */
static void uart_init(void) {
    unreset(RESETS_UART0);
    UART0_IBRD = UART_IBRD_115200;
    UART0_FBRD = UART_FBRD_115200;
    UART0_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;
    UART0_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    GPIO_CTRL(UART_TX_PIN) = FUNCSEL_UART;
    GPIO_CTRL(UART_RX_PIN) = FUNCSEL_UART;
}

static void uart_putc(char c) {
    while (UART0_FR & UART_FR_TXFF) {
    }
    UART0_DR = (uint32_t) c;
}

static void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

static void led_init(void) {
    GPIO_CTRL(LED_RED) = FUNCSEL_SIO;
    SIO_GPIO_OUT_SET = 1u << LED_RED;
    SIO_GPIO_OE_SET = 1u << LED_RED;
}

int main(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init();
    xosc_start();
    clk_peri_from_xosc();
    uart_init();

    /* M0+에는 나눗셈 명령이 없어 % 10을 쓰지 않고 글자를 직접 돌린다 */
    char digit = '0';
    while (1) {
        uart_puts("hello ");
        uart_putc(digit);
        uart_puts("\r\n");
        digit = (digit == '9') ? '0' : (char) (digit + 1);
        SIO_GPIO_OUT_XOR = 1u << LED_RED;
        delay(BLINK_DELAY);
    }
}
