/*
 * watchdog 리셋과 scratch 레지스터 (데이터시트 4.7, PSM 2.13).
 * 부팅마다 REASON과 SCRATCH0의 부팅 횟수를 보내고 횟수를 1 늘린다.
 * 홀수 회차는 먹이를 끊어 타임아웃으로, 짝수 회차는 CTRL.TRIGGER로 리셋한다.
 * LOAD는 1000000을 쓴다. 틱이 1us이고 RP2040-E1대로 틱당 2씩 줄면 0.5초 뒤 리셋된다.
 * CTRL 원값도 함께 보낸다. 이 보드에서 CTRL.TIME은 LOAD·경과 시간과 무관하게
 * 0x7a120으로 고정되어 읽혔다 (원인 미확인).
 * 4회 리셋한 뒤 멈춘다. SCRATCH4~7은 bootrom이 보는 자리라 쓰지 않는다 (2.8.1.1).
 */
#include "board.h"

#define WATCHDOG_BASE 0x40058000u
#define WATCHDOG_CTRL REG(WATCHDOG_BASE + 0x00u)
#define WATCHDOG_LOAD REG(WATCHDOG_BASE + 0x04u)
#define WATCHDOG_REASON REG(WATCHDOG_BASE + 0x08u)
#define WATCHDOG_SCRATCH0 REG(WATCHDOG_BASE + 0x0cu)
#define WATCHDOG_SCRATCH1 REG(WATCHDOG_BASE + 0x10u)
#define WATCHDOG_CTRL_ENABLE (1u << 30)
#define WATCHDOG_CTRL_TRIGGER (1u << 31)
#define PSM_WDSEL REG(0x40010000u + 0x08u)
#define PSM_WDSEL_ALL 0x1ffffu
#define PSM_WDSEL_OSC 0x3u /* ROSC, XOSC */
#define LOAD_VALUE 1000000u
#define ROUNDS 4u

static uint32_t boot_count(void) {
    uint32_t count = WATCHDOG_SCRATCH0;
    /* 전원 투입 직후 scratch는 0이지만, 다른 프로그램이 남긴 값과 구분하려고 보수를 함께 둔다 */
    return WATCHDOG_SCRATCH1 == ~count ? count : 0u;
}

static void set_boot_count(uint32_t count) {
    WATCHDOG_SCRATCH0 = count;
    WATCHDOG_SCRATCH1 = ~count;
}

int main(void) {
    board_init();
    uint32_t reason = WATCHDOG_REASON;
    uint32_t count = boot_count();
    uart_puts("boot ");
    uart_putdec(count);
    uart_puts(", reason 0x");
    uart_puthex(reason, 1);
    uart_puts(reason & 2u ? " (force)" : reason & 1u ? " (timer)" : " (power-on or RUN)");
    uart_puts("\r\n");

    if (count >= ROUNDS) {
        set_boot_count(0);
        uart_puts("done\r\n");
        while (1) {
        }
    }
    set_boot_count(count + 1u);

    WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
    PSM_WDSEL = PSM_WDSEL_ALL & ~PSM_WDSEL_OSC;

    if ((count & 1u) == 0u) {
        uart_puts("trigger\r\n");
        uart_flush();
        WATCHDOG_CTRL |= WATCHDOG_CTRL_TRIGGER;
        while (1) {
        }
    }

    WATCHDOG_LOAD = LOAD_VALUE;
    WATCHDOG_CTRL |= WATCHDOG_CTRL_ENABLE;
    for (uint32_t i = 0; i < 3u; i++) {
        sleep_us(200000u);
        WATCHDOG_LOAD = LOAD_VALUE;
        uart_puts("feed\r\n");
    }
    uint32_t start = time_us();
    uart_puts("stop feeding\r\n");
    uart_flush();
    uint32_t last = start;
    while (1) {
        uint32_t now = time_us();
        if (now - last >= 100000u) {
            last = now;
            uart_puts("  alive ");
            uart_putdec((now - start) / 1000u);
            uart_puts(" ms, CTRL 0x");
            uart_puthex(WATCHDOG_CTRL, 8);
            uart_puts("\r\n");
        }
    }
}
