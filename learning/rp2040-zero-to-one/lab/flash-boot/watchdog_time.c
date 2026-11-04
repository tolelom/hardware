/*
 * watchdog CTRL.TIME 읽기 (데이터시트 4.7). watchdog.c에서 TIME이 LOAD와 경과 시간에
 * 관계없이 0x7a120으로 읽혔다. LOAD를 바꿔 가며 TIME과 TICK 레지스터를 시간에 따라 읽는다.
 * 결과: TIME에는 전원 투입 뒤 첫 LOAD가 남는다. PSM_WDSEL을 쓰지 않으므로 전원 직후 부팅에서는
 * 만료되어도 리셋되지 않는다.
 * RP2040-E1로 LOAD / 2 us 뒤 만료되므로 LOAD는 1.5초 이상으로 두고 100ms 뒤 ENABLE을 끈다.
 */
#include "board.h"

#define WATCHDOG_BASE 0x40058000u
#define WATCHDOG_CTRL REG(WATCHDOG_BASE + 0x00u)
#define WATCHDOG_LOAD REG(WATCHDOG_BASE + 0x04u)
#define WATCHDOG_TICK REG(WATCHDOG_BASE + 0x2cu)
#define WATCHDOG_CTRL_ENABLE (1u << 30)
#define WATCHDOG_CTRL_PAUSE (7u << 24)
#define WATCHDOG_REASON REG(WATCHDOG_BASE + 0x08u)
#define WATCHDOG_SCRATCH(n) REG(WATCHDOG_BASE + 0x0cu + (n) * 4u)
#define TIME_MASK 0xffffffu
#define MAGIC 0x71e0b007u

static void sample(const char *label) {
    uart_puts(label);
    uart_puts(" CTRL 0x");
    uart_puthex(WATCHDOG_CTRL, 8);
    uart_puts(" TIME ");
    uart_putdec(WATCHDOG_CTRL & TIME_MASK);
    uart_puts("\r\n");
}

static void run(uint32_t load, int load_before_enable) {
    WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
    uart_puts(load_before_enable ? "-- LOAD then ENABLE, LOAD " : "-- ENABLE then LOAD, LOAD ");
    uart_putdec(load);
    uart_puts("\r\n");
    sample("  disabled     ");
    if (load_before_enable) {
        WATCHDOG_LOAD = load;
        sample("  after LOAD   ");
        WATCHDOG_CTRL |= WATCHDOG_CTRL_ENABLE;
    } else {
        WATCHDOG_CTRL |= WATCHDOG_CTRL_ENABLE;
        sample("  after ENABLE ");
        WATCHDOG_LOAD = load;
    }
    sample("  t=0          ");
    sleep_us(10000u);
    sample("  t=10ms       ");
    sleep_us(90000u);
    sample("  t=100ms      ");
    WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
    sleep_us(50000u);
    sample("  disabled +50 ");
}

int main(void) {
    board_init();
    uart_puts("watchdog time start, TICK 0x");
    uart_puthex(WATCHDOG_TICK, 8);
    uart_puts("\r\n");
    uart_puts("reason 0x");
    uart_puthex(WATCHDOG_REASON, 1);
    uart_puts("\r\n");
    sample("  at boot      ");
    if (WATCHDOG_SCRATCH(0) == MAGIC) {
        WATCHDOG_SCRATCH(0) = 0;
        uart_puts("boot 2 after LOAD 600000 expired\r\ndone\r\n");
        while (1) {
        }
    }
    run(3000000u, 1);
    run(6000000u, 0);
    run(9000000u, 1);
    /* 만료까지 두고 다음 부팅에서 TIME을 본다. E1로 600000은 약 300ms */
    WATCHDOG_SCRATCH(0) = MAGIC;
    /* 전원 직후에는 PAUSE_DBG0, PAUSE_DBG1, PAUSE_JTAG가 리셋값 1이다. 지운 뒤 만료를 본다 */
    WATCHDOG_CTRL &= ~WATCHDOG_CTRL_PAUSE;
    sample("  pause cleared");
    WATCHDOG_LOAD = 600000u;
    WATCHDOG_CTRL |= WATCHDOG_CTRL_ENABLE;
    for (int i = 0; i < 10; i++) {
        sample("  expiring     ");
        sleep_us(50000u);
    }
    while (1) {
    }
}
