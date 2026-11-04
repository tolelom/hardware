/*
 * core1이 도는 동안 core0이 flash를 지우고 쓴다 (데이터시트 2.8.3.1.3).
 * flash 명령 동안 XIP를 쓸 수 없으므로 core1의 코드·스택·데이터도 모두 SRAM에 둔다.
 * core1은 TIMER를 읽으며 루프 횟수와 루프 사이 최대 간격을 센다. core0은 같은 시간
 * 길이의 평소 구간과 flash 지우기·쓰기 구간에서 그 값을 비교한다.
 * 섹터는 offset 0x1fe000이다. 0x1ff000은 flash_counter.c가 쓴다.
 * core1이 flash를 읽게 하는 경우는 동작이 정해지지 않았고 보드가 멈출 수 있어 시험하지 않는다.
 */
#include "board.h"

#define XIP_BASE 0x10000000u
#define SECTOR_OFFSET 0x1fe000u
#define SECTOR_SIZE 4096u
#define PAGE_SIZE 256u
#define BOOT2_SIZE 256u
#define M0PLUS_VTOR REG(0xe000ed08u)
#define SIO_FIFO_ST REG(SIO_BASE + 0x050u)
#define SIO_FIFO_WR REG(SIO_BASE + 0x054u)
#define SIO_FIFO_RD REG(SIO_BASE + 0x058u)
#define FIFO_ST_VLD (1u << 0)
#define FIFO_ST_RDY (1u << 1)
#define CORE1_STACK_WORDS 256u

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

typedef void (*rom_void_fn)(void);
typedef void (*rom_erase_fn)(uint32_t, uint32_t, uint32_t, uint8_t);
typedef void (*rom_program_fn)(uint32_t, const uint8_t *, uint32_t);

static struct {
    rom_void_fn connect;
    rom_void_fn exit_xip;
    rom_erase_fn erase;
    rom_program_fn program;
    rom_void_fn flush_cache;
} rom;
static uint32_t boot2_copy[BOOT2_SIZE / 4u];
static uint32_t page[PAGE_SIZE / 4u];
static uint32_t core1_stack[CORE1_STACK_WORDS] __attribute__((aligned(8)));

static volatile uint32_t core1_loops;
static volatile uint32_t core1_max_gap;
static volatile uint32_t core1_restart;

RAMFUNC static void core1_main(void) {
    uint32_t last = TIMER_TIMERAWL;
    while (1) {
        uint32_t now = TIMER_TIMERAWL;
        if (core1_restart) {
            core1_loops = 0;
            core1_max_gap = 0;
            core1_restart = 0;
        } else if (now - last > core1_max_gap) {
            core1_max_gap = now - last;
        }
        last = now;
        core1_loops++;
    }
}

RAMFUNC static void flash_erase_program(void) {
    rom.connect();
    rom.exit_xip();
    rom.erase(SECTOR_OFFSET, SECTOR_SIZE, 1u << 16, 0xd8u);
    rom.program(SECTOR_OFFSET, (const uint8_t *) page, PAGE_SIZE);
    rom.flush_cache();
    ((rom_void_fn) ((uint32_t) boot2_copy + 1u))();
}

static void fifo_drain(void) {
    while (SIO_FIFO_ST & FIFO_ST_VLD) {
        (void) SIO_FIFO_RD;
    }
}

static void fifo_push(uint32_t value) {
    while (!(SIO_FIFO_ST & FIFO_ST_RDY)) {
    }
    SIO_FIFO_WR = value;
    __asm volatile("sev");
}

static uint32_t fifo_pop(void) {
    while (!(SIO_FIFO_ST & FIFO_ST_VLD)) {
        __asm volatile("wfe");
    }
    return SIO_FIFO_RD;
}

static void core1_launch(uint32_t vector_table, uint32_t sp, void (*entry)(void)) {
    const uint32_t sequence[] = {
        0u, 0u, 1u, vector_table, sp, (uint32_t) entry,
    };
    unsigned step = 0;
    while (step < sizeof sequence / sizeof sequence[0]) {
        uint32_t command = sequence[step];
        if (command == 0u) {
            fifo_drain();
            __asm volatile("sev");
        }
        fifo_push(command);
        step = (fifo_pop() == command) ? step + 1u : 0u;
    }
}

static void restart_core1_counts(void) {
    core1_restart = 1;
    while (core1_restart) {
    }
}

static void report(const char *label, uint32_t took, uint32_t loops, uint32_t gap) {
    uart_puts(label);
    uart_putdec(took);
    uart_puts(" us, core1 loops ");
    uart_putdec(loops);
    uart_puts(" (");
    uart_putdec(took ? loops / took : 0u);
    uart_puts(" per us), max gap ");
    uart_putdec(gap);
    uart_puts(" us\r\n");
}

int main(void) {
    board_init();
    rom.connect = (rom_void_fn) rom_func_lookup(ROM_CODE('I', 'F'));
    rom.exit_xip = (rom_void_fn) rom_func_lookup(ROM_CODE('E', 'X'));
    rom.erase = (rom_erase_fn) rom_func_lookup(ROM_CODE('R', 'E'));
    rom.program = (rom_program_fn) rom_func_lookup(ROM_CODE('R', 'P'));
    rom.flush_cache = (rom_void_fn) rom_func_lookup(ROM_CODE('F', 'C'));
    for (uint32_t i = 0; i < BOOT2_SIZE / 4u; i++) {
        boot2_copy[i] = ((const uint32_t *) XIP_BASE)[i];
    }
    uint32_t stamp = time_us();
    for (uint32_t i = 0; i < PAGE_SIZE / 4u; i++) {
        page[i] = stamp + i;
    }
    uart_puts("flash with core1 start, core1 entry 0x");
    uart_puthex((uint32_t) core1_main, 8);
    uart_puts("\r\n");
    core1_launch(M0PLUS_VTOR, (uint32_t) &core1_stack[CORE1_STACK_WORDS], core1_main);
    sleep_us(1000u);

    restart_core1_counts();
    uint32_t start = time_us();
    sleep_us(30000u);
    uint32_t took = time_us() - start;
    report("normal 30 ms:        ", took, core1_loops, core1_max_gap);

    restart_core1_counts();
    start = time_us();
    __asm volatile("cpsid i");
    flash_erase_program();
    __asm volatile("cpsie i");
    took = time_us() - start;
    report("erase+program:       ", took, core1_loops, core1_max_gap);

    const volatile uint32_t *sector = (const volatile uint32_t *) (XIP_BASE + SECTOR_OFFSET);
    uint32_t page_ok = 0;
    uint32_t erased_ok = 0;
    for (uint32_t i = 0; i < SECTOR_SIZE / 4u; i++) {
        if (i < PAGE_SIZE / 4u) {
            page_ok += sector[i] == stamp + i;
        } else {
            erased_ok += sector[i] == 0xffffffffu;
        }
    }
    uart_puts("readback: page words match ");
    uart_putdec(page_ok);
    uart_puts("/64, erased words ");
    uart_putdec(erased_ok);
    uart_puts("/960\r\ndone\r\n");
    while (1) {
    }
}
