/*
 * RTC 알람과 DORMANT (데이터시트 2.11.3, 4.8). DORMANT에서 RTC로 깨우려면 RTC가
 * 외부 클럭으로 돌아야 한다 (2.11.3). 이 보드는 외부 RTC 클럭이 없으므로 clk_rtc를
 * XOSC / 256(46875Hz)로 두고, DORMANT 동안 RTC가 멈추는지 본다.
 * 대조: DORMANT 없이 2초 뒤 알람이 오는 시간을 TIMER로 잰다.
 * 시험: 같은 알람을 걸고 DORMANT에 들어간다. RTC로 못 깨면 PC가 UART로 바이트를 보내
 * GPIO1 falling edge로 깨운다 (sleep_dormant.c와 같은 순서).
 */
#include "board.h"

#define RTC_BASE 0x4005c000u
#define RTC_CLKDIV_M1 REG(RTC_BASE + 0x00u)
#define RTC_SETUP_0 REG(RTC_BASE + 0x04u)
#define RTC_SETUP_1 REG(RTC_BASE + 0x08u)
#define RTC_CTRL REG(RTC_BASE + 0x0cu)
#define RTC_IRQ_SETUP_0 REG(RTC_BASE + 0x10u)
#define RTC_IRQ_SETUP_1 REG(RTC_BASE + 0x14u)
#define RTC_RTC_1 REG(RTC_BASE + 0x18u)
#define RTC_RTC_0 REG(RTC_BASE + 0x1cu)
#define RTC_INTR REG(RTC_BASE + 0x20u)
#define RTC_INTE REG(RTC_BASE + 0x24u)
#define RTC_CTRL_ENABLE (1u << 0)
#define RTC_CTRL_ACTIVE (1u << 1)
#define RTC_CTRL_LOAD (1u << 4)
#define IRQ0_MATCH_ENA (1u << 28)
#define IRQ1_SEC_ENA (1u << 28)

#define CLK_RTC_CTRL REG(CLOCKS_BASE + 0x6cu)
#define CLK_RTC_DIV REG(CLOCKS_BASE + 0x70u)
#define CLK_RTC_AUXSRC_XOSC (3u << 5)
#define CLK_RTC_HZ (XOSC_HZ / 256u)

#define XOSC_DORMANT REG(0x40024000u + 0x08u)
#define XOSC_STATUS REG(0x40024000u + 0x04u)
#define XOSC_STABLE (1u << 31)
#define XOSC_DORMANT_KEY 0x636f6d61u
#define ROSC_CTRL REG(0x40060000u + 0x00u)
#define ROSC_DISABLE (0xd1eu << 12)
#define ROSC_ENABLE (0xfabu << 12)
#define PLL_SYS_PWR REG(0x40028000u + 0x04u)
#define PLL_PWR_ALL_DOWN 0x2du
#define IO_INTR0 REG(0x40014000u + 0x0f0u)
#define IO_DORMANT_WAKE_INTE0 REG(0x40014000u + 0x160u)
#define GPIO1_EDGE_LOW (1u << 6)

static void clk_rtc_init(void) {
    CLK_RTC_CTRL = 0;
    delay(100);
    CLK_RTC_DIV = 256u << 8;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC;
    CLK_RTC_CTRL = CLK_RTC_AUXSRC_XOSC | CLK_GEN_ENABLE;
}

/* 2026-11-04 00:00:00으로 맞추고, 초가 2가 되면 알람 */
static void rtc_start_with_alarm(void) {
    RTC_CTRL = 0;
    while (RTC_CTRL & RTC_CTRL_ACTIVE) {
    }
    RTC_IRQ_SETUP_0 = 0;
    RTC_CLKDIV_M1 = CLK_RTC_HZ - 1u;
    RTC_SETUP_0 = (2026u << 12) | (11u << 8) | 4u;
    RTC_SETUP_1 = 3u << 24;
    RTC_CTRL = RTC_CTRL_LOAD;
    RTC_CTRL = RTC_CTRL_ENABLE;
    while (!(RTC_CTRL & RTC_CTRL_ACTIVE)) {
    }
    sleep_us(100u); /* 쓰기 반영 대기 (rtc.c 기록) */
    RTC_IRQ_SETUP_1 = IRQ1_SEC_ENA | 2u;
    RTC_IRQ_SETUP_0 = IRQ0_MATCH_ENA;
    RTC_INTE = 1u;
}

static void print_rtc(void) {
    uint32_t rtc0 = RTC_RTC_0; /* RTC_0을 먼저 읽는다 (4.8.6) */
    (void) RTC_RTC_1;
    uart_puts("rtc ");
    uart_putdec((rtc0 >> 16) & 0x1fu);
    uart_putc(':');
    uart_putdec((rtc0 >> 8) & 0x3fu);
    uart_putc(':');
    uart_putdec(rtc0 & 0x3fu);
}

int main(void) {
    board_init();
    clk_rtc_init();
    reset_block(RESETS_RTC);
    uart_puts("rtc dormant start\r\n");

    rtc_start_with_alarm();
    uint32_t t0 = time_us();
    while (!(RTC_INTR & 1u)) {
    }
    uint32_t t1 = time_us();
    uart_puts("control: alarm after ");
    uart_putdec((t1 - t0) / 1000u);
    uart_puts(" ms, ");
    print_rtc();
    uart_puts("\r\n");

    rtc_start_with_alarm();
    uart_puts("going dormant, rtc alarm at 0:0:2, send a byte to wake\r\n");
    uart_flush();
    CLK_SYS_CTRL &= ~1u;
    while (CLK_SYS_SELECTED != 1u) {
    }
    PLL_SYS_PWR = PLL_PWR_ALL_DOWN;
    ROSC_CTRL = (ROSC_CTRL & 0xfffu) | ROSC_DISABLE;
    IO_INTR0 = GPIO1_EDGE_LOW;
    IO_DORMANT_WAKE_INTE0 = GPIO1_EDGE_LOW;
    uint32_t before = time_us();
    XOSC_DORMANT = XOSC_DORMANT_KEY;
    while (!(XOSC_STATUS & XOSC_STABLE)) {
    }
    uint32_t after = time_us();
    uint32_t gpio_woke = IO_INTR0 & GPIO1_EDGE_LOW;
    uint32_t rtc_intr = RTC_INTR;
    IO_DORMANT_WAKE_INTE0 = 0;
    IO_INTR0 = GPIO1_EDGE_LOW;
    ROSC_CTRL = (ROSC_CTRL & 0xfffu) | ROSC_ENABLE;
    sleep_us(20000u);
    while (!(UART0_FR & UART_FR_RXFE)) {
        (void) UART0_DR;
    }
    uart_puts("awake: TIMER advanced ");
    uart_putdec(after - before);
    uart_puts(" us, GPIO1 edge ");
    uart_putdec(gpio_woke ? 1u : 0u);
    uart_puts(", RTC INTR at wake ");
    uart_putdec(rtc_intr);
    uart_puts(", ");
    print_rtc();
    uart_puts("\r\n");
    sleep_us(2500000u);
    uart_puts("2.5 s later at 12 MHz: RTC INTR ");
    uart_putdec(RTC_INTR);
    uart_puts(", ");
    print_rtc();
    uart_puts("\r\ndone\r\n");
    while (1) {
    }
}
