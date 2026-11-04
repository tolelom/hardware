/*
 * HardFault를 일부러 내고 예외 진입 때 쌓인 레지스터를 UART로 보낸다.
 * M0+는 예외 진입 때 r0~r3, r12, lr, pc, xPSR 8워드를 스택에 쌓는다 (ARMv6-M).
 * 핸들러는 EXC_RETURN(lr) bit 2로 MSP/PSP를 고르고 그 스택 주소를 C 함수에 넘긴다.
 * 보고한 뒤 watchdog으로 재부팅해 다음 경우로 넘어간다. 순번은 SCRATCH0/1에 둔다.
 *   0 정렬 안 된 주소에서 ldr
 *   1 매핑 없는 주소 0x30000000 읽기
 *   2 Thumb 비트(bit 0)가 0인 주소로 blx
 *   3 MPU 영역 0(256바이트, AP 000 = 접근 금지)에 쓰기 (2.4.6, AP 의미는 ARMv6-M 규칙)
 */
#include "board.h"

#define WATCHDOG_CTRL REG(0x40058000u + 0x00u)
#define WATCHDOG_SCRATCH0 REG(0x40058000u + 0x0cu)
#define WATCHDOG_SCRATCH1 REG(0x40058000u + 0x10u)
#define WATCHDOG_CTRL_TRIGGER (1u << 31)
#define PSM_WDSEL REG(0x40010000u + 0x08u)

#define MPU_CTRL REG(0xe000ed94u)
#define MPU_RBAR REG(0xe000ed9cu)
#define MPU_RASR REG(0xe000eda0u)
#define MPU_CTRL_ENABLE (1u << 0)
#define MPU_CTRL_PRIVDEFENA (1u << 2)
#define MPU_RBAR_VALID (1u << 4)
#define MPU_RASR_XN (1u << 28)
#define MPU_RASR_SIZE_256 (7u << 1)
#define MPU_RASR_ENABLE (1u << 0)

#define CASES 4u

static const char *const names[CASES] = {
    "unaligned ldr", "unmapped read 0x30000000", "blx to even address",
    "write to MPU no-access region",
};

static uint32_t buffer[4];
static uint8_t guard[256] __attribute__((aligned(256)));
static volatile uint32_t sink;

static uint32_t current_case(void) {
    uint32_t n = WATCHDOG_SCRATCH0;
    return WATCHDOG_SCRATCH1 == ~n ? n : 0u;
}

static void set_case(uint32_t n) {
    WATCHDOG_SCRATCH0 = n;
    WATCHDOG_SCRATCH1 = ~n;
}

static void reboot(void) {
    uart_flush();
    PSM_WDSEL = 0x1fffcu;
    WATCHDOG_CTRL |= WATCHDOG_CTRL_TRIGGER;
    while (1) {
    }
}

void hardfault_report(const uint32_t *frame) {
    static const char *const regs[8] = {"r0", "r1", "r2", "r3",
                                        "r12", "lr", "pc", "xpsr"};
    uart_puts("  HardFault, frame at 0x");
    uart_puthex((uint32_t) frame, 8);
    uart_puts("\r\n ");
    for (int i = 0; i < 8; i++) {
        uart_puts(" ");
        uart_puts(regs[i]);
        uart_puts("=");
        uart_puthex(frame[i], 8);
    }
    uart_puts("\r\n");
    set_case(current_case() + 1u);
    reboot();
}

__attribute__((naked)) void isr_hardfault(void) {
    __asm volatile(
        "movs r0, #4\n"
        "mov r1, lr\n"
        "tst r0, r1\n"
        "beq 1f\n"
        "mrs r0, psp\n"
        "b 2f\n"
        "1:\n"
        "mrs r0, msp\n"
        "2:\n"
        "ldr r1, =hardfault_report\n"
        "bx r1\n");
}

__attribute__((noinline)) static void case_unaligned(void) {
    volatile uint32_t *p = (volatile uint32_t *) ((uint32_t) buffer + 1u);
    sink = *p;
}

__attribute__((noinline)) static void case_unmapped(void) {
    sink = REG(0x30000000u);
}

__attribute__((noinline)) static void target(void) {
    sink = 1;
}

__attribute__((noinline)) static void case_even_address(void) {
    void (*f)(void) = (void (*)(void)) ((uint32_t) target & ~1u);
    f();
}

__attribute__((noinline)) static void case_mpu(void) {
    MPU_RBAR = (uint32_t) guard | MPU_RBAR_VALID | 0u;
    MPU_RASR = MPU_RASR_XN | MPU_RASR_SIZE_256 | MPU_RASR_ENABLE; /* AP 000 */
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile("dsb\nisb" ::: "memory");
    ((volatile uint8_t *) guard)[16] = 0xaau;
}

int main(void) {
    board_init();
    uint32_t n = current_case();
    if (n >= CASES) {
        set_case(0);
        uart_puts("fault: done\r\n");
        while (1) {
        }
    }
    uart_puts("fault case ");
    uart_putdec(n);
    uart_puts(": ");
    uart_puts(names[n]);
    uart_puts("\r\n");
    switch (n) {
    case 0:
        case_unaligned();
        break;
    case 1:
        case_unmapped();
        break;
    case 2:
        case_even_address();
        break;
    default:
        case_mpu();
        break;
    }
    uart_puts("  no fault\r\n");
    set_case(n + 1u);
    reboot();
}
