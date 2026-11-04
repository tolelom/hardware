/*
 * ROSC RANDOMBIT (데이터시트 2.17.5). 시스템 클럭이 XOSC/PLL일 때 ROSC 위상을 읽어
 * 1비트 난수를 얻는다. 보안 용도로는 쓸 수 없다고 데이터시트가 밝힌다.
 * 읽는 간격(지연 없음, 지연 있음)별로 비트를 모아 1의 비율과 이웃 비트가 바뀌는
 * 비율을 1000분율로 보낸다. 둘 다 치우침이 없으면 500 근처다.
 * 마지막으로 von Neumann 보정을 거친 비트로 같은 값을 재고 32비트 값을 몇 개 만든다.
 */
#include "board.h"

#define ROSC_RANDOMBIT REG(0x40060000u + 0x1cu)
#define BITS 100000u

static void measure(const char *name, uint32_t gap) {
    uint32_t ones = 0;
    uint32_t changes = 0;
    uint32_t prev = ROSC_RANDOMBIT & 1u;
    for (uint32_t i = 0; i < BITS; i++) {
        delay(gap);
        uint32_t bit = ROSC_RANDOMBIT & 1u;
        ones += bit;
        changes += bit ^ prev;
        prev = bit;
    }
    uart_puts(name);
    uart_puts(": ones ");
    uart_putdec(ones * 1000u / BITS);
    uart_puts("/1000, changes ");
    uart_putdec(changes * 1000u / BITS);
    uart_puts("/1000\r\n");
}

/* von Neumann 보정: 두 비트를 읽어 01이면 0, 10이면 1, 같으면 버린다.
   두 비트가 서로 독립이면 치우침이 사라진다 */
static uint32_t random_bit(void) {
    while (1) {
        delay(100);
        uint32_t a = ROSC_RANDOMBIT & 1u;
        delay(100);
        uint32_t b = ROSC_RANDOMBIT & 1u;
        if (a != b) {
            return a;
        }
    }
}

static void measure_whitened(void) {
    uint32_t ones = 0;
    uint32_t changes = 0;
    uint32_t prev = random_bit();
    for (uint32_t i = 0; i < BITS / 4u; i++) {
        uint32_t bit = random_bit();
        ones += bit;
        changes += bit ^ prev;
        prev = bit;
    }
    uart_puts("von Neumann, gap 100: ones ");
    uart_putdec(ones * 1000u / (BITS / 4u));
    uart_puts("/1000, changes ");
    uart_putdec(changes * 1000u / (BITS / 4u));
    uart_puts("/1000\r\n");
}

static uint32_t random32(void) {
    uint32_t value = 0;
    for (int i = 0; i < 32; i++) {
        value = (value << 1) | random_bit();
    }
    return value;
}

int main(void) {
    board_init();
    uart_puts("rosc random start\r\n");
    measure("gap 0 ", 0);
    measure("gap 10", 10);
    measure("gap 100", 100);
    measure_whitened();
    for (int i = 0; i < 4; i++) {
        uart_puts("0x");
        uart_puthex(random32(), 8);
        uart_puts("\r\n");
    }
    while (1) {
    }
}
