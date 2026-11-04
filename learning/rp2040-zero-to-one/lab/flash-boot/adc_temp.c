/*
 * ADC 채널 4(칩 내장 온도 센서)를 1초마다 읽어 온도로 바꾼다 (데이터시트 4.9.5).
 * clk_adc는 48MHz여야 하므로 PLL_USB를 켠다 (2.15 표, 4.9.2).
 * ADC 기준 전압은 ADC_AVDD이고, 3.3V라고 가정한다.
 */
#include "board.h"

#define ADC_BASE 0x4004c000u
#define ADC_CS REG(ADC_BASE + 0x00u)
#define ADC_RESULT REG(ADC_BASE + 0x04u)
#define ADC_CS_EN (1u << 0)
#define ADC_CS_TS_EN (1u << 1)
#define ADC_CS_START_ONCE (1u << 2)
#define ADC_CS_READY (1u << 8)
#define ADC_CS_ERR (1u << 9)
#define ADC_CS_AINSEL_LSB 12u
#define ADC_TEMP_CHANNEL 4u
#define SAMPLES 64u

#define TEMP_VBE_27C_UV 706000u  /* 27도에서 Vbe 0.706V */
#define TEMP_SLOPE_NV 1721000u   /* -1.721mV/도 = 1721000nV */

static uint32_t adc_read(uint32_t channel) {
    ADC_CS = ADC_CS_EN | ADC_CS_TS_EN | (channel << ADC_CS_AINSEL_LSB)
             | ADC_CS_START_ONCE;
    while (!(ADC_CS & ADC_CS_READY)) {
    }
    return ADC_RESULT & 0xfffu;
}

int main(void) {
    board_init();
    led_init(LED_RED);
    clocks_48mhz_init();
    reset_block(RESETS_ADC);
    ADC_CS = ADC_CS_EN | ADC_CS_TS_EN;
    while (!(ADC_CS & ADC_CS_READY)) {
    }
    uart_puts("adc temp start\r\n");

    while (1) {
        uint32_t sum = 0;
        uint32_t errors = 0;
        for (uint32_t i = 0; i < SAMPLES; i++) {
            sum += adc_read(ADC_TEMP_CHANNEL);
            errors += (ADC_CS & ADC_CS_ERR) ? 1u : 0u;
        }
        uint32_t raw_x64 = sum;  /* 12비트 값 64개의 합 = 평균 x 64 */
        /* uV = 합 x 3.3V / (4096 x 64). 3300000 / 2^18 x 256 = 3222.7 → 3223, 64비트 곱셈을 피한다 */
        uint32_t uv = (raw_x64 * 3223u) >> 8;
        /* T = 27 - (V - 0.706) / 0.001721. 단위: 밀리도 */
        int32_t diff_uv = (int32_t) TEMP_VBE_27C_UV - (int32_t) uv;
        int32_t milli = 27000 + diff_uv * 1000 / (int32_t) (TEMP_SLOPE_NV / 1000u);

        uart_puts("raw ");
        uart_putdec(raw_x64 / SAMPLES);
        uart_puts(", ");
        uart_putdec(uv);
        uart_puts(" uV, ");
        if (milli < 0) {
            uart_putc('-');
            milli = -milli;
        }
        uart_putdec((uint32_t) milli / 1000u);
        uart_putc('.');
        uint32_t frac = ((uint32_t) milli % 1000u) / 10u;
        if (frac < 10u) {
            uart_putc('0');
        }
        uart_putdec(frac);
        uart_puts(" C, err ");
        uart_putdec(errors);
        uart_puts("\r\n");
        SIO_GPIO_OUT_XOR = 1u << LED_RED;
        sleep_us(1000000u);
    }
}
