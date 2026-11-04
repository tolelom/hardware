/*
 * DMA → PIO → DMA (데이터시트 3, 2.5). PIO0과 PIO1의 SM0에 같은 3명령 프로그램을 둔다:
 *   pull block / mov isr, ~osr / push block   (받은 워드의 비트를 뒤집어 내보냄)
 * DMA 채널 하나가 SRAM에서 TX FIFO로, 다른 채널이 RX FIFO에서 SRAM으로 옮긴다.
 * 속도는 각 FIFO의 DREQ가 정한다. PIO0만, PIO1만, 둘 동시 세 경우의 클럭 수와 결과를 본다.
 */
#include "board.h"

#define RESETS_PIO1 (1u << 11)
#define PIO_BASE(n) (0x50200000u + (n) * 0x100000u)
#define PIO_CTRL(n) REG(PIO_BASE(n) + 0x000u)
#define PIO_TXF0_ADDR(n) (PIO_BASE(n) + 0x010u)
#define PIO_RXF0_ADDR(n) (PIO_BASE(n) + 0x020u)
#define PIO_INSTR_MEM(n, i) REG(PIO_BASE(n) + 0x048u + (i) * 4u)
#define PIO_SM0_CLKDIV(n) REG(PIO_BASE(n) + 0x0c8u)
#define PIO_SM0_EXECCTRL(n) REG(PIO_BASE(n) + 0x0ccu)
#define PIO_SM0_SHIFTCTRL(n) REG(PIO_BASE(n) + 0x0d0u)
#define PIO_SM0_INSTR(n) REG(PIO_BASE(n) + 0x0d8u)
#define EXEC_WRAP(top, bottom) (((uint32_t) (top) << 12) | ((uint32_t) (bottom) << 7))

#define DMA_BASE 0x50000000u
#define DMA_READ_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x00u)
#define DMA_WRITE_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x04u)
#define DMA_TRANS_COUNT(c) REG(DMA_BASE + (c) * 0x40u + 0x08u)
#define DMA_AL1_CTRL(c) REG(DMA_BASE + (c) * 0x40u + 0x10u)
#define DMA_MULTI_CHAN_TRIGGER REG(DMA_BASE + 0x430u)
#define CTRL_EN (1u << 0)
#define CTRL_SIZE_WORD (2u << 2)
#define CTRL_INCR_READ (1u << 4)
#define CTRL_INCR_WRITE (1u << 5)
#define CTRL_CHAIN_TO(c) ((uint32_t) (c) << 11)
#define CTRL_TREQ(n) ((uint32_t) (n) << 15)
#define CTRL_BUSY (1u << 24)
#define DREQ_PIO_TX0(n) ((n) * 8u)
#define DREQ_PIO_RX0(n) ((n) * 8u + 4u)

#define SYST_CSR REG(0xe000e010u)
#define SYST_RVR REG(0xe000e014u)
#define SYST_CVR REG(0xe000e018u)

#define WORDS 8192u

static const uint16_t program[] = {
    0x80a0, /* pull block */
    0xa0cf, /* mov isr, ~osr */
    0x8020, /* push block */
};

static uint32_t src[WORDS];
static uint32_t dst[2][WORDS];

static void pio_init(uint32_t n) {
    PIO_CTRL(n) = 0;
    for (uint32_t i = 0; i < sizeof program / sizeof program[0]; i++) {
        PIO_INSTR_MEM(n, i) = program[i];
    }
    PIO_SM0_CLKDIV(n) = 1u << 16;
    PIO_SM0_EXECCTRL(n) = EXEC_WRAP(2, 0);
    PIO_SM0_SHIFTCTRL(n) = 0;
    PIO_SM0_INSTR(n) = 0x0000; /* jmp 0 */
    PIO_CTRL(n) = 1u << 0;
}

/* 채널 2n: src → TX FIFO, 채널 2n+1: RX FIFO → dst[n]. 트리거는 하지 않는다 */
static uint32_t dma_setup(uint32_t n) {
    uint32_t tx = 2u * n;
    uint32_t rx = 2u * n + 1u;
    DMA_READ_ADDR(tx) = (uint32_t) src;
    DMA_WRITE_ADDR(tx) = PIO_TXF0_ADDR(n);
    DMA_TRANS_COUNT(tx) = WORDS;
    DMA_AL1_CTRL(tx) = CTRL_EN | CTRL_SIZE_WORD | CTRL_INCR_READ
                       | CTRL_TREQ(DREQ_PIO_TX0(n)) | CTRL_CHAIN_TO(tx);
    DMA_READ_ADDR(rx) = PIO_RXF0_ADDR(n);
    DMA_WRITE_ADDR(rx) = (uint32_t) dst[n];
    DMA_TRANS_COUNT(rx) = WORDS;
    DMA_AL1_CTRL(rx) = CTRL_EN | CTRL_SIZE_WORD | CTRL_INCR_WRITE
                       | CTRL_TREQ(DREQ_PIO_RX0(n)) | CTRL_CHAIN_TO(rx);
    return (1u << tx) | (1u << rx);
}

static uint32_t check(uint32_t n) {
    uint32_t errors = 0;
    for (uint32_t i = 0; i < WORDS; i++) {
        errors += dst[n][i] != ~src[i];
        dst[n][i] = 0;
    }
    return errors;
}

static void run(const char *label, uint32_t use0, uint32_t use1) {
    uint32_t mask = 0;
    if (use0) {
        pio_init(0);
        mask |= dma_setup(0);
    }
    if (use1) {
        pio_init(1);
        mask |= dma_setup(1);
    }
    __asm volatile("cpsid i");
    uint32_t before = SYST_CVR;
    DMA_MULTI_CHAN_TRIGGER = mask;
    for (uint32_t c = 0; c < 4u; c++) {
        if (mask & (1u << c)) {
            while (DMA_AL1_CTRL(c) & CTRL_BUSY) {
            }
        }
    }
    uint32_t cycles = (before - SYST_CVR) & 0xffffffu;
    __asm volatile("cpsie i");
    uart_puts(label);
    uart_putdec(cycles);
    uart_puts(" cycles for ");
    uart_putdec(WORDS);
    uart_puts(" words each");
    if (use0) {
        uart_puts(", pio0 errors ");
        uart_putdec(check(0));
    }
    if (use1) {
        uart_puts(", pio1 errors ");
        uart_putdec(check(1));
    }
    uart_puts("\r\n");
    PIO_CTRL(0) = 0;
    PIO_CTRL(1) = 0;
}

int main(void) {
    board_init();
    reset_block(RESETS_DMA | RESETS_PIO0 | RESETS_PIO1);
    SYST_RVR = 0xffffffu;
    SYST_CVR = 0;
    SYST_CSR = 5u;
    uint32_t x = 1u;
    for (uint32_t i = 0; i < WORDS; i++) {
        x = x * 1664525u + 1013904223u;
        src[i] = x;
    }
    uart_puts("pio dma start\r\n");
    run("pio0 only: ", 1, 0);
    run("pio1 only: ", 0, 1);
    run("pio0+pio1: ", 1, 1);
    uart_puts("done\r\n");
    while (1) {
    }
}
