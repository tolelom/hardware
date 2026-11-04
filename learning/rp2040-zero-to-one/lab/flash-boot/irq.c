/*
 * TIMER 알람 0 인터럽트(IRQ 0)로 0.5초마다 LED를 뒤집는다.
 * main은 wfi로 잠들었다가 인터럽트가 올 때마다 횟수를 UART로 찍는다.
 */
#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *) (addr))
#define ALIAS_CLR 0x3000u

/* RESETS (2.14) */
#define RESETS_BASE 0x4000c000u
#define RESETS_RESET_SET REG(RESETS_BASE + 0x2000u + 0x0u)
#define RESETS_RESET_CLR REG(RESETS_BASE + 0x3000u + 0x0u)
#define RESETS_RESET_DONE REG(RESETS_BASE + 0x08u)
#define RESETS_IO_BANK0 (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)
#define RESETS_TIMER (1u << 21)
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
#define CLK_REF_CTRL REG(CLOCKS_BASE + 0x30u)
#define CLK_REF_SELECTED REG(CLOCKS_BASE + 0x38u)
#define CLK_PERI_CTRL REG(CLOCKS_BASE + 0x48u)
#define CLK_REF_SRC_XOSC 2u
#define CLK_PERI_CTRL_ENABLE (1u << 11)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

/* WATCHDOG tick (4.7.2), TIMER (4.6) */
#define WATCHDOG_TICK REG(0x40058000u + 0x2cu)
#define WATCHDOG_TICK_ENABLE (1u << 9)
#define TICK_CYCLES_12MHZ 12u
#define TIMER_BASE 0x40054000u
#define TIMER_ALARM0 REG(TIMER_BASE + 0x10u)
#define TIMER_TIMERAWL REG(TIMER_BASE + 0x28u)
#define TIMER_INTR REG(TIMER_BASE + 0x34u)
#define TIMER_INTE REG(TIMER_BASE + 0x38u)
#define TIMER_ALARM_0 (1u << 0)

/* NVIC (2.4.8) */
#define NVIC_ISER REG(0xe000e100u)
#define NVIC_ICPR REG(0xe000e280u)
#define M0PLUS_VTOR REG(0xe000ed08u)
#define TIMER_IRQ_0 0u

#define ALARM_PERIOD_US 500000u

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


/* M0+에 나눗셈 명령이 없어 10의 거듭제곱을 빼 가며 자릿수를 센다 */
static void uart_putdec(uint32_t value) {
    static const uint32_t powers[] = {
        1000000000u, 100000000u, 10000000u, 1000000u, 100000u,
        10000u, 1000u, 100u, 10u, 1u,
    };
    int started = 0;
    for (unsigned i = 0; i < sizeof powers / sizeof powers[0]; i++) {
        char digit = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            digit++;
        }
        if (digit != '0' || started || powers[i] == 1u) {
            uart_putc(digit);
            started = 1;
        }
    }
}

static void reset_block(uint32_t blocks) {
    RESETS_RESET_SET = blocks;
    unreset(blocks);
}

/* clk_ref는 glitchless mux라 켠 채로 바꾸고 SELECTED로 완료를 기다린다 */
static void clk_ref_from_xosc(void) {
    CLK_REF_CTRL = CLK_REF_SRC_XOSC;
    while (CLK_REF_SELECTED != (1u << CLK_REF_SRC_XOSC)) {
    }
}

static void timer_start(void) {
    WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | TICK_CYCLES_12MHZ;
    reset_block(RESETS_TIMER);
}

static void uart_puthex32(uint32_t value) {
    static const char hex[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xfu]);
    }
}

static uint32_t next_alarm;
volatile uint32_t alarm_count;

/* start.S의 weak 심볼 isr_irq0을 이 정의가 덮어쓴다 */
void isr_irq0(void) {
    TIMER_INTR = TIMER_ALARM_0;
    next_alarm += ALARM_PERIOD_US;
    TIMER_ALARM0 = next_alarm;
    SIO_GPIO_OUT_XOR = 1u << LED_RED;
    alarm_count++;
}

int main(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init();
    xosc_start();
    clk_peri_from_xosc();
    uart_init();
    clk_ref_from_xosc();
    timer_start();

    uart_puts("VTOR ");
    uart_puthex32(M0PLUS_VTOR);
    uart_puts(", irq0 vector ");
    uart_puthex32(((volatile uint32_t *) M0PLUS_VTOR)[16 + TIMER_IRQ_0]);
    uart_puts("\r\n");

    TIMER_INTE = TIMER_ALARM_0;
    NVIC_ICPR = 1u << TIMER_IRQ_0;
    NVIC_ISER = 1u << TIMER_IRQ_0;
    next_alarm = TIMER_TIMERAWL + ALARM_PERIOD_US;
    TIMER_ALARM0 = next_alarm;

    uint32_t seen = 0;
    while (1) {
        __asm volatile("wfi");
        uint32_t count = alarm_count;
        if (count != seen) {
            seen = count;
            uart_puts("irq ");
            uart_putdec(count);
            uart_puts("\r\n");
        }
    }
}
