/*
 * clk_sys 오버클럭과 코어 전압 (데이터시트 2.15, 2.18, 2.10).
 * 데이터시트 최대는 133MHz이고, VREG VSEL 1.15V에서 200MHz를 적는다 (2.15.3 NOTE).
 * 전압은 1.10V와 1.15V만 쓰고, 주파수는 표의 단계로 400MHz까지 올린다.
 *
 * 각 단계는 SRAM 함수 안에서만 돈다. clk_sys를 올리면 XIP SCK(clk_sys/2)도 같이 올라
 * flash가 못 따라갈 수 있기 때문이다. 단계 안에서 FC0로 clk_sys를 재고, xorshift 루프와
 * 16KB SRAM 쓰기·읽기를 돌린 뒤 125MHz로 내려와 결과를 보낸다.
 * 멈추면 watchdog(1초)이 리셋하고, 진행 중이던 단계 번호를 SCRATCH에 남겨 다음 부팅에서
 * 보고한 뒤 그 전압의 남은 단계를 건너뛴다. SCRATCH3에는 단계 안의 진행 위치를 적는다:
 * 1 시작, 2 PLL 잠김, 3 clk_sys를 PLL로 바꿈, 4 FC0 끝, 5 xorshift 끝, 6 메모리 검사 끝.
 */
#include "board.h"

#define VREG REG(0x40064000u)
#define VREG_VSEL_SHIFT 4u
#define VREG_VSEL_MASK (0xfu << VREG_VSEL_SHIFT)
#define VSEL_1_10 0xbu
#define VSEL_1_15 0xcu

#define PLL_SYS_BASE 0x40028000u
#define PLL_CS REG(PLL_SYS_BASE + 0x0u)
#define PLL_PWR REG(PLL_SYS_BASE + 0x4u)
#define PLL_PWR_CLR REG(PLL_SYS_BASE + ALIAS_CLR + 0x4u)
#define PLL_FBDIV_INT REG(PLL_SYS_BASE + 0x8u)
#define PLL_PRIM REG(PLL_SYS_BASE + 0xcu)
#define PLL_CS_LOCK (1u << 31)
#define PLL_PWR_PD (1u << 0)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD (1u << 5)
#define PLL_PWR_ALL_DOWN 0x2du

#define FC0_REF_KHZ REG(CLOCKS_BASE + 0x80u)
#define FC0_MIN_KHZ REG(CLOCKS_BASE + 0x84u)
#define FC0_MAX_KHZ REG(CLOCKS_BASE + 0x88u)
#define FC0_INTERVAL REG(CLOCKS_BASE + 0x90u)
#define FC0_SRC REG(CLOCKS_BASE + 0x94u)
#define FC0_STATUS REG(CLOCKS_BASE + 0x98u)
#define FC0_RESULT REG(CLOCKS_BASE + 0x9cu)
#define FC0_STATUS_DONE (1u << 4)
#define FC0_STATUS_RUNNING (1u << 8)
#define FC0_SRC_CLK_SYS 0x09u

#define WATCHDOG_BASE 0x40058000u
#define WATCHDOG_CTRL REG(WATCHDOG_BASE + 0x00u)
#define WATCHDOG_LOAD REG(WATCHDOG_BASE + 0x04u)
#define WATCHDOG_REASON REG(WATCHDOG_BASE + 0x08u)
#define WATCHDOG_SCRATCH(n) REG(WATCHDOG_BASE + 0x0cu + (n) * 4u)
#define WATCHDOG_CTRL_ENABLE (1u << 30)
#define PSM_WDSEL REG(0x40010000u + 0x08u)
#define PSM_WDSEL_NO_OSC 0x1fffcu
#define WATCHDOG_LOAD_1S 2000000u /* RP2040-E1: 틱당 2씩 줄어든다 */
#define MAGIC 0x0c10c000u

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))
#define WORK_LOOPS 1000000u
#define MEM_WORDS 4096u

struct step {
    uint32_t vsel;
    uint32_t mhz;
    uint32_t fbdiv;
    uint32_t postdiv1;
    uint32_t postdiv2;
};

