/*
 * RGB2를 PWM으로 밝기 조절한다 (데이터시트 4.5).
 * GPIO16 = PWM0 A(초록), GPIO17 = PWM0 B(빨강), GPIO25 = PWM4 B(파랑) (표 515).
 * 출력은 카운터 < CC일 때 high이고, LED는 active low라 A_INV/B_INV로 뒤집는다.
 *
 * 먼저 1kHz, 25%/75%로 두고 1초 동안 wrap 횟수(INTR)와 핀 입력의 high 비율을
 * 세어 UART로 보낸 뒤, 숨쉬기 패턴으로 넘어간다.
 */
#include "board.h"

#define PWM_BASE 0x40050000u
#define PWM_CSR(s) REG(PWM_BASE + (s) * 0x14u + 0x00u)
#define PWM_DIV(s) REG(PWM_BASE + (s) * 0x14u + 0x04u)
#define PWM_CTR(s) REG(PWM_BASE + (s) * 0x14u + 0x08u)
#define PWM_CC(s) REG(PWM_BASE + (s) * 0x14u + 0x0cu)
#define PWM_TOP(s) REG(PWM_BASE + (s) * 0x14u + 0x10u)
#define PWM_INTR REG(PWM_BASE + 0xa4u)
#define PWM_CSR_EN (1u << 0)
#define PWM_CSR_A_INV (1u << 2)
#define PWM_CSR_B_INV (1u << 3)
#define PWM_DIV_INT(n) ((n) << 4)

#define SLICE_GB 0u /* A = 초록, B = 빨강 */
#define SLICE_BLUE 4u /* B = 파랑 */
#define SAMPLES 200000u

static void measure(void) {
    /* 125MHz / 125 = 1MHz 카운트, TOP 999 → 1kHz */
    PWM_DIV(SLICE_GB) = PWM_DIV_INT(125u);
    PWM_TOP(SLICE_GB) = 999u;
    PWM_CC(SLICE_GB) = (750u << 16) | 250u;
    PWM_CSR(SLICE_GB) = PWM_CSR_EN | PWM_CSR_A_INV | PWM_CSR_B_INV;

    PWM_INTR = 1u << SLICE_GB;
    uint32_t wraps = 0;
    uint32_t start = time_us();
    while (time_us() - start < 1000000u) {
        if (PWM_INTR & (1u << SLICE_GB)) {
            PWM_INTR = 1u << SLICE_GB;
            wraps++;
        }
    }

    uint32_t high_green = 0;
    uint32_t high_red = 0;
    for (uint32_t i = 0; i < SAMPLES; i++) {
        uint32_t in = SIO_GPIO_IN;
        high_green += (in >> LED_GREEN) & 1u;
        high_red += (in >> LED_RED) & 1u;
    }

    uart_puts("wraps/s ");
    uart_putdec(wraps);
    uart_puts(", pin high green ");
    uart_putdec(high_green * 1000u / SAMPLES);
    uart_puts("/1000 (expect 750), red ");
    uart_putdec(high_red * 1000u / SAMPLES);
    uart_puts("/1000 (expect 250)\r\n");
}

/* 0..255..0 삼각파. 감마 보정 없이 제곱으로 근사한다 */
static uint32_t level(uint32_t step) {
    uint32_t x = step < 256u ? step : 511u - step;
    return x * x / 16u; /* 0..4064, TOP 4095 */
}

int main(void) {
    board_init();
    unreset(RESETS_PWM);
    GPIO_CTRL(LED_GREEN) = FUNCSEL_PWM;
    GPIO_CTRL(LED_RED) = FUNCSEL_PWM;
    GPIO_CTRL(LED_BLUE) = FUNCSEL_PWM;
    uart_puts("pwm start\r\n");

    measure();

    /* 125MHz 그대로, TOP 4095 → 약 30.5kHz */
    PWM_DIV(SLICE_GB) = PWM_DIV_INT(1u);
    PWM_DIV(SLICE_BLUE) = PWM_DIV_INT(1u);
    PWM_TOP(SLICE_GB) = 4095u;
    PWM_TOP(SLICE_BLUE) = 4095u;
    PWM_CC(SLICE_GB) = 0;
    PWM_CC(SLICE_BLUE) = 0;
    PWM_CSR(SLICE_GB) = PWM_CSR_EN | PWM_CSR_A_INV | PWM_CSR_B_INV;
    PWM_CSR(SLICE_BLUE) = PWM_CSR_EN | PWM_CSR_B_INV;

    static const char *const names[3] = {"red", "green", "blue"};
    uint32_t color = 0;
    while (1) {
        uart_puts(names[color]);
        uart_puts("\r\n");
        for (uint32_t step = 0; step < 512u; step++) {
            uint32_t v = level(step);
            PWM_CC(SLICE_GB) = color == 0 ? v << 16 : color == 1 ? v : 0;
            PWM_CC(SLICE_BLUE) = color == 2 ? v << 16 : 0;
            sleep_us(4000u);
        }
        color = color == 2 ? 0 : color + 1;
    }
}
