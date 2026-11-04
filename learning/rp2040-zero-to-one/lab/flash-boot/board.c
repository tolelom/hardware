#include "board.h"

/* XOSC (2.16) */
#define XOSC_CTRL REG(0x40024000u)
#define XOSC_STATUS REG(0x40024004u)
#define XOSC_CTRL_ENABLE (0xfabu << 12)
#define XOSC_CTRL_RANGE_1_15MHZ 0xaa0u
#define XOSC_STATUS_STABLE (1u << 31)

#define CLK_REF_SRC_XOSC 2u
#define CLK_SYS_SRC_REF 0u
#define CLK_SYS_SRC_AUX 1u
#define CLK_SYS_AUXSRC_PLL_SYS (0u << 5)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

/* PLL (2.18). SYS: 12MHz x 125 = 1500MHz / 6 / 2 = 125MHz,
   USB: 12MHz x 100 = 1200MHz / 5 / 5 = 48MHz */
#define PLL_SYS_BASE 0x40028000u
#define PLL_USB_BASE 0x4002c000u
#define PLL_CS(base) REG((base) + 0x0u)
#define PLL_PWR_CLR(base) REG((base) + ALIAS_CLR + 0x4u)
#define PLL_FBDIV_INT(base) REG((base) + 0x8u)
#define PLL_PRIM(base) REG((base) + 0xcu)
#define CLK_AUXSRC_PLL_USB (0u << 5)  /* CLK_USB_CTRL, CLK_ADC_CTRL 공통 */
#define PLL_CS_LOCK (1u << 31)
#define PLL_PWR_PD (1u << 0)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD (1u << 5)

/* UART0 (4.2) */
#define UART0_IBRD REG(UART0_BASE + 0x024u)
#define UART0_FBRD REG(UART0_BASE + 0x028u)
#define UART0_LCR_H REG(UART0_BASE + 0x02cu)
#define UART0_CR REG(UART0_BASE + 0x030u)
#define UART_LCR_H_WLEN_8 (3u << 5)
#define UART_LCR_H_FEN (1u << 4)
#define UART_CR_UARTEN (1u << 0)
#define UART_CR_TXE (1u << 8)
#define UART_CR_RXE (1u << 9)
#define UART_IMSC_RXIM (1u << 4)
#define UART_IMSC_RTIM (1u << 6)
#define UART_IBRD_115200 6u   /* 12MHz / (16 x 115200) = 6.5104 */
#define UART_FBRD_115200 33u  /* 0.5104 x 64 + 0.5 = 33.2 */
#define UART0_IRQ 20u
#define UART_TX_PIN 0u
#define UART_RX_PIN 1u

/* WATCHDOG tick (4.7.2): clk_ref 12MHz / 12 = 1us */
#define WATCHDOG_TICK REG(0x40058000u + 0x2cu)
#define WATCHDOG_TICK_ENABLE (1u << 9)

/* bootrom 함수 표 (2.8.3) */
#define ROM_FUNC_TABLE_PTR 0x00000014u
#define ROM_TABLE_LOOKUP_PTR 0x00000018u

void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
    }
}

void unreset(uint32_t blocks) {
    RESETS_RESET_CLR = blocks;
    while ((RESETS_RESET_DONE & blocks) != blocks) {
    }
}

void reset_block(uint32_t blocks) {
    RESETS_RESET_SET = blocks;
    unreset(blocks);
}

static void xosc_start(void) {
    XOSC_CTRL = XOSC_CTRL_ENABLE | XOSC_CTRL_RANGE_1_15MHZ;
    while (!(XOSC_STATUS & XOSC_STATUS_STABLE)) {
    }
}

/* 2.18.3 순서: 분주값 → 전원·VCO 켬 → LOCK 대기 → post divider 켬 */
static void pll_start(uint32_t base, uint32_t reset_bit, uint32_t fbdiv,
                      uint32_t postdiv1, uint32_t postdiv2) {
    reset_block(reset_bit);
    PLL_CS(base) = 1u;
    PLL_FBDIV_INT(base) = fbdiv;
    PLL_PWR_CLR(base) = PLL_PWR_PD | PLL_PWR_VCOPD;
    while (!(PLL_CS(base) & PLL_CS_LOCK)) {
    }
    PLL_PRIM(base) = (postdiv1 << 16) | (postdiv2 << 12);
    PLL_PWR_CLR(base) = PLL_PWR_POSTDIVPD;
}

