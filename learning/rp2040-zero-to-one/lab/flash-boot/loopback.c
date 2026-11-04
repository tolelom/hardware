/*
 * SPI0과 UART1 내부 루프백 (데이터시트 4.4, 4.2). 핀은 연결하지 않는다.
 * SPI0: SSPCR1.LBM이 송신 시프터 출력을 수신 시프터 입력에 칩 안에서 잇는다.
 *   clk_peri 12MHz에서 프레임 크기, 비트 속도, SPH를 바꿔 1024 프레임을 보내고 받는다.
 * UART1: CR.LBE(SIRTEST 0)가 UARTTXD를 UARTRXD로 돌린다. 보드레이트를 바꿔 256바이트.
 * 각 경우 받은 값을 비교하고 걸린 시간을 이론값과 비교한다.
 */
#include "board.h"

#define RESETS_SPI0 (1u << 16)
#define RESETS_UART1 (1u << 23)

#define SPI0_BASE 0x4003c000u
#define SSPCR0 REG(SPI0_BASE + 0x000u)
#define SSPCR1 REG(SPI0_BASE + 0x004u)
#define SSPDR REG(SPI0_BASE + 0x008u)
#define SSPSR REG(SPI0_BASE + 0x00cu)
#define SSPCPSR REG(SPI0_BASE + 0x010u)
#define SSPCR1_LBM (1u << 0)
#define SSPCR1_SSE (1u << 1)
#define SSPCR0_SPH (1u << 7)
#define SSPSR_TNF (1u << 1)
#define SSPSR_RNE (1u << 2)
#define SPI_FRAMES 1024u

#define UART1_BASE 0x40038000u
#define UART1_DR REG(UART1_BASE + 0x000u)
#define UART1_RSR REG(UART1_BASE + 0x004u)
#define UART1_FR REG(UART1_BASE + 0x018u)
#define UART1_IBRD REG(UART1_BASE + 0x024u)
#define UART1_FBRD REG(UART1_BASE + 0x028u)
#define UART1_LCR_H REG(UART1_BASE + 0x02cu)
#define UART1_CR REG(UART1_BASE + 0x030u)
#define UART_CR_UARTEN (1u << 0)
#define UART_CR_LBE (1u << 7)
#define UART_CR_TXE (1u << 8)
#define UART_CR_RXE (1u << 9)
#define UART_LCR_H_FEN (1u << 4)
#define UART_LCR_H_WLEN_8 (3u << 5)
#define UART_BYTES 256u
#define CLK_PERI_HZ XOSC_HZ

