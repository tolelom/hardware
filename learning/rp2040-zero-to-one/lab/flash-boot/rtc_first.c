/*
 * RTC 설정 직후 초 값 추적 (데이터시트 4.8). rtc.c에서 설정한 57초가 읽히지 않고 첫 값이
 * 58초였다. 설정 직후부터 RTC_0을 계속 읽어 값이 바뀌는 시각을 TIMER로 적는다.
 * A: SDK rtc_set_datetime과 같은 순서 (LOAD 바로 뒤 ENABLE)
 * B: LOAD 뒤 끈 상태로 1ms 기다려 읽고, 그다음 ENABLE
 */
#include "board.h"

#define RTC_BASE 0x4005c000u
#define RTC_CLKDIV_M1 REG(RTC_BASE + 0x00u)
#define RTC_SETUP_0 REG(RTC_BASE + 0x04u)
#define RTC_SETUP_1 REG(RTC_BASE + 0x08u)
#define RTC_CTRL REG(RTC_BASE + 0x0cu)
#define RTC_RTC_0 REG(RTC_BASE + 0x1cu)
#define RTC_CTRL_ENABLE (1u << 0)
#define RTC_CTRL_ACTIVE (1u << 1)
#define RTC_CTRL_LOAD (1u << 4)

#define CLK_RTC_CTRL REG(CLOCKS_BASE + 0x6cu)
#define CLK_RTC_DIV REG(CLOCKS_BASE + 0x70u)
#define CLK_RTC_AUXSRC_XOSC (3u << 5)
#define CLK_RTC_HZ (XOSC_HZ / 256u)
#define SEC 57u
#define CHANGES 4u

static void clk_rtc_init(void) {
    CLK_RTC_CTRL = 0;
    delay(100);
    CLK_RTC_DIV = 256u << 8;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC | CLK_GEN_ENABLE;
}

static void put_sample(const char *label, uint32_t t0) {
    uint32_t rtc0 = RTC_RTC_0;
    uint32_t now = time_us();
    uart_puts(label);
    uart_puts(" +");
    uart_putdec(now - t0);
    uart_puts(" us sec ");
    uart_putdec(rtc0 & 0x3fu);
    uart_puts("\r\n");
}

/* ENABLE 직후부터 초 값이 CHANGES번 바뀔 때까지 읽는다. 시각은 t0 기준 */
static void trace(uint32_t t0) {
    uint32_t times[CHANGES + 1];
    uint32_t secs[CHANGES + 1];
    uint32_t n = 0;
    uint32_t last = 0xffu;
    while (n <= CHANGES) {
        uint32_t sec = RTC_RTC_0 & 0x3fu;
        if (sec != last) {
            times[n] = time_us() - t0;
            secs[n] = sec;
            last = sec;
            n++;
        }
    }
    for (uint32_t i = 0; i <= CHANGES; i++) {
        uart_puts("  +");
        uart_putdec(times[i]);
        uart_puts(" us sec ");
        uart_putdec(secs[i]);
        uart_puts("\r\n");
    }
}

static void prepare(void) {
    RTC_CTRL = 0;
    while (RTC_CTRL & RTC_CTRL_ACTIVE) {
    }
    RTC_CLKDIV_M1 = CLK_RTC_HZ - 1u;
    RTC_SETUP_0 = (2026u << 12) | (11u << 8) | 4u;
    RTC_SETUP_1 = (3u << 24) | (12u << 16) | (0u << 8) | SEC;
}

int main(void) {
    board_init();
    clk_rtc_init();
    uart_puts("rtc first second\r\n");
    for (int round = 0; round < 2; round++) {
        reset_block(RESETS_RTC);
        uart_puts("A: LOAD then ENABLE at once\r\n");
        prepare();
        uint32_t t0 = time_us();
        RTC_CTRL = RTC_CTRL_LOAD;
        RTC_CTRL = RTC_CTRL_ENABLE;
        trace(t0);

        reset_block(RESETS_RTC);
        uart_puts("B: LOAD, wait 1ms disabled, then ENABLE\r\n");
        prepare();
        t0 = time_us();
        RTC_CTRL = RTC_CTRL_LOAD;
        sleep_us(1000u);
        put_sample("  disabled", t0);
        RTC_CTRL = RTC_CTRL_ENABLE;
        trace(t0);
    }
    uart_puts("done\r\n");
    while (1) {
    }
}
