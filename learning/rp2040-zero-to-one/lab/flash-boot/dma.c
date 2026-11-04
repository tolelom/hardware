/*
 * DMA 세 가지 (데이터시트 2.5).
 * 1. SRAM 4KiB 복사: CPU 루프와 걸린 시간 비교, 내용 대조
 * 2. sniffer CRC32: flash의 boot2 앞 252바이트를 읽혀 끝 4바이트(저장된 CRC)와 대조
 * 3. UART0 TX: DREQ_UART0_TX(20)로 속도를 맞춰 문자열을 DR에 쓴다
 */
#include "board.h"

#define DMA_BASE 0x50000000u
#define DMA_READ_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x00u)
#define DMA_WRITE_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x04u)
#define DMA_TRANS_COUNT(c) REG(DMA_BASE + (c) * 0x40u + 0x08u)
#define DMA_CTRL_TRIG(c) REG(DMA_BASE + (c) * 0x40u + 0x0cu)
#define DMA_SNIFF_CTRL REG(DMA_BASE + 0x434u)
#define DMA_SNIFF_DATA REG(DMA_BASE + 0x438u)

#define CTRL_EN (1u << 0)
#define CTRL_SIZE_BYTE (0u << 2)
#define CTRL_SIZE_WORD (2u << 2)
#define CTRL_INCR_READ (1u << 4)
#define CTRL_INCR_WRITE (1u << 5)
#define CTRL_CHAIN_TO(c) ((c) << 11) /* 자기 자신이면 chain 없음 */
#define CTRL_TREQ(n) ((n) << 15)
#define CTRL_SNIFF_EN (1u << 23)
#define CTRL_BUSY (1u << 24)
#define CTRL_AHB_ERROR (1u << 31)
#define TREQ_UART0_TX 20u
#define TREQ_PERMANENT 0x3fu

#define UART_DMACR_TXDMAE (1u << 1)
#define BOOT2 0x10000000u
#define WORDS 1024u
#define CH 0u

static uint32_t src[WORDS];
static uint32_t dst[WORDS];

static uint32_t dma_run(uint32_t from, uint32_t to, uint32_t count,
                        uint32_t ctrl) {
    DMA_READ_ADDR(CH) = from;
    DMA_WRITE_ADDR(CH) = to;
    DMA_TRANS_COUNT(CH) = count;
    uint32_t start = time_us();
    DMA_CTRL_TRIG(CH) = ctrl | CTRL_EN | CTRL_CHAIN_TO(CH);
    while (DMA_CTRL_TRIG(CH) & CTRL_BUSY) {
    }
    uint32_t took = time_us() - start;
    if (DMA_CTRL_TRIG(CH) & CTRL_AHB_ERROR) {
        uart_puts("dma bus error\r\n");
    }
    return took;
}

static void copy_test(void) {
    for (uint32_t i = 0; i < WORDS; i++) {
        src[i] = i * 0x9e3779b9u;
        dst[i] = 0;
    }
    uint32_t dma_us = dma_run((uint32_t) src, (uint32_t) dst, WORDS,
                              CTRL_SIZE_WORD | CTRL_INCR_READ | CTRL_INCR_WRITE
                                  | CTRL_TREQ(TREQ_PERMANENT));
    uint32_t bad = 0;
    for (uint32_t i = 0; i < WORDS; i++) {
        bad += dst[i] != src[i];
    }

    /* volatile로 두어 컴파일러가 memcpy 호출로 바꾸지 못하게 한다 */
    volatile uint32_t *s = src;
    volatile uint32_t *d = dst;
    uint32_t start = time_us();
    for (uint32_t i = 0; i < WORDS; i++) {
        d[i] = s[i];
    }
    uint32_t cpu_us = time_us() - start;

    uart_puts("copy 4KiB: dma ");
    uart_putdec(dma_us);
    uart_puts(" us, cpu ");
    uart_putdec(cpu_us);
    uart_puts(" us, mismatch ");
    uart_putdec(bad);
    uart_puts("\r\n");
}

static void crc_test(void) {
    static uint32_t sink;
    DMA_SNIFF_DATA = 0xffffffffu;
    DMA_SNIFF_CTRL = 1u | (CH << 1); /* EN, DMACH, CALC 0 = CRC32 MSB-first */
    dma_run(BOOT2, (uint32_t) &sink, 252u,
            CTRL_SIZE_BYTE | CTRL_INCR_READ | CTRL_TREQ(TREQ_PERMANENT)
                | CTRL_SNIFF_EN);
    uint32_t crc = DMA_SNIFF_DATA;
    DMA_SNIFF_CTRL = 0;
    uint32_t stored = REG(BOOT2 + 252u);
    uart_puts("boot2 crc: sniffer 0x");
    uart_puthex(crc, 8);
    uart_puts(", stored 0x");
    uart_puthex(stored, 8);
    uart_puts(crc == stored ? " match\r\n" : " differ\r\n");
}

static void uart_test(void) {
    static const char msg[] = "this line was written to UART0 by DMA\r\n";
    uart_flush();
    UART0_DMACR = UART_DMACR_TXDMAE;
    uint32_t took = dma_run((uint32_t) msg, (uint32_t) &UART0_DR,
                            sizeof msg - 1u,
                            CTRL_SIZE_BYTE | CTRL_INCR_READ
                                | CTRL_TREQ(TREQ_UART0_TX));
    UART0_DMACR = 0;
    uart_flush();
    uart_puts("uart dma: ");
    uart_putdec(sizeof msg - 1u);
    uart_puts(" bytes, ");
    uart_putdec(took);
    uart_puts(" us until channel idle\r\n");
}

int main(void) {
    board_init();
    unreset(RESETS_DMA);
    uart_puts("dma start\r\n");
    copy_test();
    crc_test();
    uart_test();
    while (1) {
    }
}