/* VCO = 12MHz x fbdiv, 750~1600MHz (2.18.2) */
static const struct step steps[] = {
    {VSEL_1_10, 125, 125, 6, 2}, {VSEL_1_10, 150, 125, 5, 2},
    {VSEL_1_10, 200, 100, 6, 1}, {VSEL_1_10, 228, 114, 6, 1},
    {VSEL_1_10, 250, 125, 6, 1}, {VSEL_1_10, 276, 92, 4, 1},
    {VSEL_1_10, 300, 100, 4, 1}, {VSEL_1_10, 324, 108, 4, 1},
    {VSEL_1_10, 348, 116, 4, 1}, {VSEL_1_10, 372, 124, 4, 1},
    {VSEL_1_10, 400, 100, 3, 1},
    {VSEL_1_15, 250, 125, 6, 1}, {VSEL_1_15, 276, 92, 4, 1},
    {VSEL_1_15, 300, 100, 4, 1}, {VSEL_1_15, 324, 108, 4, 1},
    {VSEL_1_15, 348, 116, 4, 1}, {VSEL_1_15, 372, 124, 4, 1},
    {VSEL_1_15, 400, 100, 3, 1},
};
#define STEP_COUNT (sizeof steps / sizeof steps[0])

struct result {
    uint32_t khz;
    uint32_t work_us;
    uint32_t work_value;
    uint32_t mem_errors;
};

static uint32_t mem[MEM_WORDS];

RAMFUNC static void clk_sys_to_ref(void) {
    CLK_SYS_CTRL &= ~1u;
    while (CLK_SYS_SELECTED != 1u) {
    }
}

RAMFUNC static void clk_sys_to_pll(void) {
    CLK_SYS_CTRL |= 1u;
    while (CLK_SYS_SELECTED != 2u) {
    }
}

/* clk_sys가 clk_ref에 있는 동안 PLL_SYS를 다시 설정한다 (2.18.2 순서) */
RAMFUNC static void pll_set(uint32_t fbdiv, uint32_t postdiv1, uint32_t postdiv2) {
    PLL_PWR = PLL_PWR_ALL_DOWN;
    PLL_FBDIV_INT = fbdiv;
    PLL_PWR_CLR = PLL_PWR_PD | PLL_PWR_VCOPD;
    while (!(PLL_CS & PLL_CS_LOCK)) {
    }
    PLL_PRIM = (postdiv1 << 16) | (postdiv2 << 12);
    PLL_PWR_CLR = PLL_PWR_POSTDIVPD;
}

RAMFUNC static uint32_t fc0_khz(void) {
    while (FC0_STATUS & FC0_STATUS_RUNNING) {
    }
    FC0_REF_KHZ = XOSC_HZ / 1000u;
    FC0_INTERVAL = 10u;
    FC0_MIN_KHZ = 0u;
    FC0_MAX_KHZ = 0x1ffffffu;
    FC0_SRC = FC0_SRC_CLK_SYS;
    while (!(FC0_STATUS & FC0_STATUS_DONE)) {
    }
    return FC0_RESULT >> 5;
}

RAMFUNC static uint32_t work(void) {
    uint32_t x = 1u;
    for (uint32_t i = 0; i < WORK_LOOPS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        x += i;
    }
    return x;
}

RAMFUNC static uint32_t mem_test(void) {
    uint32_t x = 0x12345678u;
    for (uint32_t i = 0; i < MEM_WORDS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        mem[i] = x;
    }
    x = 0x12345678u;
    uint32_t errors = 0;
    for (uint32_t i = 0; i < MEM_WORDS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        errors += mem[i] != x;
    }
    return errors;
}

/* 진행 표시. 멈추면 다음 부팅에서 어디까지 갔는지 본다 */
#define PHASE(n) (WATCHDOG_SCRATCH(3) = (n))

RAMFUNC static void run_step(const struct step *s, struct result *r) {
    PHASE(1);
    clk_sys_to_ref();
    pll_set(s->fbdiv, s->postdiv1, s->postdiv2);
    PHASE(2);
    clk_sys_to_pll();
    PHASE(3);
    r->khz = fc0_khz();
    PHASE(4);
    uint32_t start = TIMER_TIMERAWL;
    r->work_value = work();
    r->work_us = TIMER_TIMERAWL - start;
    PHASE(5);
    r->mem_errors = mem_test();
    PHASE(6);
    clk_sys_to_ref();
    pll_set(125u, 6u, 2u);
    clk_sys_to_pll();
}

