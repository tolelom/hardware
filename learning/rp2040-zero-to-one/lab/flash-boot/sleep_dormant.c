/*
 * SLEEP과 DORMANT (데이터시트 2.11).
 *
 * SLEEP: 두 코어가 모두 WFI/WFE이고 DMA가 쉬면 칩이 SLEEP 상태가 되고, 클럭 게이트가
 * WAKE_ENx 대신 SLEEP_ENx를 따른다 (2.11.2). SLEEP_EN0에서 CLK_SYS_PWM만 끄고
 * TIMER 알람으로 50ms 뒤 깨어나, 그동안 PWM 카운터(CTR)가 움직였는지 본다.
 * 대조군으로 SLEEP_EN0을 모두 1로 둔 경우도 잰다.
 * SCR.SLEEPDEEP 0과 1을 비교한다. core1은 bootrom 안에서 WFE로 잠들어 있다.
 *
 * DORMANT: 모든 발진기가 멈춘다 (2.11.3). clk_sys를 XOSC로 내리고 PLL_SYS와 ROSC를
 * 끈 뒤 XOSC DORMANT에 0x636f6d61을 쓴다. GPIO1(UART RX)의 falling edge로 깨어난다.
 * PC가 몇 초 뒤 바이트를 보내 깨우고, 그동안 TIMER가 멈췄는지 본다.
 * 깨어난 뒤에는 clk_sys 12MHz(XOSC)로 남는다.
 */
#include "board.h"

#define SLEEP_EN0 REG(CLOCKS_BASE + 0xa8u)
#define SLEEP_EN1 REG(CLOCKS_BASE + 0xacu)
#define CLK_SYS_PWM_BIT (1u << 17)
#define M0PLUS_SCR REG(0xe000ed10u)
#define SCR_SLEEPDEEP (1u << 2)

#define TIMER_ALARM0 REG(0x40054000u + 0x10u)
#define TIMER_INTR REG(0x40054000u + 0x34u)
#define TIMER_INTE REG(0x40054000u + 0x38u)

#define PWM_BASE 0x40050000u
#define PWM_CSR(s) REG(PWM_BASE + (s) * 0x14u + 0x00u)
#define PWM_DIV(s) REG(PWM_BASE + (s) * 0x14u + 0x04u)
#define PWM_CTR(s) REG(PWM_BASE + (s) * 0x14u + 0x08u)
#define PWM_TOP(s) REG(PWM_BASE + (s) * 0x14u + 0x10u)
#define SLICE 2u /* 핀에 연결하지 않음 */

#define XOSC_DORMANT REG(0x40024000u + 0x08u)
#define XOSC_STATUS REG(0x40024000u + 0x04u)
#define XOSC_STABLE (1u << 31)
#define XOSC_DORMANT_KEY 0x636f6d61u
#define ROSC_CTRL REG(0x40060000u + 0x00u)
#define ROSC_DISABLE (0xd1eu << 12)
#define ROSC_ENABLE (0xfabu << 12)
#define PLL_SYS_PWR REG(0x40028000u + 0x04u)
#define PLL_PWR_ALL_DOWN 0x2du /* VCOPD, POSTDIVPD, DSMPD, PD */
#define CLK_SYS_SRC_AUX 1u

#define IO_INTR0 REG(0x40014000u + 0x0f0u)
#define IO_DORMANT_WAKE_INTE0 REG(0x40014000u + 0x160u)
#define GPIO1_EDGE_LOW (1u << 6)

void isr_irq0(void) {
    TIMER_INTR = 1u;
}

static void sleep_test(int deep, int gate_pwm) {
    TIMER_INTE = 1u;
    NVIC_ICPR = 1u;
    NVIC_ISER = 1u;
    SLEEP_EN0 = gate_pwm ? 0xffffffffu & ~CLK_SYS_PWM_BIT : 0xffffffffu;
    SLEEP_EN1 = 0x7fffu;
    if (deep) {
        M0PLUS_SCR |= SCR_SLEEPDEEP;
    } else {
        M0PLUS_SCR &= ~SCR_SLEEPDEEP;
    }
    uart_flush();

    uint32_t ctr_before = PWM_CTR(SLICE);
    uint32_t t_before = time_us();
    TIMER_ALARM0 = t_before + 50000u;
    __asm volatile("wfi");
    uint32_t ctr_after = PWM_CTR(SLICE);
    uint32_t t_after = time_us();
    M0PLUS_SCR &= ~SCR_SLEEPDEEP;
    NVIC_ICER = 1u;

    /* 125MHz / 255 = 490196 카운트/초. 50ms면 24510 */
    uint32_t moved = (ctr_after - ctr_before) & 0xffffu;
    uart_puts(deep ? "wfi, SLEEPDEEP 1, " : "wfi, SLEEPDEEP 0, ");
    uart_puts(gate_pwm ? "PWM gated in sleep: " : "PWM not gated:      ");
    uart_putdec(t_after - t_before);
    uart_puts(" us asleep, PWM CTR moved ");
    uart_putdec(moved);
    uart_puts(" (24510 if PWM clock ran)\r\n");
}

static void dormant_test(void) {
    uart_puts("going dormant, send a byte to wake\r\n");
    uart_flush();
    /* clk_sys를 clk_ref(XOSC)로 내리고 PLL과 ROSC를 끈다 */
    CLK_SYS_CTRL &= ~CLK_SYS_SRC_AUX;
    while (CLK_SYS_SELECTED != 1u) {
    }
    PLL_SYS_PWR = PLL_PWR_ALL_DOWN;
    ROSC_CTRL = (ROSC_CTRL & 0xfffu) | ROSC_DISABLE;

    IO_INTR0 = GPIO1_EDGE_LOW;
    IO_DORMANT_WAKE_INTE0 = GPIO1_EDGE_LOW;
    uint32_t t_before = time_us();
    XOSC_DORMANT = XOSC_DORMANT_KEY;
    while (!(XOSC_STATUS & XOSC_STABLE)) {
    }
    uint32_t t_after = time_us();
    IO_DORMANT_WAKE_INTE0 = 0;
    IO_INTR0 = GPIO1_EDGE_LOW;
    /* ROSC를 다시 켠다. watchdog 리셋은 ROSC를 리셋하지 않고 리셋 뒤 클럭은 ROSC에서
       시작하므로, 꺼 둔 채 재부팅하면 칩이 멈춘다 (이 보드에서 겪음) */
    ROSC_CTRL = (ROSC_CTRL & 0xfffu) | ROSC_ENABLE;

    sleep_us(20000u); /* 깨운 바이트의 나머지가 지나가길 기다린다 */
    while (!(UART0_FR & UART_FR_RXFE)) {
        (void) UART0_DR;
    }
    uart_puts("awake, TIMER advanced ");
    uart_putdec(t_after - t_before);
    uart_puts(" us while dormant\r\n");
}

int main(void) {
    board_init();
    unreset(RESETS_PWM);
    PWM_TOP(SLICE) = 0xffffu;
    PWM_DIV(SLICE) = 255u << 4;
    PWM_CSR(SLICE) = 1u;
    uart_puts("sleep start\r\n");
    for (int deep = 0; deep < 2; deep++) {
        sleep_test(deep, 0);
        sleep_test(deep, 1);
    }
    dormant_test();
    uart_puts("done\r\n");
    while (1) {
    }
}
