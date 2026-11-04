/*
 * GPIO 인터럽트와 NVIC 우선순위 (데이터시트 2.19.3, 2.3.2).
 * 엣지는 파랑 LED 핀(GPIO25)을 SIO 출력으로 0/1로 바꿔 만들고, 같은 핀의 입력을 본다.
 * LED는 active low라 0인 동안 잠깐 켜진다.
 *
 * A0: GPIO26 CTRL의 INOVER로 주변장치 쪽 입력만 바꾸면 INTR에 반영되는지 본다.
 * A: 인터럽트를 켜지 않고 INTR3의 GPIO25 비트 4개를 본다. 엣지는 래치되고 레벨은 아니다.
 * B: EDGE_HIGH 인터럽트(IO_IRQ_BANK0, IRQ 13)의 진입 지연을 SysTick 클럭 수로 잰다.
 * C: GPIO 핸들러 안에서 IRQ 0을 NVIC_ISPR로 대기시키고, 우선순위에 따라 끼어드는지 본다.
 */
#include "board.h"

#define PIN 25u
#define PIN_OVER 26u
#define GPIO_STATUS(n) REG(IO_BANK0 + (n) * 0x8u)
#define IO_BANK0 0x40014000u
#define IO_INTR3 REG(IO_BANK0 + 0x0fcu)
#define IO_PROC0_INTE3 REG(IO_BANK0 + 0x10cu)
#define PIN_SHIFT ((PIN % 8u) * 4u)
#define BIT_EDGE_HIGH (1u << (PIN_SHIFT + 3u))
#define BITS_ALL (0xfu << PIN_SHIFT)
#define INOVER_HIGH (3u << 16)
#define PAD_IE (1u << 6)
#define PAD_PDE (1u << 2)

#define SYST_CSR REG(0xe000e010u)
#define SYST_RVR REG(0xe000e014u)
#define SYST_CVR REG(0xe000e018u)
#define NVIC_ISPR REG(0xe000e200u)
#define NVIC_IPR(n) REG(0xe000e400u + (n) * 4u)
#define IRQ_TIMER0 0u
#define IRQ_BANK0 13u

static volatile uint32_t cvr_isr;
static volatile uint32_t entries;
static char order[16];
static volatile uint32_t order_len;
static volatile int pend_timer_in_gpio;

static void mark(char c) {
    if (order_len < sizeof order - 1u) {
        order[order_len++] = c;
    }
}

void isr_irq13(void) {
    cvr_isr = SYST_CVR;
    entries++;
    IO_INTR3 = BITS_ALL;
    mark('G');
    if (pend_timer_in_gpio) {
        NVIC_ISPR = 1u << IRQ_TIMER0;
        for (volatile int i = 0; i < 100; i++) {
        }
    }
    mark('g');
}

void isr_irq0(void) {
    mark('T');
}

static void set_priority(uint32_t irq, uint32_t level) {
    uint32_t shift = (irq % 4u) * 8u + 6u;
    NVIC_IPR(irq / 4u) = (NVIC_IPR(irq / 4u) & ~(3u << shift)) | (level << shift);
}

static void pin_low(void) {
    SIO_GPIO_OUT_CLR = 1u << PIN;
}

static void pin_high(void) {
    SIO_GPIO_OUT_SET = 1u << PIN;
}

static void print_bits(const char *label) {
    uint32_t v = (IO_INTR3 & BITS_ALL) >> PIN_SHIFT;
    uart_puts(label);
    uart_puts(": EDGE_HIGH ");
    uart_putdec((v >> 3) & 1u);
    uart_puts(" EDGE_LOW ");
    uart_putdec((v >> 2) & 1u);
    uart_puts(" LEVEL_HIGH ");
    uart_putdec((v >> 1) & 1u);
    uart_puts(" LEVEL_LOW ");
    uart_putdec(v & 1u);
    uart_puts("\r\n");
}

static void print_over(const char *label) {
    uint32_t s = GPIO_STATUS(PIN_OVER);
    uint32_t v = (IO_INTR3 >> ((PIN_OVER % 8u) * 4u)) & 0xfu;
    uart_puts(label);
    uart_puts(": INFROMPAD ");
    uart_putdec((s >> 17) & 1u);
    uart_puts(" INTOPERI ");
    uart_putdec((s >> 19) & 1u);
    uart_puts(", INTR EDGE_HIGH ");
    uart_putdec((v >> 3) & 1u);
    uart_puts(" LEVEL_HIGH ");
    uart_putdec((v >> 1) & 1u);
    uart_puts("\r\n");
}

