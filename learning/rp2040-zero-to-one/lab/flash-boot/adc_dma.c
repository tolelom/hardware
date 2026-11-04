/*
 * ADC 연속 변환을 DMA로 받는다 (데이터시트 4.9.2.2, 4.9.2.4, 2.5).
 * 온도 센서(채널 4)를 START_MANY로 돌리고, FIFO가 차면 DREQ_ADC(36)로 DMA가
 * 16비트씩 SRAM에 옮긴다. 1만 개를 받는 데 걸린 시간으로 샘플링 속도를 잰다.
 * DIV 0이면 변환 하나에 clk_adc 96사이클(48MHz에서 500ksps), DIV n이면 n + 1사이클마다.
 */
#include "board.h"

#define ADC_BASE 0x4004c000u
#define ADC_CS REG(ADC_BASE + 0x00u)
#define ADC_FCS REG(ADC_BASE + 0x08u)
#define ADC_FIFO_ADDR (ADC_BASE + 0x0cu)
#define ADC_DIV REG(ADC_BASE + 0x10u)
#define ADC_CS_EN (1u << 0)
#define ADC_CS_TS_EN (1u << 1)
#define ADC_CS_START_MANY (1u << 3)
#define ADC_CS_READY (1u << 8)
#define ADC_CS_AINSEL(n) ((uint32_t) (n) << 12)
#define ADC_FCS_EN (1u << 0)
#define ADC_FCS_DREQ_EN (1u << 3)
#define ADC_FCS_UNDER (1u << 10)
#define ADC_FCS_OVER (1u << 11)
#define ADC_FCS_LEVEL(v) (((v) >> 16) & 0xfu)
#define ADC_FCS_THRESH(n) ((uint32_t) (n) << 24)

#define DMA_BASE 0x50000000u
#define DMA_READ_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x00u)
#define DMA_WRITE_ADDR(c) REG(DMA_BASE + (c) * 0x40u + 0x04u)
#define DMA_TRANS_COUNT(c) REG(DMA_BASE + (c) * 0x40u + 0x08u)
#define DMA_CTRL_TRIG(c) REG(DMA_BASE + (c) * 0x40u + 0x0cu)
#define CTRL_EN (1u << 0)
#define CTRL_SIZE_HALFWORD (1u << 2)
#define CTRL_INCR_WRITE (1u << 5)
#define CTRL_TREQ(n) ((uint32_t) (n) << 15)
#define CTRL_BUSY (1u << 24)
#define TREQ_ADC 36u

#define CH 0u
#define SAMPLES 10000u

static uint16_t samples[SAMPLES];

static void capture(uint32_t div_int) {
    ADC_CS = ADC_CS_EN | ADC_CS_TS_EN | ADC_CS_AINSEL(4);
    while (!(ADC_CS & ADC_CS_READY)) {
    }
    ADC_DIV = div_int << 8;
    /* 이전 시험에서 남은 FIFO 값을 버린다 */
    ADC_FCS = 0;
    while (ADC_FCS_LEVEL(ADC_FCS)) {
        (void) REG(ADC_FIFO_ADDR);
    }
    ADC_FCS = ADC_FCS_EN | ADC_FCS_DREQ_EN | ADC_FCS_THRESH(1)
              | ADC_FCS_OVER | ADC_FCS_UNDER;

    DMA_READ_ADDR(CH) = ADC_FIFO_ADDR;
    DMA_WRITE_ADDR(CH) = (uint32_t) samples;
    DMA_TRANS_COUNT(CH) = SAMPLES;
    DMA_CTRL_TRIG(CH) = CTRL_EN | CTRL_SIZE_HALFWORD | CTRL_INCR_WRITE
                        | CTRL_TREQ(TREQ_ADC) | (CH << 11);

    uint32_t start = time_us();
    ADC_CS |= ADC_CS_START_MANY;
    while (DMA_CTRL_TRIG(CH) & CTRL_BUSY) {
    }
    uint32_t took = time_us() - start;
    ADC_CS &= ~ADC_CS_START_MANY;
    uint32_t fcs = ADC_FCS;

    uint32_t min = 0xfffu;
    uint32_t max = 0;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < SAMPLES; i++) {
        uint32_t v = samples[i] & 0xfffu;
        sum += v;
        min = v < min ? v : min;
        max = v > max ? v : max;
    }

    uart_puts("DIV ");
    uart_putdec(div_int);
    uart_puts(": ");
    uart_putdec(SAMPLES);
    uart_puts(" samples in ");
    uart_putdec(took);
    uart_puts(" us = ");
    uart_putdec((uint32_t) ((SAMPLES * 1000u) / (took / 1000u)));
    uart_puts(" sps, raw mean ");
    uart_putdec(sum / SAMPLES);
    uart_puts(" min ");
    uart_putdec(min);
    uart_puts(" max ");
    uart_putdec(max);
    uart_puts((fcs & ADC_FCS_OVER) ? ", FIFO overflow" : ", no overflow");
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    clocks_48mhz_init();
    reset_block(RESETS_ADC | RESETS_DMA);
    uart_puts("adc dma start\r\n");
    capture(0u);
    capture(47999u);
    while (1) {
    }
}