static void set_vsel(uint32_t vsel) {
    VREG = (VREG & ~VREG_VSEL_MASK) | (vsel << VREG_VSEL_SHIFT);
    sleep_us(10000u); /* 안정 대기. SDK 예제의 sleep_ms(10)과 같은 길이 */
}

static void print_voltage(uint32_t vsel) {
    uart_puts(vsel == VSEL_1_15 ? "1.15V" : "1.10V");
}

static uint32_t next_voltage_step(uint32_t i) {
    uint32_t vsel = steps[i].vsel;
    while (i < STEP_COUNT && steps[i].vsel == vsel) {
        i++;
    }
    return i;
}

int main(void) {
    board_init();
    uint32_t reason = WATCHDOG_REASON;
    uint32_t start_step = 0;
    if (WATCHDOG_SCRATCH(0) == MAGIC && WATCHDOG_SCRATCH(2) == ~WATCHDOG_SCRATCH(1)
        && WATCHDOG_SCRATCH(1) < STEP_COUNT && (reason & 1u)) {
        uint32_t hung = WATCHDOG_SCRATCH(1);
        uart_puts("watchdog reset during step: ");
        print_voltage(steps[hung].vsel);
        uart_puts(" ");
        uart_putdec(steps[hung].mhz);
        uart_puts(" MHz, phase ");
        uart_putdec(WATCHDOG_SCRATCH(3));
        uart_puts(", VREG now 0x");
        uart_puthex(VREG, 4);
        uart_puts("\r\n");
        start_step = next_voltage_step(hung);
    }
    WATCHDOG_SCRATCH(0) = 0;
    set_vsel(VSEL_1_10);

    /* 125MHz에서 기준값을 만든다 */
    uint32_t t0 = time_us();
    uint32_t reference = work();
    uint32_t reference_us = time_us() - t0;
    uart_puts("overclock start, reference 0x");
    uart_puthex(reference, 8);
    uart_puts(" in ");
    uart_putdec(reference_us);
    uart_puts(" us at 125 MHz\r\n");

    PSM_WDSEL = PSM_WDSEL_NO_OSC;
    for (uint32_t i = start_step; i < STEP_COUNT;) {
        const struct step *s = &steps[i];
        set_vsel(s->vsel);
        WATCHDOG_SCRATCH(1) = i;
        WATCHDOG_SCRATCH(2) = ~i;
        WATCHDOG_SCRATCH(0) = MAGIC;
        uart_flush();

        struct result r;
        WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
        WATCHDOG_LOAD = WATCHDOG_LOAD_1S;
        WATCHDOG_CTRL |= WATCHDOG_CTRL_ENABLE;
        __asm volatile("cpsid i");
        run_step(s, &r);
        __asm volatile("cpsie i");
        WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
        WATCHDOG_SCRATCH(0) = 0;

        int ok = r.work_value == reference && r.mem_errors == 0u;
        print_voltage(s->vsel);
        uart_puts(" ");
        uart_putdec(s->mhz);
        uart_puts(" MHz: fc0 ");
        uart_putdec(r.khz);
        uart_puts(" kHz, work ");
        uart_putdec(r.work_us);
        uart_puts(" us (");
        uart_putdec(r.work_us ? 125u * reference_us / r.work_us : 0u);
        uart_puts(" MHz equiv), value ");
        uart_puts(r.work_value == reference ? "ok" : "BAD");
        uart_puts(", mem errors ");
        uart_putdec(r.mem_errors);
        uart_puts(ok ? "\r\n" : "  FAIL\r\n");
        i = ok ? i + 1u : next_voltage_step(i);
    }
    set_vsel(VSEL_1_10);
    uart_puts("VREG back to 0x");
    uart_puthex(VREG, 4);
    uart_puts("\r\ndone\r\n");
    while (1) {
    }
}
