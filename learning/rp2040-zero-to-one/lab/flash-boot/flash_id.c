/*
 * flash 칩에 직접 명령을 보내 JEDEC ID(0x9f), SFDP(0x5a), 고유 ID(0x4b)를 읽는다.
 * SSI(4.10)로 바이트를 주고받고 CS는 IO_QSPI SS_CTRL OUTOVER로 직접 잡는다.
 * 순서는 tools/jedec-id/main.c와 같다. 명령 동안 XIP를 쓸 수 없으므로 전송 함수는
 * SRAM에 두고 인터럽트를 끈다. 끝나면 flush_cache 뒤 SRAM에 복사한 boot2로 XIP를 되살린다.
 * 받은 바이트는 16진수로 UART에 보내고 해석은 PC에서 한다.
 */
#include "board.h"

#define XIP_BASE 0x10000000u
#define QSPI_SS_CTRL REG(0x40018000u + 0x0cu)
#define QSPI_OUTOVER_MASK (3u << 8)
#define QSPI_OUTOVER_LOW (2u << 8)
#define QSPI_OUTOVER_HIGH (3u << 8)
#define SSI_SR REG(0x18000000u + 0x28u)
#define SSI_DR0 REG(0x18000000u + 0x60u)
#define SSI_SR_TFNF (1u << 1)
#define SSI_SR_RFNE (1u << 3)
#define BOOT2_SIZE 256u
#define MAX_BYTES 300u

typedef void (*rom_void_fn)(void);

static rom_void_fn rom_connect;
static rom_void_fn rom_exit_xip;
static rom_void_fn rom_flush_cache;
static uint32_t boot2_copy[BOOT2_SIZE / 4u];
static uint8_t tx[MAX_BYTES];
static uint8_t rx[MAX_BYTES];

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

/* tx를 count바이트 보내며 같은 수를 rx로 받는다. RX FIFO가 넘치지 않게 보낸 수와 받은 수의
   차이를 14 미만으로 둔다 (SDK flash_do_cmd와 같은 방식) */
RAMFUNC static void flash_cmd(uint32_t count) {
    rom_connect();
    rom_exit_xip();
    QSPI_SS_CTRL = (QSPI_SS_CTRL & ~QSPI_OUTOVER_MASK) | QSPI_OUTOVER_LOW;
    uint32_t tx_left = count;
    uint32_t rx_left = count;
    const uint8_t *t = tx;
    uint8_t *r = rx;
    while (tx_left || rx_left) {
        uint32_t status = SSI_SR;
        if ((status & SSI_SR_TFNF) && tx_left && rx_left - tx_left < 14u) {
            SSI_DR0 = *t++;
            tx_left--;
        }
        if ((status & SSI_SR_RFNE) && rx_left) {
            *r++ = (uint8_t) SSI_DR0;
            rx_left--;
        }
    }
    QSPI_SS_CTRL = (QSPI_SS_CTRL & ~QSPI_OUTOVER_MASK) | QSPI_OUTOVER_HIGH;
    rom_flush_cache();
    ((rom_void_fn) ((uint32_t) boot2_copy + 1u))();
}

/* header 바이트를 앞에 두고 뒤는 0으로 채워 보내고, 받은 것 중 header 뒤 n바이트를 보낸다 */
static void run(const char *name, const uint8_t *header, uint32_t header_len,
                uint32_t n) {
    for (uint32_t i = 0; i < header_len + n; i++) {
        tx[i] = i < header_len ? header[i] : 0u;
    }
    __asm volatile("cpsid i");
    flash_cmd(header_len + n);
    __asm volatile("cpsie i");
    uart_puts(name);
    for (uint32_t i = 0; i < n; i++) {
        uart_putc((i % 16u) ? ' ' : (i ? '\n' : ' '));
        if (i % 16u == 0 && i) {
            uart_puts("   ");
        }
        uart_puthex(rx[header_len + i], 2);
    }
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    rom_connect = (rom_void_fn) rom_func_lookup(ROM_CODE('I', 'F'));
    rom_exit_xip = (rom_void_fn) rom_func_lookup(ROM_CODE('E', 'X'));
    rom_flush_cache = (rom_void_fn) rom_func_lookup(ROM_CODE('F', 'C'));
    for (uint32_t i = 0; i < BOOT2_SIZE / 4u; i++) {
        boot2_copy[i] = ((const uint32_t *) XIP_BASE)[i];
    }
    uart_puts("flash id start\r\n");

    static const uint8_t jedec[] = {0x9f};
    static const uint8_t sfdp[] = {0x5a, 0x00, 0x00, 0x00, 0x00}; /* 주소 3바이트 + dummy 1 */
    static const uint8_t uid[] = {0x4b, 0x00, 0x00, 0x00, 0x00};  /* dummy 4바이트 */
    run("jedec", jedec, sizeof jedec, 3);
    run("sfdp ", sfdp, sizeof sfdp, 256);
    run("uid  ", uid, sizeof uid, 16);
    uart_puts("xip still works: boot2 word 0x");
    uart_puthex(((volatile uint32_t *) XIP_BASE)[63], 8);
    uart_puts("\r\ndone\r\n");
    while (1) {
    }
}