/* aux mux만 있는 클럭: 끄고, 소스 클럭 2사이클 이상 기다린 뒤 바꾸고 켠다 (2.15.3) */
static void clk_aux_switch(volatile uint32_t *ctrl, uint32_t auxsrc) {
    *ctrl = 0u;
    delay(10);
    *ctrl = auxsrc;
    *ctrl = auxsrc | CLK_GEN_ENABLE;
}

void clocks_48mhz_init(void) {
    pll_start(PLL_USB_BASE, RESETS_PLL_USB, 100u, 5u, 5u);
    clk_aux_switch(&CLK_USB_CTRL, CLK_AUXSRC_PLL_USB);
    clk_aux_switch(&CLK_ADC_CTRL, CLK_AUXSRC_PLL_USB);
}

/* 2.15.3: glitchless mux는 SELECTED를 기다리고, aux mux만 있으면 끈 뒤 바꾼다 */
static void clocks_init(void) {
    xosc_start();

    CLK_REF_CTRL = CLK_REF_SRC_XOSC;
    while (CLK_REF_SELECTED != (1u << CLK_REF_SRC_XOSC)) {
    }

    pll_start(PLL_SYS_BASE, RESETS_PLL_SYS, 125u, 6u, 2u);

    CLK_SYS_CTRL = CLK_SYS_SRC_REF;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_REF)) {
    }
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_REF;
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_AUX;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_AUX)) {
    }

    clk_aux_switch(&CLK_PERI_CTRL, CLK_PERI_AUXSRC_XOSC);
}

static void uart_init(void) {
    reset_block(RESETS_UART0);
    UART0_IBRD = UART_IBRD_115200;
    UART0_FBRD = UART_FBRD_115200;
    UART0_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;
    UART0_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    GPIO_CTRL(UART_TX_PIN) = FUNCSEL_UART;
    GPIO_CTRL(UART_RX_PIN) = FUNCSEL_UART;
    UART0_IMSC = UART_IMSC_RXIM | UART_IMSC_RTIM;
    NVIC_ICPR = 1u << UART0_IRQ;
    NVIC_ISER = 1u << UART0_IRQ;
}

void uart_putc(char c) {
    while (UART0_FR & UART_FR_TXFF) {
    }
    UART0_DR = (uint32_t) (uint8_t) c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

/* TXFE는 FIFO가 비었다는 뜻일 뿐이라, 마지막 바이트가 시프트 레지스터를
   다 나갈 때까지 BUSY를 기다린다 (4.2 UARTFR) */
void uart_flush(void) {
    while (UART0_FR & UART_FR_BUSY) {
    }
}

void uart_puthex(uint32_t value, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xfu]);
    }
}

/*
 * SIO 하드웨어 나눗셈기 (2.3.1.5). 피연산자를 쓰면 8사이클 뒤 결과가 나온다.
 * gcc는 M0+에서 / 와 %를 아래 AEABI 함수 호출로 바꾸므로, 이 정의가 있으면
 * -nostdlib에서도 / 와 %를 쓸 수 있다. 몫을 마지막에 읽는다 (SDK 권장).
 * 인터럽트 핸들러에서 나눗셈을 쓰면 상태를 저장·복원해야 하므로 핸들러에서는 쓰지 않는다.
 */
#define SIO_DIV_UDIVIDEND REG(SIO_BASE + 0x060u)
#define SIO_DIV_UDIVISOR REG(SIO_BASE + 0x064u)
#define SIO_DIV_SDIVIDEND REG(SIO_BASE + 0x068u)
#define SIO_DIV_SDIVISOR REG(SIO_BASE + 0x06cu)
#define SIO_DIV_QUOTIENT REG(SIO_BASE + 0x070u)
#define SIO_DIV_REMAINDER REG(SIO_BASE + 0x074u)
#define SIO_DIV_CSR REG(SIO_BASE + 0x078u)
#define SIO_DIV_CSR_READY (1u << 0)

