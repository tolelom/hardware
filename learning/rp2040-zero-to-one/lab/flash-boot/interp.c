/*
 * SIO interpolator (데이터시트 2.3.1.6). 데이터시트가 SDK 예제와 함께 실은 세 가지를
 * 레지스터로 직접 재현하고, 데이터시트에 적힌 기대 출력과 대조한다.
 * 1. 9단 곱셈표: POP으로 ACCUM0 += BASE0
 * 2. 니블 마스킹: ACCUM0_ADD를 읽으면 BASE 없이 shift·mask 값만 나온다
 * 3. blend: BASE0 = 500, BASE1 = 1000 사이를 alpha(ACCUM1 하위 8비트)로 보간
 */
#include "board.h"

#define INTERP0_ACCUM0 REG(SIO_BASE + 0x080u)
#define INTERP0_ACCUM1 REG(SIO_BASE + 0x084u)
#define INTERP0_BASE0 REG(SIO_BASE + 0x088u)
#define INTERP0_BASE1 REG(SIO_BASE + 0x08cu)
#define INTERP0_POP_LANE0 REG(SIO_BASE + 0x094u)
#define INTERP0_PEEK_LANE1 REG(SIO_BASE + 0x0a4u)
#define INTERP0_CTRL_LANE0 REG(SIO_BASE + 0x0acu)
#define INTERP0_CTRL_LANE1 REG(SIO_BASE + 0x0b0u)
#define INTERP0_ACCUM0_ADD REG(SIO_BASE + 0x0b4u)

#define CTRL_MASK(lsb, msb) (((uint32_t) (lsb) << 5) | ((uint32_t) (msb) << 10))
#define CTRL_SIGNED (1u << 15)
#define CTRL_BLEND (1u << 21)
#define CTRL_DEFAULT CTRL_MASK(0, 31) /* SDK interp_default_config: shift 0, mask 0..31 */

static uint32_t failures;

static void check(uint32_t got, uint32_t want) {
    uart_puthex(got, 8);
    if (got != want) {
        uart_puts(" (want ");
        uart_puthex(want, 8);
        uart_puts(")");
        failures++;
    }
    uart_puts(" ");
}

static void times_table(void) {
    INTERP0_CTRL_LANE0 = CTRL_DEFAULT;
    INTERP0_ACCUM0 = 0;
    INTERP0_BASE0 = 9;
    uart_puts("times table: ");
    for (uint32_t i = 0; i < 10u; i++) {
        uint32_t got = INTERP0_POP_LANE0;
        uart_putdec(got);
        uart_putc(' ');
        failures += got != 9u * (i + 1u);
    }
    uart_puts("\r\n");
}

static void moving_mask(void) {
    static const uint32_t want_unsigned[8] = {
        0x0000000du, 0x000000c0u, 0x00000b00u, 0x0000a000u,
        0x00040000u, 0x00300000u, 0x02000000u, 0x10000000u,
    };
    static const uint32_t want_signed[8] = {
        0xfffffffdu, 0xffffffc0u, 0xfffffb00u, 0xffffa000u,
        0x00040000u, 0x00300000u, 0x02000000u, 0x10000000u,
    };
    INTERP0_ACCUM0 = 0x1234abcdu;
    for (uint32_t s = 0; s < 2u; s++) {
        uart_puts(s ? "mask signed:   " : "mask unsigned: ");
        for (uint32_t i = 0; i < 8u; i++) {
            INTERP0_CTRL_LANE0 = CTRL_MASK(i * 4u, i * 4u + 3u) | (s ? CTRL_SIGNED : 0u);
            check(INTERP0_ACCUM0_ADD, s ? want_signed[i] : want_unsigned[i]);
        }
        uart_puts("\r\n");
    }
}

static void simple_blend(void) {
    static const uint32_t want[7] = {500, 582, 666, 748, 832, 914, 998};
    INTERP0_CTRL_LANE0 = CTRL_DEFAULT | CTRL_BLEND;
    INTERP0_CTRL_LANE1 = CTRL_DEFAULT;
    INTERP0_BASE0 = 500;
    INTERP0_BASE1 = 1000;
    uart_puts("blend: ");
    for (uint32_t i = 0; i <= 6u; i++) {
        INTERP0_ACCUM1 = 255u * i / 6u;
        uint32_t got = INTERP0_PEEK_LANE1;
        uart_putdec(got);
        uart_putc(' ');
        failures += got != want[i];
    }
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    uart_puts("interp start\r\n");
    times_table();
    moving_mask();
    simple_blend();
    uart_puts("failures ");
    uart_putdec(failures);
    uart_puts("\r\n");
    while (1) {
    }
}
