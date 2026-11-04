/*
 * SysTick + PendSV로 두 작업을 번갈아 돌린다 (데이터시트 2.4.8 M0PLUS 레지스터).
 * SysTick이 1ms마다 PendSV를 걸고, PendSV가 r4~r11과 PSP를 바꿔 다른 작업으로 간다.
 * 예외 진입 때 r0~r3, r12, lr, pc, xPSR은 하드웨어가 PSP에 쌓으므로 나머지 8개만 저장한다.
 * 이 순서와 EXC_RETURN 0xfffffffd(thread 모드, PSP)는 ARMv6-M 규칙이다.
 *
 * main은 PSP로 옮겨 작업 0이 되고, 작업 1은 미리 만든 스택 프레임에서 시작한다.
 * 작업 1은 카운터만 올리고, 작업 0은 자기 카운터를 올리다 1초마다 두 카운터와
 * SysTick 때 어느 작업이 돌고 있었는지 센 횟수를 보낸다.
 * SIO 나눗셈기는 작업 0만 쓴다. 두 작업이 함께 쓰면 전환 때 나눗셈기 상태도 저장해야 한다.
 */
#include "board.h"

#define SYST_CSR REG(0xe000e010u)
#define SYST_RVR REG(0xe000e014u)
#define SYST_CVR REG(0xe000e018u)
#define SYST_CSR_ENABLE (1u << 0)
#define SYST_CSR_TICKINT (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define ICSR REG(0xe000ed04u)
#define ICSR_PENDSVSET (1u << 28)
#define SHPR3 REG(0xe000ed20u)
#define PRI_LOWEST 3u

#define STACK_WORDS 256u
#define TICK_CYCLES (CLK_SYS_HZ / 1000u)

static uint32_t stack0[STACK_WORDS] __attribute__((aligned(8)));
static uint32_t stack1[STACK_WORDS] __attribute__((aligned(8)));

/* PendSV가 쓰는 상태. 어셈블리에서 이름으로 참조한다 */
uint32_t task_sp[2];
uint32_t task_current;

static volatile uint32_t count[2];
static volatile uint32_t slices[2]; /* SysTick 때 돌고 있던 작업별 횟수 */
static volatile uint32_t task1_sp_seen;

void isr_systick(void) {
    slices[task_current]++;
    ICSR = ICSR_PENDSVSET;
}

__attribute__((naked)) void isr_pendsv(void) {
    __asm volatile(
        ".syntax unified\n"
        "mrs r0, psp\n"
        "subs r0, #32\n"
        "mov r12, r0\n"
        "stmia r0!, {r4-r7}\n"
        "mov r4, r8\n"
        "mov r5, r9\n"
        "mov r6, r10\n"
        "mov r7, r11\n"
        "stmia r0!, {r4-r7}\n"
        "ldr r2, =task_current\n"
        "ldr r3, [r2]\n"
        "ldr r1, =task_sp\n"
        "lsls r3, r3, #2\n"
        "mov r0, r12\n"
        "str r0, [r1, r3]\n"
        "movs r0, #4\n"
        "eors r3, r0\n"
        "lsrs r0, r3, #2\n"
        "str r0, [r2]\n"
        "ldr r0, [r1, r3]\n"
        "adds r0, #16\n"
        "ldmia r0!, {r4-r7}\n"
        "mov r8, r4\n"
        "mov r9, r5\n"
        "mov r10, r6\n"
        "mov r11, r7\n"
        "subs r0, #32\n"
        "ldmia r0!, {r4-r7}\n"
        "adds r0, #16\n"
        "msr psp, r0\n"
        "bx lr\n");
}

static void task1(void) {
    uint32_t local;
    task1_sp_seen = (uint32_t) &local;
    while (1) {
        count[1]++;
    }
}

/* 작업 1의 첫 스택: 하드웨어 프레임 8워드 + 소프트웨어 저장분 8워드 */
static void task1_prepare(void) {
    uint32_t *sp = &stack1[STACK_WORDS] - 16;
    for (int i = 0; i < 8; i++) {
        sp[i] = 0; /* r4~r7, r8~r11 */
    }
    sp[8] = 0;                      /* r0 */
    sp[9] = 0;                      /* r1 */
    sp[10] = 0;                     /* r2 */
    sp[11] = 0;                     /* r3 */
    sp[12] = 0;                     /* r12 */
    sp[13] = 0xffffffffu;           /* lr: 작업 함수는 돌아오지 않는다 */
    sp[14] = (uint32_t) task1 & ~1u; /* pc: 예외 복귀 프레임의 pc는 bit 0이 0이어야 한다 */
    sp[15] = 1u << 24;              /* xPSR: Thumb */
    task_sp[1] = (uint32_t) sp;
}

static void move_main_to_psp(void) {
    uint32_t top = (uint32_t) &stack0[STACK_WORDS];
    __asm volatile(
        "msr psp, %0\n"
        "movs r1, #2\n"
        "msr control, r1\n"
        "isb\n" ::"r"(top)
        : "r1", "memory");
}

/* 스택을 바꾼 함수 안에서는 지역변수 위치가 옛 SP 기준이라 틀어진다.
   PSP로 옮긴 직후 이 함수를 불러 여기서만 일한다 */
__attribute__((noinline)) static void task0(void) {
    uint32_t local;
    uart_puts("task0 stack var at 0x");
    uart_puthex((uint32_t) &local, 8);
    uart_puts(", stack0 ");
    uart_puthex((uint32_t) stack0, 8);
    uart_puts("..");
    uart_puthex((uint32_t) &stack0[STACK_WORDS], 8);
    uart_puts(", stack1 ");
    uart_puthex((uint32_t) stack1, 8);
    uart_puts("..");
    uart_puthex((uint32_t) &stack1[STACK_WORDS], 8);
    uart_puts("\r\n");

    SHPR3 = (PRI_LOWEST << 30) | (PRI_LOWEST << 22);
    SYST_RVR = TICK_CYCLES - 1u;
    SYST_CVR = 0;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;

    /* 카운터는 각 작업만 쓰고, 여기서는 이전 값과의 차이로 센다 */
    uint32_t prev[2] = {0, 0};
    uint32_t prev_slices[2] = {0, 0};
    uint32_t next = time_us() + 1000000u;
    for (uint32_t report = 0; report < 5u;) {
        count[0]++;
        if ((int32_t) (time_us() - next) < 0) {
            continue;
        }
        next += 1000000u;
        report++;
        uint32_t c[2] = {count[0], count[1]};
        uint32_t s[2] = {slices[0], slices[1]};
        uart_puts("slices task0 ");
        uart_putdec(s[0] - prev_slices[0]);
        uart_puts(" task1 ");
        uart_putdec(s[1] - prev_slices[1]);
        uart_puts(", loops task0 ");
        uart_putdec(c[0] - prev[0]);
        uart_puts(" task1 ");
        uart_putdec(c[1] - prev[1]);
        uart_puts(", task1 stack var at 0x");
        uart_puthex(task1_sp_seen, 8);
        uart_puts("\r\n");
        for (int i = 0; i < 2; i++) {
            prev[i] = c[i];
            prev_slices[i] = s[i];
        }
    }
    SYST_CSR = 0;
    uart_puts("scheduler stopped\r\n");
    while (1) {
    }
}

int main(void) {
    board_init();
    task1_prepare();
    task_current = 0;
    move_main_to_psp();
    task0();
}
