/*
 * RTC (데이터시트 4.8). clk_rtc = XOSC 12MHz / 256 = 46875Hz, CLKDIV_M1 = 46874로
 * 1초 기준을 만든다 (4.8.4). 2026-11-04 수 23:59:57에서 시작해 날짜·요일이 넘어가는지,
 * 초가 바뀌는 간격이 TIMER로 1초인지 본다.
 * 이 보드에서는 설정한 57초가 읽히지 않고 첫 값이 58초였다 (원인 미확인).
 */
#include "board.h"

#define RTC_BASE 0x4005c000u
#define RTC_CLKDIV_M1 REG(RTC_BASE + 0x00u)
#define RTC_SETUP_0 REG(RTC_BASE + 0x04u)
#define RTC_SETUP_1 REG(RTC_BASE + 0x08u)
#define RTC_CTRL REG(RTC_BASE + 0x0cu)
#define RTC_RTC_1 REG(RTC_BASE + 0x18u)
#define RTC_RTC_0 REG(RTC_BASE + 0x1cu)
#define RTC_CTRL_ENABLE (1u << 0)
#define RTC_CTRL_ACTIVE (1u << 1)
#define RTC_CTRL_LOAD (1u << 4)

#define CLK_RTC_CTRL REG(CLOCKS_BASE + 0x6cu)
#define CLK_RTC_DIV REG(CLOCKS_BASE + 0x70u)
#define CLK_RTC_AUXSRC_XOSC (3u << 5)
#define CLK_RTC_HZ (XOSC_HZ / 256u)

static void clk_rtc_init(void) {
    /* AUXSRC는 끈 상태에서 바꾼다 (2.15.3) */
    CLK_RTC_CTRL = 0;
    delay(100);
    CLK_RTC_DIV = 256u << 8;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC | CLK_GEN_ENABLE;
}

static void rtc_set(uint32_t year, uint32_t month, uint32_t day, uint32_t dotw,
                    uint32_t hour, uint32_t min, uint32_t sec) {
    RTC_CTRL = 0;
    while (RTC_CTRL & RTC_CTRL_ACTIVE) {
    }
    RTC_CLKDIV_M1 = CLK_RTC_HZ - 1u;
    RTC_SETUP_0 = (year << 12) | (month << 8) | day;
    RTC_SETUP_1 = (dotw << 24) | (hour << 16) | (min << 8) | sec;
    RTC_CTRL = RTC_CTRL_LOAD;
    RTC_CTRL = RTC_CTRL_ENABLE;
    while (!(RTC_CTRL & RTC_CTRL_ACTIVE)) {
    }
}

static void put2(uint32_t v) {
    uart_putc((char) ('0' + v / 10u));
    uart_putc((char) ('0' + v % 10u));
}

static void print_now(uint32_t rtc0, uint32_t rtc1) {
    static const char *const days[7] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};
    uart_putdec(rtc1 >> 12);
    uart_putc('-');
    put2((rtc1 >> 8) & 0xfu);
    uart_putc('-');
    put2(rtc1 & 0x1fu);
    uart_putc(' ');
    uart_puts(days[(rtc0 >> 24) & 7u]);
    uart_putc(' ');
    put2((rtc0 >> 16) & 0x1fu);
    uart_putc(':');
    put2((rtc0 >> 8) & 0x3fu);
    uart_putc(':');
    put2(rtc0 & 0x3fu);
}

int main(void) {
    board_init();
    clk_rtc_init();
    reset_block(RESETS_RTC);
    rtc_set(2026u, 11u, 4u, 3u, 23u, 59u, 57u);
    /* 쓰기는 clk_rtc 2주기(약 43us) 뒤 반영된다 (4.8.4 NOTE). 그 전 읽기는 이전 값이다 */
    sleep_us(100u);
    uart_puts("rtc start\r\n");

    uint32_t last_sec = 0xffu;
    uint32_t last_us = 0;
    for (uint32_t ticks = 0; ticks < 6u;) {
        uint32_t rtc0 = RTC_RTC_0; /* RTC_0을 먼저 읽어야 RTC_1이 같은 순간으로 잡힌다 */
        uint32_t rtc1 = RTC_RTC_1;
        uint32_t sec = rtc0 & 0x3fu;
        if (sec == last_sec) {
            continue;
        }
        uint32_t now = time_us();
        print_now(rtc0, rtc1);
        if (last_sec != 0xffu) {
            uart_puts(", +");
            uart_putdec(now - last_us);
            uart_puts(" us");
            ticks++;
        }
        uart_puts("\r\n");
        last_sec = sec;
        last_us = now;
    }
    while (1) {
    }
}
