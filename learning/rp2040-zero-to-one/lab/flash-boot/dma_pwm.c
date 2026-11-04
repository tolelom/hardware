/*
 * DMA가 PWM 밝기를 바꾸고 CPU는 잠든다 (데이터시트 2.5, 4.5).
 * 슬라이스 0(GPIO16 초록 A, GPIO17 빨강 B)은 30.5kHz로 LED를 구동한다.
 * 슬라이스 1은 핀에 연결하지 않고 약 127Hz로 wrap만 내서 DMA 박자(DREQ_PWM_WRAP1)로 쓴다.
 * 채널 0: 밝기 표 256워드를 슬라이스 0 CC로 하나씩 옮긴다. 끝나면 채널 1로 chain.
 * 채널 1: 표 시작 주소를 채널 0의 READ_ADDR_TRIG(별칭 3, +0x3c)에 써서 채널 0을 다시 띄운다.
 * 채널 0의 TRANS_COUNT는 다시 시작할 때 마지막에 쓴 값으로 다시 채워진다 (2.5.1.2).
 * 6초 동안 0.5초마다 상태를 보낸 뒤 CPU는 wfi로 잠든다.
 */
#include "board.h"

#define PWM_BASE 0x40050000u
#define PWM_CSR(s) REG(PWM_BASE + (s) * 0x14u + 0x00u)
#define PWM_DIV(s) REG(PWM_BASE + (s) * 0x14u + 0x04u)
#define PWM_CC(s) REG(PWM_BASE + (s) * 0x14u + 0x0cu)
#define PWM_TOP(s) REG(PWM_BASE + (s) * 0x14u + 0x10u)
#define PWM_CSR_EN (1u << 0)
#define PWM_CSR_A_INV (1u << 2)
#define PWM_CSR_B_INV (1u << 3)

#define DMA_BASE 0x50000000u
#define DMA_READ_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x00u)
#define DMA_WRITE_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x04u)
#define DMA_TRANS_COUNT(c) REG(DMA_BASE + (c) * 0x40u + 0x08u)
#define DMA_CTRL_TRIG(c) REG(DMA_BASE + (c) * 0x40u + 0x0cu)
#define DMA_AL1_CTRL(c) REG(DMA_BASE + (c) * 0x40u + 0x10u)
#define DMA_AL3_READ_ADDR_TRIG_ADDR(c) (DMA_BASE + (c) * 0x40u + 0x3cu)
#define CTRL_EN (1u << 0)
#define CTRL_SIZE_WORD (2u << 2)
#define CTRL_INCR_READ (1u << 4)
#define CTRL_CHAIN_TO(c) ((uint32_t) (c) << 11)
#define CTRL_TREQ(n) ((uint32_t) (n) << 15)
#define TREQ_PWM_WRAP1 25u
#define TREQ_PERMANENT 0x3fu

#define SLICE_LED 0u
#define SLICE_PACE 1u
#define CH_DATA 0u
#define CH_RELOAD 1u
#define STEPS 256u

static uint32_t table[STEPS];
static const uint32_t *table_start = table;

static uint32_t level(uint32_t i) {
    uint32_t x = i < STEPS / 2u ? i * 2u : (STEPS - 1u - i) * 2u;
    return x * x / 16u; /* 0..4032, TOP 4095 */
}

static void table_init(void) {
    for (uint32_t i = 0; i < STEPS; i++) {
        uint32_t green = level(i);
        uint32_t red = level((i + STEPS / 2u) % STEPS);
        table[i] = (red << 16) | green;
    }
}

static void pwm_init(void) {
    unreset(RESETS_PWM);
    PWM_TOP(SLICE_LED) = 4095u;
    PWM_DIV(SLICE_LED) = 1u << 4;
    PWM_CC(SLICE_LED) = 0;
    /* 125MHz / 15 / 65536 = 127.2Hz */
    PWM_TOP(SLICE_PACE) = 0xffffu;
    PWM_DIV(SLICE_PACE) = 15u << 4;
    GPIO_CTRL(LED_GREEN) = FUNCSEL_PWM;
    GPIO_CTRL(LED_RED) = FUNCSEL_PWM;
    PWM_CSR(SLICE_LED) = PWM_CSR_EN | PWM_CSR_A_INV | PWM_CSR_B_INV;
    PWM_CSR(SLICE_PACE) = PWM_CSR_EN;
}

static void dma_init(void) {
    unreset(RESETS_DMA);
    DMA_READ_ADDR(CH_RELOAD) = (uint32_t) &table_start;
    DMA_WRITE_ADDR(CH_RELOAD) = DMA_AL3_READ_ADDR_TRIG_ADDR(CH_DATA);
    DMA_TRANS_COUNT(CH_RELOAD) = 1u;
    /* 별칭 1의 CTRL은 트리거가 아니라 설정만 한다 */
    DMA_AL1_CTRL(CH_RELOAD) = CTRL_EN | CTRL_SIZE_WORD | CTRL_TREQ(TREQ_PERMANENT)
                              | CTRL_CHAIN_TO(CH_RELOAD);

    DMA_READ_ADDR(CH_DATA) = (uint32_t) table;
    DMA_WRITE_ADDR(CH_DATA) = (uint32_t) &PWM_CC(SLICE_LED);
    DMA_TRANS_COUNT(CH_DATA) = STEPS;
    DMA_CTRL_TRIG(CH_DATA) = CTRL_EN | CTRL_SIZE_WORD | CTRL_INCR_READ
                             | CTRL_TREQ(TREQ_PWM_WRAP1) | CTRL_CHAIN_TO(CH_RELOAD);
}

int main(void) {
    board_init();
    table_init();
    pwm_init();
    dma_init();
    uart_puts("dma pwm start\r\n");

    uint32_t start = time_us();
    uint32_t last_index = 0;
    uint32_t last_wrap_us = start;
    for (uint32_t t = 0; t < 12u; t++) {
        sleep_us(500000u);
        uint32_t index = (DMA_READ_ADDR(CH_DATA) - (uint32_t) table) / 4u;
        uint32_t now = time_us();
        uart_puts("t ");
        uart_putdec((now - start) / 1000u);
        uart_puts(" ms, index ");
        uart_putdec(index);
        uart_puts(", CC 0x");
        uart_puthex(PWM_CC(SLICE_LED), 8);
        if (index < last_index) {
            uart_puts(", wrapped, ");
            uart_putdec((now - last_wrap_us) / 1000u);
            uart_puts(" ms since last");
            last_wrap_us = now;
        }
        uart_puts("\r\n");
        last_index = index;
    }
    uart_puts("cpu sleeps now\r\n");
    while (1) {
        __asm volatile("wfi");
    }
}
