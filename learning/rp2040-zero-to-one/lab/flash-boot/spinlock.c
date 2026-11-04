/*
 * 두 코어와 하드웨어 spinlock (데이터시트 2.3.1.3).
 * 1. 공유 변수를 두 코어가 각각 COUNT번 1씩 올린다. lock 없이 한 번, SPINLOCK0으로
 *    감싸서 한 번. M0+에는 원자적 읽기-수정-쓰기 명령이 없어 lock이 없으면 덧셈이 샐 수 있다.
 * 2. 두 코어가 UART에 한 줄씩 번갈아 쓴다. 한 줄 전체를 SPINLOCK1로 감싼다.
 * core1 기동 순서는 core1.c와 같다 (2.8.2).
 */
#include "board.h"

#define SIO_FIFO_ST REG(SIO_BASE + 0x050u)
#define SIO_FIFO_WR REG(SIO_BASE + 0x054u)
#define SIO_FIFO_RD REG(SIO_BASE + 0x058u)
#define FIFO_ST_VLD (1u << 0)
#define FIFO_ST_RDY (1u << 1)
#define SIO_SPINLOCK(n) REG(SIO_BASE + 0x100u + (n) * 4u)
#define M0PLUS_VTOR REG(0xe000ed08u)

#define LOCK_COUNTER 0u
#define LOCK_UART 1u
#define COUNT 100000u
#define LINES 10u
#define CORE1_STACK_WORDS 256u

enum { PHASE_ADD_UNLOCKED = 1, PHASE_ADD_LOCKED, PHASE_PRINT };

static volatile uint32_t shared;
static volatile uint32_t span[2][2]; /* 코어별 add 시작·끝 시각 (us) */

static void lock(uint32_t n) {
    while (SIO_SPINLOCK(n) == 0u) {
    }
    __asm volatile("" ::: "memory");
}

static void unlock(uint32_t n) {
    __asm volatile("" ::: "memory");
    SIO_SPINLOCK(n) = 0u;
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

/* 같은 코드를 같은 순간 시작한 두 코어는 flash에서도 SRAM에서도 유실이 0이었다.
   박자가 고정되어 읽기~쓰기 창이 안 겹친 것으로 보고, 반복마다 지연을 흩뜨린다 */
__attribute__((section(".ramfunc"), noinline, long_call))
static void add(int locked) {
    uint32_t core = SIO_CPUID;
    uint32_t seed = core + 1u;
    span[core][0] = time_us();
    for (uint32_t i = 0; i < COUNT; i++) {
        if (locked) {
            lock(LOCK_COUNTER);
        }
        shared = shared + 1u;
        if (locked) {
            unlock(LOCK_COUNTER);
        }
        /* 코어마다 다른 씨앗의 LCG로 0~7회 지연해 두 코어의 박자를 흩뜨린다 */
        seed = seed * 1664525u + 1013904223u;
        for (volatile uint32_t j = 0; j < (seed >> 29); j++) {
        }
    }
    span[core][1] = time_us();
}

static void print_lines(void) {
    uint32_t core = SIO_CPUID;
    for (uint32_t i = 0; i < LINES; i++) {
        lock(LOCK_UART);
        uart_puts("core");
        uart_putdec(core);
        uart_puts(" line ");
        uart_putdec(i);
        uart_puts("\r\n");
        unlock(LOCK_UART);
    }
}

static void run(uint32_t phase) {
    if (phase == PHASE_PRINT) {
        print_lines();
    } else {
        add(phase == PHASE_ADD_LOCKED);
    }
}

static uint32_t core1_stack[CORE1_STACK_WORDS] __attribute__((aligned(8)));

static void core1_main(void) {
    while (1) {
        uint32_t phase = fifo_pop();
        run(phase);
        fifo_push(phase);
    }
}

/* 두 코어가 같은 일을 하고 core1이 끝났다고 알릴 때까지 기다린다 */
static uint32_t both(uint32_t phase) {
    uint32_t start = time_us();
    fifo_push(phase);
    run(phase);
    while (fifo_pop() != phase) {
    }
    return time_us() - start;
}

static void report_add(const char *name, uint32_t phase) {
    shared = 0;
    uint32_t start = time_us();
    uint32_t took = both(phase);
    uart_puts(name);
    uart_putdec(shared);
    uart_puts(" / ");
    uart_putdec(2u * COUNT);
    uart_puts(", ");
    uart_putdec(took);
    uart_puts(" us\r\n    core0 ");
    uart_putdec(span[0][0] - start);
    uart_puts("..");
    uart_putdec(span[0][1] - start);
    uart_puts(" us, core1 ");
    uart_putdec(span[1][0] - start);
    uart_puts("..");
    uart_putdec(span[1][1] - start);
    uart_puts(" us\r\n");
}

int main(void) {
    board_init();
    unlock(LOCK_COUNTER);
    unlock(LOCK_UART);
    core1_launch(M0PLUS_VTOR,
                 (uint32_t) &core1_stack[CORE1_STACK_WORDS], core1_main);
    uart_puts("spinlock start\r\n");

    report_add("add without lock: ", PHASE_ADD_UNLOCKED);
    report_add("add with lock:    ", PHASE_ADD_LOCKED);
    both(PHASE_PRINT);
    uart_puts("done\r\n");
    while (1) {
    }
}
