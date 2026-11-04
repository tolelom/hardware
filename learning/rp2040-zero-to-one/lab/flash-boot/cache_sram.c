/*
 * XIP 캐시를 SRAM으로 쓰기 (데이터시트 2.6.3.1, RP2040-E9).
 * XIP CTRL.EN을 0으로 두면 캐시 데이터 메모리 16KB가 0x15000000에 SRAM처럼 보인다.
 * 1회째 부팅: 캐시를 끄고 16KB에 xorshift 값을 쓰고 읽어 비교한 뒤, 같은 일을 main SRAM과
 *   속도 비교한다. 그 뒤 watchdog로 리셋한다.
 * 2회째 부팅: 리셋 뒤 CTRL 값을 보고(E9: watchdog 리셋이 캐시를 다시 켠다), 캐시를 끄고
 *   0x15000000의 내용이 몇 워드 남았는지 센다. 이 프로그램은 flash에서 실행되므로
 *   리셋 뒤 부팅과 main까지의 실행이 캐시를 채워 일부 내용을 덮어쓴다.
 * 끝나면 캐시를 다시 켜고 FLUSH한다. 캐시를 SRAM으로 쓴 뒤의 tag는 의미가 없기 때문이다.
 */
#include "board.h"

#define XIP_CTRL REG(0x14000000u)
#define XIP_FLUSH REG(0x14000004u)
#define XIP_CTRL_EN (1u << 0)
#define CACHE_SRAM 0x15000000u
#define WORDS 4096u

#define SYST_CSR REG(0xe000e010u)
#define SYST_RVR REG(0xe000e014u)
#define SYST_CVR REG(0xe000e018u)

#define WATCHDOG_BASE 0x40058000u
#define WATCHDOG_CTRL REG(WATCHDOG_BASE + 0x00u)
#define WATCHDOG_REASON REG(WATCHDOG_BASE + 0x08u)
#define WATCHDOG_SCRATCH(n) REG(WATCHDOG_BASE + 0x0cu + (n) * 4u)
#define WATCHDOG_CTRL_ENABLE (1u << 30)
#define WATCHDOG_CTRL_TRIGGER (1u << 31)
#define PSM_WDSEL REG(0x40010000u + 0x08u)
#define PSM_WDSEL_NO_OSC 0x1fffcu
#define MAGIC 0xcac4e5a0u
#define SEED 0x2545f491u

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

static uint32_t sram_buffer[WORDS];
static volatile uint32_t last_cycles;

/* p[0..WORDS)에 xorshift 값을 쓰고 다시 읽어 다른 워드 수를 돌려준다. 걸린 클럭은 last_cycles */
RAMFUNC static uint32_t fill_check(volatile uint32_t *p) {
    uint32_t before = SYST_CVR;
    uint32_t x = SEED;
    for (uint32_t i = 0; i < WORDS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        p[i] = x;
    }
    x = SEED;
    uint32_t errors = 0;
    for (uint32_t i = 0; i < WORDS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        errors += p[i] != x;
    }
    last_cycles = (before - SYST_CVR) & 0xffffffu;
    return errors;
}

/* 첫 워드부터 같은 xorshift 순서로 맞는 워드 수 */
RAMFUNC static uint32_t count_kept(const volatile uint32_t *p, uint32_t *first_bad) {
    uint32_t x = SEED;
    uint32_t kept = 0;
    *first_bad = WORDS;
    for (uint32_t i = 0; i < WORDS; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        if (p[i] == x) {
            kept++;
        } else if (*first_bad == WORDS) {
            *first_bad = i;
        }
    }
    return kept;
}

static void cache_on_and_flush(void) {
    XIP_CTRL |= XIP_CTRL_EN;
    XIP_FLUSH = 1u;
    (void) XIP_FLUSH; /* 읽기는 flush가 끝날 때까지 버스를 잡는다 (Table 156) */
}

static void print_ctrl(const char *label) {
    uart_puts(label);
    uart_puts("XIP CTRL 0x");
    uart_puthex(XIP_CTRL, 1);
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    SYST_RVR = 0xffffffu;
    SYST_CVR = 0;
    SYST_CSR = 5u;

    if (WATCHDOG_SCRATCH(0) == MAGIC && (WATCHDOG_REASON & 2u)) {
        WATCHDOG_SCRATCH(0) = 0;
        print_ctrl("boot 2 after watchdog reset: ");
        (void) XIP_FLUSH;
        XIP_CTRL &= ~XIP_CTRL_EN;
        uint32_t first_bad;
        uint32_t kept = count_kept((const volatile uint32_t *) CACHE_SRAM, &first_bad);
        cache_on_and_flush();
        uart_puts("cache-as-SRAM words kept ");
        uart_putdec(kept);
        uart_puts("/4096, first changed word ");
        uart_putdec(first_bad);
        uart_puts("\r\ndone\r\n");
        while (1) {
        }
    }

    print_ctrl("boot 1: ");
    __asm volatile("cpsid i");
    uint32_t sram_errors = fill_check(sram_buffer);
    uint32_t sram_cycles = last_cycles;
    XIP_CTRL &= ~XIP_CTRL_EN;
    uint32_t cache_errors = fill_check((volatile uint32_t *) CACHE_SRAM);
    uint32_t cache_cycles = last_cycles;
    __asm volatile("cpsie i");
    print_ctrl("cache disabled: ");
    uart_puts("main SRAM 16KB write+check: errors ");
    uart_putdec(sram_errors);
    uart_puts(", ");
    uart_putdec(sram_cycles);
    uart_puts(" cycles\r\n");
    uart_puts("0x15000000 16KB write+check: errors ");
    uart_putdec(cache_errors);
    uart_puts(", ");
    uart_putdec(cache_cycles);
    uart_puts(" cycles\r\nwatchdog reset with cache still disabled\r\n");
    uart_flush();

    WATCHDOG_SCRATCH(0) = MAGIC;
    PSM_WDSEL = PSM_WDSEL_NO_OSC;
    WATCHDOG_CTRL = WATCHDOG_CTRL_TRIGGER;
    while (1) {
    }
}
