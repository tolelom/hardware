/*
 * SRAM과 flash(XIP) 실행·읽기 속도 (데이터시트 2.6.3).
 * 같은 본문의 함수를 .text(flash)와 .ramfunc(SRAM)에 하나씩 두고 SysTick 클럭 수로 잰다.
 * 재는 함수 measure는 SRAM에 있어, 캐시를 비워도 재는 코드 자체는 영향을 받지 않는다.
 * 코드: SRAM / flash 캐시 hit / flash 캐시를 비운 직후 / 캐시를 끈 상태.
 * 데이터: flash 앞 8KB를 워드로 더한다. 캐시 alias(0x10…), 캐시를 거치지 않는 alias(0x13…),
 * SRAM 복사본을 비교한다. 각 경우 XIP CTR_HIT/CTR_ACC 증가분도 보낸다.
 */
#include "board.h"

#define XIP_CTRL REG(0x14000000u)
#define XIP_FLUSH REG(0x14000004u)
#define XIP_STAT REG(0x14000008u)
#define XIP_CTR_HIT REG(0x1400000cu)
#define XIP_CTR_ACC REG(0x14000010u)
#define XIP_CTRL_EN (1u << 0)
#define XIP_STAT_FLUSH_READY (1u << 0)
#define XIP_CACHED 0x10000000u
#define XIP_NOCACHE_NOALLOC 0x13000000u
#define DATA_WORDS 2048u

#define SYST_CSR REG(0xe000e010u)
#define SYST_RVR REG(0xe000e014u)
#define SYST_CVR REG(0xe000e018u)

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

typedef uint32_t (*work_fn)(uint32_t);

#define WORK_BODY                                    \
    uint32_t x = seed;                               \
    for (uint32_t i = 0; i < 200u; i++) {            \
        x ^= x << 13;                                \
        x ^= x >> 17;                                \
        x ^= x << 5;                                 \
        x += i;                                      \
    }                                                \
    return x;

__attribute__((noinline)) static uint32_t work_flash(uint32_t seed) {
    WORK_BODY
}

RAMFUNC static uint32_t work_sram(uint32_t seed) {
    WORK_BODY
}

#define SUM_BODY                                     \
    const volatile uint32_t *p = (const volatile uint32_t *) base; \
    uint32_t s = 0;                                  \
    for (uint32_t i = 0; i < DATA_WORDS; i++) {      \
        s += p[i];                                   \
    }                                                \
    return s;

RAMFUNC static uint32_t sum_words(uint32_t base) {
    SUM_BODY
}

/* 8바이트 줄의 앞 워드만, 또는 뒤 워드만 읽는다 */
RAMFUNC static uint32_t sum_even(uint32_t base) {
    const volatile uint32_t *p = (const volatile uint32_t *) base;
    uint32_t s = 0;
    for (uint32_t i = 0; i < DATA_WORDS; i += 2u) {
        s += p[i];
    }
    return s;
}

RAMFUNC static uint32_t sum_odd(uint32_t base) {
    const volatile uint32_t *p = (const volatile uint32_t *) base;
    uint32_t s = 0;
    for (uint32_t i = 1; i < DATA_WORDS; i += 2u) {
        s += p[i];
    }
    return s;
}

static uint32_t sram_copy[DATA_WORDS];

static volatile uint32_t result_hits;
static volatile uint32_t result_accesses;
static volatile uint32_t result_value;

/* flush가 1이면 캐시를 비우고 끝날 때까지 기다린 뒤 잰다 */
RAMFUNC static uint32_t measure(work_fn fn, uint32_t arg, int flush) {
    if (flush) {
        XIP_FLUSH = 1u;
        while (!(XIP_STAT & XIP_STAT_FLUSH_READY)) {
        }
    }
    XIP_CTR_HIT = 0;
    XIP_CTR_ACC = 0;
    uint32_t before = SYST_CVR;
    uint32_t value = fn(arg);
    uint32_t after = SYST_CVR;
    result_hits = XIP_CTR_HIT;
    result_accesses = XIP_CTR_ACC;
    result_value = value;
    return (before - after) & 0xffffffu;
}

static void report(const char *label, uint32_t cycles) {
    uart_puts(label);
    uart_putdec(cycles);
    uart_puts(" cycles, xip hit ");
    uart_putdec(result_hits);
    uart_puts(" / access ");
    uart_putdec(result_accesses);
    uart_puts(", value 0x");
    uart_puthex(result_value, 8);
    uart_puts("\r\n");
}

static void run(const char *label, work_fn fn, uint32_t arg, int flush) {
    __asm volatile("cpsid i");
    uint32_t cycles = measure(fn, arg, flush);
    __asm volatile("cpsie i");
    report(label, cycles);
}

int main(void) {
    board_init();
    SYST_RVR = 0xffffffu;
    SYST_CVR = 0;
    SYST_CSR = 5u;
    for (uint32_t i = 0; i < DATA_WORDS; i++) {
        sram_copy[i] = ((const uint32_t *) XIP_CACHED)[i];
    }
    uart_puts("xip speed start\r\n");

    run("code sram                ", work_sram, 1u, 0);
    run("code sram                ", work_sram, 1u, 0);
    run("code flash, after flush  ", work_flash, 1u, 1);
    run("code flash, cached       ", work_flash, 1u, 0);
    XIP_CTRL &= ~XIP_CTRL_EN;
    run("code flash, cache off    ", work_flash, 1u, 0);
    XIP_CTRL |= XIP_CTRL_EN;

    run("data 8KB sram copy       ", sum_words, (uint32_t) sram_copy, 0);
    run("data 8KB 0x10, flush     ", sum_words, XIP_CACHED, 1);
    run("data 8KB 0x10, cached    ", sum_words, XIP_CACHED, 0);
    run("data 8KB 0x13, no cache  ", sum_words, XIP_NOCACHE_NOALLOC, 0);
    run("data even words, flush   ", sum_even, XIP_CACHED, 1);
    run("data odd words after even", sum_odd, XIP_CACHED, 0);
    run("data 0x13 right after flush", sum_words, XIP_NOCACHE_NOALLOC, 1);
    uart_puts("done\r\n");
    while (1) {
    }
}
