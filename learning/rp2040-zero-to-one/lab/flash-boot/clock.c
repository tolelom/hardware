/*
 * clk_sys를 PLL_SYS 125MHz로 올리고 주파수 카운터(FC0)로 잰다.
 * clk_peri는 XOSC 그대로라 UART 분주값은 바뀌지 않는다.
 * 시간은 TIMER(1us, clk_ref = XOSC에서 만든 tick)로 잰다.
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
#define RESETS_PLL_SYS (1u << 12)
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
#define CLK_SYS_CTRL REG(CLOCKS_BASE + 0x3cu)
#define CLK_SYS_SELECTED REG(CLOCKS_BASE + 0x44u)
#define CLK_PERI_CTRL REG(CLOCKS_BASE + 0x48u)
#define CLK_REF_SRC_XOSC 2u
#define CLK_SYS_SRC_REF 0u
#define CLK_SYS_SRC_AUX 1u
#define CLK_SYS_AUXSRC_PLL_SYS (0u << 5)
#define CLK_PERI_CTRL_ENABLE (1u << 11)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

/* 주파수 카운터 (2.15.6.2) */
#define FC0_REF_KHZ REG(CLOCKS_BASE + 0x80u)
#define FC0_MIN_KHZ REG(CLOCKS_BASE + 0x84u)
#define FC0_MAX_KHZ REG(CLOCKS_BASE + 0x88u)
#define FC0_INTERVAL REG(CLOCKS_BASE + 0x90u)
#define FC0_SRC REG(CLOCKS_BASE + 0x94u)
#define FC0_STATUS REG(CLOCKS_BASE + 0x98u)
#define FC0_RESULT REG(CLOCKS_BASE + 0x9cu)
#define FC0_STATUS_DONE (1u << 4)
#define FC0_STATUS_RUNNING (1u << 8)
#define FC0_RESULT_KHZ_LSB 5u
#define FC0_SRC_PLL_SYS 0x01u
#define FC0_SRC_ROSC 0x03u
#define FC0_SRC_XOSC 0x05u
#define FC0_SRC_CLK_REF 0x08u
#define FC0_SRC_CLK_SYS 0x09u
#define FC0_SRC_CLK_PERI 0x0au
#define XOSC_KHZ 12000u

/* PLL_SYS (2.18). 12MHz / 1 x 125 = 1500MHz VCO, / 6 / 2 = 125MHz */
#define PLL_SYS_BASE 0x40028000u
#define PLL_SYS_CS REG(PLL_SYS_BASE + 0x0u)
#define PLL_SYS_PWR_CLR REG(PLL_SYS_BASE + ALIAS_CLR + 0x4u)
#define PLL_SYS_FBDIV_INT REG(PLL_SYS_BASE + 0x8u)
#define PLL_SYS_PRIM REG(PLL_SYS_BASE + 0xcu)
#define PLL_CS_LOCK (1u << 31)
#define PLL_PWR_PD (1u << 0)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD (1u << 5)
#define PLL_REFDIV 1u
#define PLL_FBDIV 125u
#define PLL_POSTDIV1 6u
#define PLL_POSTDIV2 2u

/* WATCHDOG tick (4.7.2), TIMER (4.6) */
#define WATCHDOG_TICK REG(0x40058000u + 0x2cu)
#define WATCHDOG_TICK_ENABLE (1u << 9)
#define TICK_CYCLES_12MHZ 12u
#define TIMER_TIMERAWL REG(0x40054000u + 0x28u)

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

/* 2.18.3 순서: 분주값 → 전원·VCO 켬 → LOCK 대기 → post divider 켬 */
static void pll_sys_start(void) {
    reset_block(RESETS_PLL_SYS);
    PLL_SYS_CS = PLL_REFDIV;
    PLL_SYS_FBDIV_INT = PLL_FBDIV;
    PLL_SYS_PWR_CLR = PLL_PWR_PD | PLL_PWR_VCOPD;
    while (!(PLL_SYS_CS & PLL_CS_LOCK)) {
    }
    PLL_SYS_PRIM = (PLL_POSTDIV1 << 16) | (PLL_POSTDIV2 << 12);
    PLL_SYS_PWR_CLR = PLL_PWR_POSTDIVPD;
}

/* AUXSRC는 바꿀 때 글리치가 나므로 SRC를 clk_ref에 둔 상태에서 바꾼다 */
static void clk_sys_from_pll(void) {
    CLK_SYS_CTRL = CLK_SYS_SRC_REF;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_REF)) {
    }
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_REF;
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_AUX;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_AUX)) {
    }
}

/* clk_ref가 XOSC여야 FC0의 기준값 12000kHz가 맞다 */
static uint32_t measure_khz(uint32_t source) {
    while (FC0_STATUS & FC0_STATUS_RUNNING) {
    }
    FC0_REF_KHZ = XOSC_KHZ;
    FC0_INTERVAL = 10u;
    FC0_MIN_KHZ = 0u;
    FC0_MAX_KHZ = 0x1ffffffu;
    FC0_SRC = source;
    while (!(FC0_STATUS & FC0_STATUS_DONE)) {
    }
    return FC0_RESULT >> FC0_RESULT_KHZ_LSB;
}

static void report(const char *name, uint32_t source) {
    uart_puts(name);
    uart_puts(" ");
    uart_putdec(measure_khz(source));
    uart_puts(" kHz\r\n");
}

static void timer_start(void) {
    WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | TICK_CYCLES_12MHZ;
    reset_block(RESETS_TIMER);
}

static void sleep_us(uint32_t us) {
    uint32_t start = TIMER_TIMERAWL;
    while (TIMER_TIMERAWL - start < us) {
    }
}

int main(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init();
    xosc_start();
    clk_peri_from_xosc();
    uart_init();
    clk_ref_from_xosc();

    uart_puts("before\r\n");
    report("rosc   ", FC0_SRC_ROSC);
    report("xosc   ", FC0_SRC_XOSC);
    report("clk_sys", FC0_SRC_CLK_SYS);

    pll_sys_start();
    clk_sys_from_pll();
    timer_start();

    uart_puts("after\r\n");
    report("pll_sys ", FC0_SRC_PLL_SYS);
    report("clk_sys ", FC0_SRC_CLK_SYS);
    report("clk_ref ", FC0_SRC_CLK_REF);
    report("clk_peri", FC0_SRC_CLK_PERI);

    uint32_t seconds = 0;
    while (1) {
        uart_puts("t=");
        uart_putdec(seconds);
        uart_puts(" s\r\n");
        SIO_GPIO_OUT_XOR = 1u << LED_RED;
        sleep_us(1000000u);
        seconds++;
    }
}
