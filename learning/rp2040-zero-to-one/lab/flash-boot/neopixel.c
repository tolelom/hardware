/*
 * 보드의 NeoPixel(RGB1)을 PIO0 SM0으로 구동한다. 회로도 v1.3: DIN = GPIO12,
 * VCC = GPIO11(NEO_PWR). LED 전원을 GPIO가 직접 주므로(최대 12mA) 밝기를 낮게 둔다.
 * PIO 프로그램은 pico-sdk pico_status_led/ws2812.pio(T1 3, T2 3, T3 4)를
 * 손으로 인코딩했다(데이터시트 3.4 Table 366).
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
#define RESETS_PIO0 (1u << 10)
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
#define FUNCSEL_PIO0 6u

/* PADS_BANK0 (2.19.6.3) */
#define PADS_BANK0_GPIO(n) REG(0x4001c000u + 0x04u + (n) * 0x4u)
#define PAD_IE (1u << 6)
#define PAD_DRIVE_12MA (3u << 4)

/* SIO (2.3.1) */
#define SIO_BASE 0xd0000000u
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x1cu)
#define SIO_GPIO_OE_SET REG(SIO_BASE + 0x24u)

/* PIO0 (3.7) */
#define PIO0_BASE 0x50200000u
#define PIO0_CTRL REG(PIO0_BASE + 0x000u)
#define PIO0_FSTAT REG(PIO0_BASE + 0x004u)
#define PIO0_FDEBUG REG(PIO0_BASE + 0x008u)
#define PIO0_TXF0 REG(PIO0_BASE + 0x010u)
#define PIO0_INSTR_MEM(n) REG(PIO0_BASE + 0x048u + (n) * 0x4u)
#define PIO0_SM0_CLKDIV REG(PIO0_BASE + 0x0c8u)
#define PIO0_SM0_EXECCTRL REG(PIO0_BASE + 0x0ccu)
#define PIO0_SM0_SHIFTCTRL REG(PIO0_BASE + 0x0d0u)
#define PIO0_SM0_INSTR REG(PIO0_BASE + 0x0d8u)
#define PIO0_SM0_PINCTRL REG(PIO0_BASE + 0x0dcu)
#define CTRL_SM0_ENABLE (1u << 0)
#define FSTAT_TXFULL_SM0 (1u << 16)
#define FDEBUG_TXSTALL_SM0 (1u << 24)

#define NEO_PWR_PIN 11u
#define NEO_DATA_PIN 12u
#define NEO_LEVEL 0x10u

/* 125MHz / 15.625 = 8MHz, 비트당 10사이클 = 800kHz */
#define WS2812_CLKDIV_INT 15u
#define WS2812_CLKDIV_FRAC 160u
#define WS2812_WRAP_TOP 3u
#define WS2812_BITS 24u


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

static void timer_start(void) {
    WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | TICK_CYCLES_12MHZ;
    reset_block(RESETS_TIMER);
}

static void sleep_us(uint32_t us) {
    uint32_t start = TIMER_TIMERAWL;
    while (TIMER_TIMERAWL - start < us) {
    }
}

/*
 * .side_set 1, wrap 0~3
 * 0: out x, 1        side 0 [3]
 * 1: jmp !x 3        side 1 [2]
 * 2: jmp 0           side 1 [2]
 * 3: nop (mov y, y)  side 0 [2]
 */
static const uint16_t ws2812_program[] = {0x6321u, 0x1223u, 0x1200u, 0xa242u};
#define PIO_SET_PINDIRS_1 0xe081u
#define PIO_JMP_0 0x0000u

static void neopixel_power_on(void) {
    PADS_BANK0_GPIO(NEO_PWR_PIN) = PAD_IE | PAD_DRIVE_12MA;
    GPIO_CTRL(NEO_PWR_PIN) = FUNCSEL_SIO;
    SIO_GPIO_OUT_SET = 1u << NEO_PWR_PIN;
    SIO_GPIO_OE_SET = 1u << NEO_PWR_PIN;
}

static void ws2812_init(void) {
    reset_block(RESETS_PIO0);
    for (unsigned i = 0; i < sizeof ws2812_program / sizeof ws2812_program[0]; i++) {
        PIO0_INSTR_MEM(i) = ws2812_program[i];
    }
    PIO0_SM0_CLKDIV = (WS2812_CLKDIV_INT << 16) | (WS2812_CLKDIV_FRAC << 8);
    PIO0_SM0_EXECCTRL = (WS2812_WRAP_TOP << 12) | (0u << 7);
    /* TX FIFO 8칸, 24비트마다 autopull, 왼쪽(MSB부터) 시프트 */
    PIO0_SM0_SHIFTCTRL = (1u << 30) | (WS2812_BITS << 25) | (1u << 17);
    /* side-set 1비트 = GPIO12. SET도 GPIO12로 두어 방향 설정에 쓴다 */
    PIO0_SM0_PINCTRL = (1u << 29) | (1u << 26) | (NEO_DATA_PIN << 10)
                       | (NEO_DATA_PIN << 5);
    PIO0_SM0_INSTR = PIO_SET_PINDIRS_1;
    PIO0_SM0_INSTR = PIO_JMP_0;
    GPIO_CTRL(NEO_DATA_PIN) = FUNCSEL_PIO0;
    PIO0_CTRL = CTRL_SM0_ENABLE;
}

/* WS2812는 G, R, B 순서로 MSB부터 받는다. OSR 상위 24비트에 둔다 */
static void ws2812_put(uint32_t red, uint32_t green, uint32_t blue) {
    while (PIO0_FSTAT & FSTAT_TXFULL_SM0) {
    }
    PIO0_TXF0 = (green << 24) | (red << 16) | (blue << 8);
}

struct color {
    const char *name;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
};

static const struct color colors[] = {
    {"red", NEO_LEVEL, 0u, 0u},
    {"green", 0u, NEO_LEVEL, 0u},
    {"blue", 0u, 0u, NEO_LEVEL},
    {"white", NEO_LEVEL, NEO_LEVEL, NEO_LEVEL},
    {"off", 0u, 0u, 0u},
};

int main(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init();
    xosc_start();
    clk_peri_from_xosc();
    uart_init();
    clk_ref_from_xosc();
    pll_sys_start();
    clk_sys_from_pll();
    timer_start();

    neopixel_power_on();
    sleep_us(10000u);
    ws2812_init();
    uart_puts("ws2812 ready\r\n");

    unsigned index = 0;
    while (1) {
        const struct color *c = &colors[index];
        PIO0_FDEBUG = FDEBUG_TXSTALL_SM0;
        ws2812_put(c->red, c->green, c->blue);
        sleep_us(1000u);
        uart_puts(c->name);
        uart_puts((PIO0_FDEBUG & FDEBUG_TXSTALL_SM0) ? ", sent\r\n"
                                                     : ", not sent\r\n");
        SIO_GPIO_OUT_XOR = 1u << LED_RED;
        sleep_us(1000000u);
        index = (index + 1u == sizeof colors / sizeof colors[0]) ? 0u : index + 1u;
    }
}
