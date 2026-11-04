/*
 * 코어마다 따로 있는 NVIC (데이터시트 2.3.2, 2.3.1.4).
 * core1이 자기 NVIC에서 SIO_IRQ_PROC1(IRQ 16)과 TIMER_IRQ_1을 켜고, core0은 켜지 않는다.
 * - FIFO: core0이 값 10개를 보내면 core1의 IRQ 16 핸들러가 꺼내 더한다.
 * - TIMER 알람 1: core1 핸들러가 1ms마다 다시 걸어 100번 받는다.
 * - TIMER 알람 2(IRQ 2): 두 코어가 모두 켜고, 알람 한 번에 어느 코어 핸들러가 몇 번 도는지 센다.
 * 핸들러는 SIO CPUID로 어느 코어에서 돌았는지 나눠 센다.
 */
#include "board.h"

#define M0PLUS_VTOR REG(0xe000ed08u)
#define SIO_FIFO_ST REG(SIO_BASE + 0x050u)
#define SIO_FIFO_WR REG(SIO_BASE + 0x054u)
#define SIO_FIFO_RD REG(SIO_BASE + 0x058u)
#define FIFO_ST_VLD (1u << 0)
#define FIFO_ST_RDY (1u << 1)
#define TIMER_ALARM(n) REG(0x40054000u + 0x10u + (n) * 4u)
#define TIMER_INTR REG(0x40054000u + 0x34u)
#define TIMER_INTE REG(0x40054000u + 0x38u)
#define IRQ_TIMER1 1u
#define IRQ_TIMER2 2u
#define IRQ_SIO_PROC1 16u
#define CORE1_STACK_WORDS 256u
#define ALARM1_TIMES 100u
#define ALARM2_TIMES 10u

static uint32_t core1_stack[CORE1_STACK_WORDS] __attribute__((aligned(8)));
static volatile uint32_t fifo_count[2];
static volatile uint32_t fifo_sum;
static volatile uint32_t alarm1_count[2];
static volatile uint32_t alarm2_count[2];
static volatile uint32_t alarm2_saw_flag[2];
static volatile uint32_t core1_iser;
static volatile uint32_t core1_ready;
static volatile uint32_t core1_fifo_st_at_start;

void isr_irq16(void) {
    uint32_t core = SIO_CPUID;
    while (SIO_FIFO_ST & FIFO_ST_VLD) {
        fifo_sum += SIO_FIFO_RD;
        fifo_count[core]++;
    }
    SIO_FIFO_ST = 0xfu;
}

void isr_irq1(void) {
    uint32_t core = SIO_CPUID;
    TIMER_INTR = 1u << 1;
    alarm1_count[core]++;
    if (alarm1_count[core] < ALARM1_TIMES) {
        TIMER_ALARM(1) = TIMER_TIMERAWL + 1000u;
    }
}

/* 두 코어가 같은 IRQ를 받는다. INTR 비트를 본 뒤 지운다 */
void isr_irq2(void) {
    uint32_t core = SIO_CPUID;
    alarm2_count[core]++;
    if (TIMER_INTR & (1u << 2)) {
        alarm2_saw_flag[core]++;
    }
    TIMER_INTR = 1u << 2;
}

static void core1_main(void) {
    core1_fifo_st_at_start = SIO_FIFO_ST;
    SIO_FIFO_ST = 0xfu; /* 쓰면 WOF, ROE가 지워진다 */
    NVIC_ICPR = (1u << IRQ_SIO_PROC1) | (1u << IRQ_TIMER1) | (1u << IRQ_TIMER2);
    NVIC_ISER = (1u << IRQ_SIO_PROC1) | (1u << IRQ_TIMER1) | (1u << IRQ_TIMER2);
    core1_iser = NVIC_ISER;
    TIMER_ALARM(1) = TIMER_TIMERAWL + 1000u;
    core1_ready = 1;
    while (1) {
        __asm volatile("wfi");
    }
}

static void fifo_drain(void) {
    while (SIO_FIFO_ST & FIFO_ST_VLD) {
        (void) SIO_FIFO_RD;
    }
}

static void fifo_push(uint32_t value) {
    while (!(SIO_FIFO_ST & FIFO_ST_RDY)) {
    }
    SIO_FIFO_WR = value;
    __asm volatile("sev");
}

static uint32_t fifo_pop(void) {
    while (!(SIO_FIFO_ST & FIFO_ST_VLD)) {
        __asm volatile("wfe");
    }
    return SIO_FIFO_RD;
}

static void core1_launch(uint32_t vector_table, uint32_t sp, void (*entry)(void)) {
    const uint32_t sequence[] = {
        0u, 0u, 1u, vector_table, sp, (uint32_t) entry,
    };
    unsigned step = 0;
    while (step < sizeof sequence / sizeof sequence[0]) {
        uint32_t command = sequence[step];
        if (command == 0u) {
            fifo_drain();
            __asm volatile("sev");
        }
        fifo_push(command);
        step = (fifo_pop() == command) ? step + 1u : 0u;
    }
}

static void print_pair(const char *label, volatile uint32_t *pair) {
    uart_puts(label);
    uart_puts("core0 ");
    uart_putdec(pair[0]);
    uart_puts(", core1 ");
    uart_putdec(pair[1]);
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    TIMER_INTE |= (1u << 1) | (1u << 2);
    uart_puts("core1 irq start\r\n");
    core1_launch(M0PLUS_VTOR, (uint32_t) &core1_stack[CORE1_STACK_WORDS], core1_main);
    while (!core1_ready) {
    }
    uart_puts("core1 FIFO_ST at start 0x");
    uart_puthex(core1_fifo_st_at_start, 1);
    uart_puts("\r\n");
    uart_puts("NVIC_ISER read on core0 0x");
    uart_puthex(NVIC_ISER, 8);
    uart_puts(", on core1 0x");
    uart_puthex(core1_iser, 8);
    uart_puts("\r\n");

    for (uint32_t i = 1; i <= 10u; i++) {
        fifo_push(i);
        sleep_us(100u);
    }
    sleep_us(1000u);
    print_pair("fifo values handled: ", fifo_count);
    uart_puts("fifo sum ");
    uart_putdec(fifo_sum);
    uart_puts(" (55 expected)\r\n");

    sleep_us(150000u);
    print_pair("alarm1 handled:      ", alarm1_count);

    NVIC_ICPR = 1u << IRQ_TIMER2;
    NVIC_ISER = 1u << IRQ_TIMER2;
    for (uint32_t i = 0; i < ALARM2_TIMES; i++) {
        TIMER_ALARM(2) = TIMER_TIMERAWL + 500u;
        sleep_us(2000u);
    }
    print_pair("alarm2 entries:      ", alarm2_count);
    print_pair("alarm2 saw INTR set: ", alarm2_saw_flag);
    uart_puts("done\r\n");
    while (1) {
    }
}