static void part_a0(void) {
    PADS_GPIO(PIN_OVER) = PAD_IE | PAD_PDE;
    GPIO_CTRL(PIN_OVER) = FUNCSEL_NULL;
    IO_INTR3 = 0xfu << ((PIN_OVER % 8u) * 4u);
    print_over("A0 GPIO26 no override ");
    GPIO_CTRL(PIN_OVER) = FUNCSEL_NULL | INOVER_HIGH;
    print_over("A0 GPIO26 INOVER high ");
    GPIO_CTRL(PIN_OVER) = FUNCSEL_NULL;
}

static void part_a(void) {
    pin_low();
    IO_INTR3 = BITS_ALL;
    print_bits("A low, cleared      ");
    pin_high();
    print_bits("A high              ");
    pin_low();
    print_bits("A low again         ");
    IO_INTR3 = BITS_ALL;
    print_bits("A cleared           ");
    /* 연속한 두 store로 만든 짧은 펄스 */
    SIO_GPIO_OUT_SET = 1u << PIN;
    SIO_GPIO_OUT_CLR = 1u << PIN;
    print_bits("A short pulse       ");
    IO_INTR3 = BITS_ALL;
}

static void part_b(void) {
    SYST_RVR = 0xffffffu;
    SYST_CVR = 0;
    SYST_CSR = 5u; /* ENABLE, CLKSOURCE 프로세서 클럭, 인터럽트 없음 */
    IO_INTR3 = BITS_ALL;
    IO_PROC0_INTE3 = BIT_EDGE_HIGH;
    NVIC_ICPR = 1u << IRQ_BANK0;
    NVIC_ISER = 1u << IRQ_BANK0;
    for (int i = 0; i < 8; i++) {
        pin_low();
        uint32_t before_entries = entries;
        __asm volatile("cpsid i");
        uint32_t cvr_before = SYST_CVR;
        SIO_GPIO_OUT_SET = 1u << PIN;
        __asm volatile("cpsie i");
        while (entries == before_entries) {
        }
        uint32_t cycles = (cvr_before - cvr_isr) & 0xffffffu;
        uart_puts("B edge to handler: ");
        uart_putdec(cycles);
        uart_puts(" cycles\r\n");
    }
    /* 인터럽트를 켠 채 store 바로 뒤에서 잰다 */
    for (int i = 0; i < 4; i++) {
        pin_low();
        uint32_t before_entries = entries;
        uint32_t cvr_before = SYST_CVR;
        SIO_GPIO_OUT_SET = 1u << PIN;
        while (entries == before_entries) {
        }
        uart_puts("B irq on, store to handler: ");
        uart_putdec((cvr_before - cvr_isr) & 0xffffffu);
        uart_puts(" cycles\r\n");
    }
    pin_low();
}

static void run_case(const char *label, uint32_t pri_gpio, uint32_t pri_timer,
                     int both_pending) {
    set_priority(IRQ_BANK0, pri_gpio);
    set_priority(IRQ_TIMER0, pri_timer);
    order_len = 0;
    NVIC_ICPR = (1u << IRQ_BANK0) | (1u << IRQ_TIMER0);
    NVIC_ISER = (1u << IRQ_BANK0) | (1u << IRQ_TIMER0);
    __asm volatile("cpsid i");
    if (both_pending) {
        pend_timer_in_gpio = 0;
        NVIC_ISPR = (1u << IRQ_BANK0) | (1u << IRQ_TIMER0);
    } else {
        pend_timer_in_gpio = 1;
        pin_low();
        pin_high();
    }
    __asm volatile("cpsie i");
    sleep_us(100u);
    order[order_len] = 0;
    uart_puts(label);
    uart_puts(order);
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    uart_puts("gpio irq start\r\n");
    part_a0();
    part_a();
    part_b();
    uart_puts("C order (G gpio enter, g gpio leave, T timer)\r\n");
    run_case("C gpio 3, timer 0, timer pended in gpio: ", 3u, 0u, 0);
    run_case("C gpio 0, timer 0, timer pended in gpio: ", 0u, 0u, 0);
    run_case("C gpio 0, timer 3, timer pended in gpio: ", 0u, 3u, 0);
    run_case("C gpio 0, timer 0, both pended:          ", 0u, 0u, 1);
    run_case("C gpio 0, timer 3, both pended:          ", 0u, 3u, 1);
    run_case("C gpio 3, timer 0, both pended:          ", 3u, 0u, 1);
    NVIC_ICER = (1u << IRQ_BANK0) | (1u << IRQ_TIMER0);
    uart_puts("done\r\n");
    while (1) {
    }
}