uint64_t __aeabi_uidivmod(uint32_t n, uint32_t d) {
    SIO_DIV_UDIVIDEND = n;
    SIO_DIV_UDIVISOR = d;
    while (!(SIO_DIV_CSR & SIO_DIV_CSR_READY)) {
    }
    uint32_t r = SIO_DIV_REMAINDER;
    uint32_t q = SIO_DIV_QUOTIENT;
    return (uint64_t) q | ((uint64_t) r << 32);  /* r0 = 몫, r1 = 나머지 */
}

uint32_t __aeabi_uidiv(uint32_t n, uint32_t d) {
    return (uint32_t) __aeabi_uidivmod(n, d);
}

uint64_t __aeabi_idivmod(int32_t n, int32_t d) {
    SIO_DIV_SDIVIDEND = (uint32_t) n;
    SIO_DIV_SDIVISOR = (uint32_t) d;
    while (!(SIO_DIV_CSR & SIO_DIV_CSR_READY)) {
    }
    uint32_t r = SIO_DIV_REMAINDER;
    uint32_t q = SIO_DIV_QUOTIENT;
    return (uint64_t) q | ((uint64_t) r << 32);
}

int32_t __aeabi_idiv(int32_t n, int32_t d) {
    return (int32_t) (uint32_t) __aeabi_idivmod(n, d);
}

void uart_putdec(uint32_t value) {
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char) ('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (n) {
        uart_putc(digits[--n]);
    }
}

uint32_t time_us(void) {
    return TIMER_TIMERAWL;
}

void sleep_us(uint32_t us) {
    uint32_t start = TIMER_TIMERAWL;
    while (TIMER_TIMERAWL - start < us) {
    }
}

typedef void *(*rom_table_lookup_fn)(uint16_t *table, uint32_t code);
typedef void (*rom_reset_to_usb_boot_fn)(uint32_t gpio_activity_pin_mask,
                                         uint32_t disable_interface_mask);

void *rom_func_lookup(uint32_t code) {
    uint16_t *table = (uint16_t *) (uintptr_t) *(volatile uint16_t *) ROM_FUNC_TABLE_PTR;
    rom_table_lookup_fn lookup =
        (rom_table_lookup_fn) (uintptr_t) *(volatile uint16_t *) ROM_TABLE_LOOKUP_PTR;
    return lookup(table, code);
}

/* bootrom _reset_to_usb_boot ('U','B', 2.8.3). watchdog으로 리셋되어 돌아오지 않는다 */
void reset_to_bootsel(void) {
    uart_puts("reboot to BOOTSEL\r\n");
    uart_flush();
    rom_reset_to_usb_boot_fn reset =
        (rom_reset_to_usb_boot_fn) rom_func_lookup(ROM_CODE('U', 'B'));
    reset(0u, 0u);
}

/* UART0 수신 인터럽트. "BOOTSEL"이 연속으로 들어오면 BOOTSEL 모드로 재부팅 */
void isr_irq20(void) {
    static const char magic[] = "BOOTSEL";
    static unsigned matched;
    while (!(UART0_FR & UART_FR_RXFE)) {
        char c = (char) (UART0_DR & 0xffu);
        if (c == magic[matched]) {
            matched++;
            if (magic[matched] == '\0') {
                reset_to_bootsel();
            }
        } else {
            matched = (c == magic[0]) ? 1u : 0u;
        }
    }
    UART0_ICR = UART_IMSC_RXIM | UART_IMSC_RTIM;
}

void led_init(uint32_t pin) {
    GPIO_CTRL(pin) = FUNCSEL_SIO;
    SIO_GPIO_OUT_SET = 1u << pin;  /* active low: high = 꺼짐 */
    SIO_GPIO_OE_SET = 1u << pin;
}

/* 리셋 직후 패드는 pull-down이 켜져 있어(PADS PDE 리셋값 1), 구동하지 않은
   active low LED가 희미하게 켜진다. 세 색을 high로 구동해 꺼 둔다 */
void board_init(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init(LED_GREEN);
    led_init(LED_RED);
    led_init(LED_BLUE);
    clocks_init();
    WATCHDOG_TICK = WATCHDOG_TICK_ENABLE | 12u;
    reset_block(RESETS_TIMER);
    uart_init();
}