static uint32_t lcg(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

/* bits 크기 프레임 SPI_FRAMES개를 보내고 받는다. 틀린 수를 돌려주고 시간은 *us */
static uint32_t spi_run(uint32_t bits, uint32_t cpsdvsr, uint32_t scr, int sph,
                        uint32_t *us) {
    SSPCR1 = 0;
    SSPCPSR = cpsdvsr;
    SSPCR0 = (scr << 8) | (sph ? SSPCR0_SPH : 0u) | (bits - 1u);
    SSPCR1 = SSPCR1_LBM | SSPCR1_SSE;
    uint32_t mask = (1u << bits) - 1u;
    uint32_t tx_state = 7u;
    uint32_t rx_state = 7u;
    uint32_t tx_left = SPI_FRAMES;
    uint32_t rx_left = SPI_FRAMES;
    uint32_t errors = 0;
    uint32_t start = time_us();
    while (rx_left) {
        /* 보낸 수와 받은 수의 차이를 8(FIFO 깊이) 미만으로 둔다 */
        if (tx_left && (SSPSR & SSPSR_TNF) && rx_left - tx_left < 8u) {
            SSPDR = lcg(&tx_state) & mask;
            tx_left--;
        }
        if (SSPSR & SSPSR_RNE) {
            errors += (SSPDR & mask) != (lcg(&rx_state) & mask);
            rx_left--;
        }
    }
    *us = time_us() - start;
    SSPCR1 = 0;
    return errors;
}

static void spi_case(uint32_t bits, uint32_t cpsdvsr, uint32_t scr, int sph) {
    uint32_t us;
    uint32_t errors = spi_run(bits, cpsdvsr, scr, sph, &us);
    uint32_t bit_hz = CLK_PERI_HZ / (cpsdvsr * (1u + scr));
    uart_puts("spi ");
    uart_putdec(bits);
    uart_puts("-bit, ");
    uart_putdec(bit_hz / 1000u);
    uart_puts(" kbit/s, SPH ");
    uart_putdec((uint32_t) sph);
    uart_puts(": errors ");
    uart_putdec(errors);
    uart_puts(", ");
    uart_putdec(us);
    uart_puts(" us, ideal ");
    uart_putdec(SPI_FRAMES * bits * cpsdvsr * (1u + scr) / (CLK_PERI_HZ / 1000000u));
    uart_puts(" us\r\n");
}

/* SDK uart_set_baudrate와 같은 계산: 64배 분주값을 반올림해 정수부와 소수부 6비트 */
static uint32_t uart1_set_baud(uint32_t baud) {
    uint32_t div = 8u * CLK_PERI_HZ / baud;
    uint32_t ibrd = div >> 7;
    uint32_t fbrd = ((div & 0x7fu) + 1u) / 2u;
    if (ibrd == 0u) {
        ibrd = 1u;
        fbrd = 0u;
    }
    UART1_IBRD = ibrd;
    UART1_FBRD = fbrd;
    UART1_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;
    return 4u * CLK_PERI_HZ / (64u * ibrd + fbrd);
}

static void uart_case(uint32_t baud) {
    reset_block(RESETS_UART1);
    uint32_t actual = uart1_set_baud(baud);
    UART1_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE | UART_CR_LBE;
    uint32_t tx_state = 11u;
    uint32_t rx_state = 11u;
    uint32_t tx_left = UART_BYTES;
    uint32_t rx_left = UART_BYTES;
    uint32_t errors = 0;
    uint32_t flagged = 0;
    uint32_t start = time_us();
    while (rx_left) {
        if (tx_left && !(UART1_FR & UART_FR_TXFF) && rx_left - tx_left < 32u) {
            UART1_DR = lcg(&tx_state) & 0xffu;
            tx_left--;
        }
        if (!(UART1_FR & UART_FR_RXFE)) {
            uint32_t d = UART1_DR;
            flagged += (d & 0xf00u) != 0u; /* OE, BE, PE, FE */
            errors += (d & 0xffu) != (lcg(&rx_state) & 0xffu);
            rx_left--;
        }
    }
    uint32_t us = time_us() - start;
    UART1_CR = 0;
    uart_puts("uart1 ");
    uart_putdec(baud);
    uart_puts(" baud (actual ");
    uart_putdec(actual);
    uart_puts("): errors ");
    uart_putdec(errors);
    uart_puts(", error flags ");
    uart_putdec(flagged);
    uart_puts(", ");
    uart_putdec(us);
    uart_puts(" us, ideal ");
    uart_putdec(UART_BYTES * 10u * 1000000u / actual);
    uart_puts(" us\r\n");
}

int main(void) {
    board_init();
    reset_block(RESETS_SPI0);
    uart_puts("loopback start\r\n");
    spi_case(8, 2, 0, 0);
    spi_case(8, 2, 0, 1);
    spi_case(16, 2, 0, 0);
    spi_case(16, 2, 0, 1);
    spi_case(4, 2, 0, 1);
    spi_case(4, 2, 1, 1); /* 3 Mbit/s: 프레임당 CPU 시간이 두 배일 때 이론값에 붙는지 */
    spi_case(8, 2, 0, 1);
    spi_case(8, 2, 5, 0);
    spi_case(8, 2, 5, 1);
    uart_case(115200u);
    uart_case(460800u);
    uart_case(750000u);
    uart_case(1000000u);
    uart_puts("done\r\n");
    while (1) {
    }
}
